#include "cogitor_iot/log_service.hpp"

#include "embed/registry.hpp"
#include "esp_log.h"
#include "esp_log_write.h"
#include "esp_timer.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sys/time.h>

namespace cogitor::iot {

static const char* TAG = "LogSvc";

// Global pointer — accessed from the static vprintf callback.
static LogService* g_logService = nullptr;

thread_local bool LogService::publishing_ = false;

static int64_t epochMs()
{
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

const char* LogService::levelName(int level)
{
    switch (level) {
    case 0: return "ERROR";
    case 1: return "WARN";
    case 2: return "INFO";
    case 3: return "DEBUG";
    case 4: return "VERBOSE";
    default: return "INFO";
    }
}

// Parse ESP-IDF v6 log format:
//   "\e[<codes>m<LEVEL> (<timestamp>) <tag>: <message>\e[0m\n"
// LEVEL = single letter: E/W/I/D/V
// timestamp = uint32 (RTOS ticks) or string (system time)
// tag = module name
// message = user log text
static void parseLogLine(const char* fmt, va_list ap,
                         int& outLevel, const char*& outTag,
                         char* msgBuf, int msgBufSize)
{
    outLevel = 2; // default INFO
    outTag = "";

    // Format the full line into a temp buffer so we can parse it.
    char line[512];
    const int len = vsnprintf(line, sizeof(line), fmt, ap);
    if (len <= 0) return;

    const char* p = line;

    // Skip ANSI escape sequences: \e[...m
    while (*p == '\x1b') {
        while (*p && *p != 'm') p++;
        if (*p == 'm') p++;
    }

    // Level letter
    if (*p == 'E') { outLevel = 0; p++; }
    else if (*p == 'W') { outLevel = 1; p++; }
    else if (*p == 'I') { outLevel = 2; p++; }
    else if (*p == 'D') { outLevel = 3; p++; }
    else if (*p == 'V') { outLevel = 4; p++; }

    // Skip " (<timestamp>) "
    const char* openParen = strchr(p, '(');
    const char* closeParen = openParen ? strchr(openParen, ')') : nullptr;
    if (closeParen) {
        p = closeParen + 1;
        // Skip space after ')'
        while (*p == ' ') p++;
    }

    // Tag: everything up to ": "
    const char* colonPos = strstr(p, ": ");
    if (colonPos) {
        // We cannot return a pointer into a stack buffer for the tag.
        // The caller will use "unknown" when tag is empty.
        // For the tag we need a static/thread_local buffer.
        static thread_local char tagBuf[64];
        const int tagLen = static_cast<int>(colonPos - p);
        const int copyLen = tagLen < (int)sizeof(tagBuf) - 1 ? tagLen : (int)sizeof(tagBuf) - 1;
        memcpy(tagBuf, p, copyLen);
        tagBuf[copyLen] = '\0';
        outTag = tagBuf;
        p = colonPos + 2;
    }

    // Message: strip trailing \e[0m\n
    const char* msgStart = p;
    int msgLen = static_cast<int>(strlen(msgStart));
    // Strip trailing ESC[0m and newline
    while (msgLen > 0 && (msgStart[msgLen - 1] == '\n' || msgStart[msgLen - 1] == '\r')) {
        msgLen--;
    }
    if (msgLen >= 4 && msgStart[msgLen - 3] == 'm' &&
        msgStart[msgLen - 4] == '\x1b') {
        // Check for longer escape like \e[0m
        int escStart = msgLen - 4;
        while (escStart > 0 && msgStart[escStart - 1] != '\x1b') escStart--;
        if (escStart > 0 && msgStart[escStart - 1] == '\x1b') {
            msgLen = escStart - 1;
        }
    }

    const int copyLen = msgLen < msgBufSize - 1 ? msgLen : msgBufSize - 1;
    memcpy(msgBuf, msgStart, copyLen);
    msgBuf[copyLen] = '\0';
}

int LogService::logVprintf(const char* fmt, va_list ap)
{
    // Always forward to original console output first.
    int ret = 0;
    if (g_logService && g_logService->originalVprintf_) {
        va_list apCopy;
        va_copy(apCopy, ap);
        ret = g_logService->originalVprintf_(fmt, apCopy);
        va_end(apCopy);
    } else {
        va_list apCopy;
        va_copy(apCopy, ap);
        ret = vprintf(fmt, apCopy);
        va_end(apCopy);
    }

    // Capture for MQTT forwarding.
    if (g_logService && !g_logService->publishing_ && g_logService->iot_) {
        int level = 2;
        const char* tag = "";
        char msg[384];

        va_list ap2;
        va_copy(ap2, ap);
        parseLogLine(fmt, ap2, level, tag, msg, sizeof(msg));
        va_end(ap2);

        if (level <= g_logService->minLevel_ && msg[0] != '\0') {
            g_logService->appendEntry(level, tag, msg);
        }
    }

    return ret;
}

void LogService::start()
{
    auto& reg = embed::ServiceRegistry::instance();
    iot_ = reg.getService<IotService>();
    if (!iot_) {
        ESP_LOGE(TAG, "IotService not found");
        return;
    }

    g_logService = this;
    originalVprintf_ = esp_log_set_vprintf(logVprintf);

    const esp_timer_create_args_t args = {
        .callback = flushCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "log_flush",
        .skip_unhandled_events = true
    };
    esp_timer_create(&args, &flushTimer_);
    esp_timer_start_periodic(flushTimer_, kFlushIntervalUs);

    ESP_LOGI(TAG, "Started — forwarding logs to MQTT (min level=%d)", minLevel_);
}

void LogService::stop()
{
    if (flushTimer_) {
        esp_timer_stop(flushTimer_);
        esp_timer_delete(flushTimer_);
        flushTimer_ = nullptr;
    }

    // Flush remaining entries before restoring original vprintf.
    flush();

    if (originalVprintf_) {
        esp_log_set_vprintf(originalVprintf_);
        originalVprintf_ = nullptr;
    }
    g_logService = nullptr;
    iot_ = nullptr;
}

void LogService::appendEntry(int level, const char* tag, const char* msg)
{
    const int64_t ts = epochMs();

    // Build a single JSON object: {"ts":...,"level":"...","module":"...","message":"..."}
    char entry[512];
    const int n = snprintf(entry, sizeof(entry),
        "{\"ts\":%lld,\"level\":\"%s\",\"module\":\"%s\",\"message\":\"%s\"}",
        static_cast<long long>(ts),
        levelName(level),
        tag[0] ? tag : "app",
        msg);
    if (n <= 0 || n >= (int)sizeof(entry)) return;

    // Flush if buffer grew too large (heuristic: 1536 bytes).
    if (batchBuf_.size() + n + 3 > 1536) {
        flush();
    }

    if (!batchBuf_.empty()) {
        batchBuf_ += ',';
    } else {
        batchBuf_ = '[';
    }
    batchBuf_.append(entry, n);
    entryCount_++;
}

void LogService::flush()
{
    if (batchBuf_.empty() || !iot_) return;

    // Close the JSON array.
    batchBuf_ += ']';

    publishing_ = true;
    iot_->publishLogs(batchBuf_, 0);
    publishing_ = false;

    batchBuf_.clear();
    entryCount_ = 0;
}

void LogService::flushCallback(void* arg)
{
    auto* self = static_cast<LogService*>(arg);
    self->flush();
}

} // namespace cogitor::iot
