#include "cogitor_iot/config_portal_service.hpp"
#include "cogitor_iot/settings.hpp"
#include "embed_core/device_settings.hpp"
#include "embed_core/firmware_slot.hpp"
#include "embed_core/wifi_service.hpp"

#include "embed/registry.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace cogitor::iot {

static const char* TAG = "ConfigPortal";

struct ConfigPortalService::Impl {
    httpd_handle_t server = nullptr;
};

namespace {

int hexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool urlDecode(const char* in, size_t inLen, char* out, size_t outLen)
{
    if (!out || outLen == 0) {
        return false;
    }
    size_t o = 0;
    for (size_t i = 0; i < inLen && o + 1 < outLen; ++i) {
        if (in[i] == '+') {
            out[o++] = ' ';
        } else if (in[i] == '%' && i + 2 < inLen) {
            const int hi = hexVal(in[i + 1]);
            const int lo = hexVal(in[i + 2]);
            if (hi < 0 || lo < 0) {
                return false;
            }
            out[o++] = static_cast<char>((hi << 4) | lo);
            i += 2;
        } else {
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
    return true;
}

bool formGet(const char* body, const char* key, char* out, size_t outLen)
{
    if (!body || !key || !out || outLen == 0) {
        return false;
    }
    out[0] = '\0';
    const size_t keyLen = std::strlen(key);
    const char* p = body;
    while (p && *p) {
        const char* amp = std::strchr(p, '&');
        const size_t pairLen = amp ? static_cast<size_t>(amp - p) : std::strlen(p);
        const char* eq = static_cast<const char*>(std::memchr(p, '=', pairLen));
        if (eq && static_cast<size_t>(eq - p) == keyLen &&
            std::strncmp(p, key, keyLen) == 0) {
            const char* val = eq + 1;
            const size_t valLen = pairLen - keyLen - 1;
            return urlDecode(val, valLen, out, outLen);
        }
        p = amp ? amp + 1 : nullptr;
    }
    return false;
}

void htmlEscape(std::string& out, const char* s)
{
    for (; s && *s; ++s) {
        switch (*s) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += *s; break;
        }
    }
}

void appendBanner(std::string& html, const char* query)
{
    const char* msg = nullptr;
    if (query) {
        if (std::strstr(query, "saved=1")) {
            msg = "Saved. Rebooting…";
        } else if (std::strstr(query, "restored=1")) {
            msg = "Backup restored. Rebooting…";
        } else if (std::strstr(query, "reset=1")) {
            msg = "Credentials wiped (backup kept). Rebooting…";
        } else if (std::strstr(query, "rollback=1")) {
            msg = "Booting previous firmware…";
        } else if (std::strstr(query, "imported=1")) {
            msg = "Credentials imported. Rebooting…";
        } else if (std::strstr(query, "err=nobak")) {
            msg = "No backup to restore.";
        } else if (std::strstr(query, "err=noslot")) {
            msg = "No previous firmware in the other OTA slot.";
        } else if (std::strstr(query, "err=badjson")) {
            msg = "Invalid credentials JSON (need wifi.ssid + cogitor product/device/host/token).";
        }
    }
    if (!msg) {
        return;
    }
    html += "<p class=banner>";
    htmlEscape(html, msg);
    html += "</p>";
}

std::string buildIndexHtml(const char* query)
{
    embed::WifiSettings wifi{};
    CogitorSettings cogitor{};
    if (!embed::loadWifiSettings(wifi))
    {
        ESP_LOGW(TAG, "no wifi settings found");
    }
    if (!loadSettings(cogitor)) {
        ESP_LOGW(TAG, "no cogitor settings found");
    }

    embed::WifiSettings bakW{};
    CogitorSettings bakC{};
    const bool haveBak = embed::loadWifiBackup(bakW) || loadSettingsBackup(bakC);

    std::string html;
    html.reserve(6500);
    html +=
        "<!doctype html><html><head><meta charset=utf-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>embed config</title><style>"
        "body{font-family:sans-serif;max-width:28rem;margin:1.2rem auto;padding:0 1rem;"
        "background:#111;color:#eee}"
        "h1{font-size:1.2rem}h2{font-size:1rem}label{display:block;margin:.7rem 0 .2rem;color:#bbb}"
        "input,button,textarea{width:100%;box-sizing:border-box;padding:.55rem;border-radius:6px;"
        "border:1px solid #444;background:#1c1c1c;color:#eee}"
        "textarea{min-height:8rem;font-family:monospace;font-size:.8rem}"
        "input[type=file]{padding:.3rem}input[type=checkbox]{width:auto}"
        "button{margin-top:1rem;background:#0a7;border:0;"
        "font-weight:600;cursor:pointer}button.danger{background:#a33}"
        "button.secondary{background:#246}"
        ".row{display:flex;gap:.8rem;align-items:center}small{color:#888}"
        ".banner{background:#143;border:1px solid #2a6;padding:.6rem .8rem;border-radius:6px}"
        ".bak{background:#1a1a22;padding:.8rem;border-radius:8px;margin-top:1.2rem}"
        "</style></head><body><h1>Device setup</h1>";
    appendBanner(html, query);
    html +=
        "<p><small>WiFi + Cogitor MQTT. Saved to NVS <code>fctry</code>.</small></p>"
        "<form method=post action=/save>"
        "<h2>WiFi</h2>"
        "<label>SSID</label><input name=ssid required value=\"";
    htmlEscape(html, wifi.ssid);
    html +=
        "\"><label>Password</label><input name=pass type=password placeholder=\"(unchanged if empty)\">"
        "<h2>Cogitor MQTT</h2>"
        "<label>Product ID</label><input name=product required value=\"";
    htmlEscape(html, cogitor.product);
    html += "\"><label>Device ID</label><input name=device required value=\"";
    htmlEscape(html, cogitor.device);
    html += "\"><label>Broker host</label><input name=host required value=\"";
    htmlEscape(html, cogitor.host);
    html += "\"><label>Port (0 = default)</label><input name=port type=number min=0 max=65535 value=\"";
    html += std::to_string(cogitor.port);
    html += "\"><label>Access token</label><input name=token type=password placeholder=\"";
    html += cogitor.token[0] ? "(unchanged if empty)" : "required";
    html += "\"><div class=row><input id=tls type=checkbox name=tls";
    if (cogitor.useTls) html += " checked";
    html += "><label for=tls>TLS (mqtts)</label></div>"
            "<div class=row><input id=short type=checkbox name=short";
    if (cogitor.topicShort) html += " checked";
    html += "><label for=short>Short topics (v1/…)</label></div>"
            "<button type=submit>Save &amp; reboot</button></form>";

    if (haveBak) {
        html += "<div class=bak><h2>Backup</h2><p><small>";
        if (bakW.ssid[0]) {
            html += "WiFi ";
            htmlEscape(html, bakW.ssid);
            html += " · ";
        }
        if (bakC.product[0] || bakC.device[0]) {
            htmlEscape(html, bakC.product);
            html += ".";
            htmlEscape(html, bakC.device);
            if (bakC.host[0]) {
                html += " @ ";
                htmlEscape(html, bakC.host);
            }
        }
        html +=
            "</small></p>"
            "<form method=post action=/restore "
            "onsubmit=\"return confirm('Restore previous WiFi + MQTT settings?');\">"
            "<button class=secondary type=submit>Restore backup &amp; reboot</button></form>"
            "</div>";
    } else {
        html += "<p><small>No backup yet — it is created on Save, Import, or Factory reset.</small></p>";
    }

    html +=
        "<div class=bak><h2>JSON credentials</h2>"
        "<p><small>Import WiFi + Cogitor MQTT from a file or paste. Backup is taken first. "
        "<a href=/credentials.json download=credentials.json style=color:#8cf>Download current JSON</a>"
        "</small></p>"
        "<form method=post action=/import "
        "onsubmit=\"event.preventDefault();var j=this.json.value.trim();"
        "if(!j){alert('Paste or pick a JSON file');return false;}"
        "if(!confirm('Import credentials from JSON and reboot?'))return false;"
        "fetch('/import',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:j,redirect:'manual'}).then(function(r){"
        "var loc=r.headers.get('Location');location.href=loc||'/?imported=1';});return false;\">"
        "<textarea name=json placeholder='{\"wifi\":{\"ssid\":\"…\",\"password\":\"…\"},"
        "\"cogitor\":{\"product\":\"…\",\"device\":\"…\",\"host\":\"…\",\"token\":\"…\","
        "\"port\":0,\"tls\":false,\"topic_short\":true}}'></textarea>"
        "<label>Or pick a .json file</label>"
        "<input type=file accept=\"application/json,.json\" "
        "onchange=\"var f=this.files[0];if(!f)return;f.text().then(function(t){this.form.json.value=t;}.bind(this))\">"
        "<button class=secondary type=submit>Import JSON &amp; reboot</button></form></div>";

    embed::FirmwareSlotInfo runFw{};
    embed::FirmwareSlotInfo prevFw{};
    embed::loadFirmwareSlots(runFw, prevFw);
    html += "<div class=bak><h2>Firmware</h2><p><small>Running ";
    htmlEscape(html, runFw.valid ? runFw.version : "?");
    html += " (";
    htmlEscape(html, runFw.label[0] ? runFw.label : "?");
    html += ")";
    if (prevFw.valid) {
        html += " · previous ";
        htmlEscape(html, prevFw.version);
        html += " (";
        htmlEscape(html, prevFw.label);
        html += ")</small></p>"
                "<form method=post action=/ota_rollback "
                "onsubmit=\"return confirm('Boot previous firmware?');\">"
                "<button class=secondary type=submit>Rollback firmware &amp; reboot</button></form>";
    } else {
        html += " · no previous OTA image</small></p>";
    }
    html += "</div>";

    html +=
        "<form method=post action=/reset "
        "onsubmit=\"return confirm('Wipe active WiFi + token? Backup is kept.');\">"
        "<button class=danger type=submit>Factory reset credentials</button></form>"
        "</body></html>";
    return html;
}

esp_err_t sendHtml(httpd_req_t* req, const char* html)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, html);
}

esp_err_t redirectRoot(httpd_req_t* req, const char* query)
{
    char loc[32] = "/";
    if (query && query[0]) {
        std::snprintf(loc, sizeof(loc), "/?%s", query);
    }
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", loc);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, nullptr, 0);
}

esp_err_t handleIndex(httpd_req_t* req)
{
    char query[64]{};
    httpd_req_get_url_query_str(req, query, sizeof(query));
    const std::string html = buildIndexHtml(query[0] ? query : nullptr);
    return sendHtml(req, html.c_str());
}

esp_err_t handleCaptive(httpd_req_t* req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, nullptr, 0);
}

esp_err_t readBody(httpd_req_t* req, std::string& body)
{
    const size_t len = req->content_len;
    if (len == 0 || len > 4096) {
        return ESP_ERR_INVALID_SIZE;
    }
    body.resize(len);
    int recvd = 0;
    while (recvd < static_cast<int>(len)) {
        const int n = httpd_req_recv(req, body.data() + recvd, len - static_cast<size_t>(recvd));
        if (n <= 0) {
            return ESP_FAIL;
        }
        recvd += n;
    }
    return ESP_OK;
}

esp_err_t handleSave(httpd_req_t* req)
{
    std::string body;
    if (readBody(req, body) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "bad form");
    }

    embed::WifiSettings wifi{};
    CogitorSettings cogitor{};
    if (!embed::loadWifiSettings(wifi))
    {
        ESP_LOGW(TAG, "no wifi settings found");
    }
    if (!loadSettings(cogitor)) {
        ESP_LOGW(TAG, "no cogitor settings found");
    }

    char buf[256]{};
    if (!formGet(body.c_str(), "ssid", buf, sizeof(buf)) || buf[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "ssid required");
    }
    std::strncpy(wifi.ssid, buf, sizeof(wifi.ssid) - 1);

    if (formGet(body.c_str(), "pass", buf, sizeof(buf)) && buf[0] != '\0') {
        std::strncpy(wifi.password, buf, sizeof(wifi.password) - 1);
    }

    if (!formGet(body.c_str(), "product", buf, sizeof(buf)) || buf[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "product required");
    }
    std::strncpy(cogitor.product, buf, sizeof(cogitor.product) - 1);

    if (!formGet(body.c_str(), "device", buf, sizeof(buf)) || buf[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "device required");
    }
    std::strncpy(cogitor.device, buf, sizeof(cogitor.device) - 1);

    if (!formGet(body.c_str(), "host", buf, sizeof(buf)) || buf[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "host required");
    }
    std::strncpy(cogitor.host, buf, sizeof(cogitor.host) - 1);

    if (formGet(body.c_str(), "port", buf, sizeof(buf))) {
        cogitor.port = static_cast<uint16_t>(std::atoi(buf));
    }

    if (formGet(body.c_str(), "token", buf, sizeof(buf)) && buf[0] != '\0') {
        std::strncpy(cogitor.token, buf, sizeof(cogitor.token) - 1);
    }
    if (cogitor.token[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "token required");
    }

    cogitor.useTls = formGet(body.c_str(), "tls", buf, sizeof(buf));
    cogitor.topicShort = formGet(body.c_str(), "short", buf, sizeof(buf));

    const esp_err_t bak = backupDeviceSettings();
    if (bak != ESP_OK && bak != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "backup before save failed: %s", esp_err_to_name(bak));
    }

    esp_err_t err = embed::saveWifiSettings(wifi);
    if (err == ESP_OK) {
        err = saveSettings(cogitor);
    }
    if (err == ESP_OK) {
        err = embed::setConfigPortalRequested(false);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "save failed");
    }

    ESP_LOGI(TAG, "saved wifi=%s cogitor=%s.%s @ %s — reboot",
             wifi.ssid, cogitor.product, cogitor.device, cogitor.host);
    embed::scheduleReboot(1500);
    return redirectRoot(req, "saved=1");
}

esp_err_t handleImport(httpd_req_t* req)
{
    std::string body;
    if (readBody(req, body) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "bad body");
    }

    char jsonBuf[4096]{};
    const char* json = body.c_str();
    if (formGet(body.c_str(), "json", jsonBuf, sizeof(jsonBuf)) && jsonBuf[0]) {
        json = jsonBuf;
    }

    const esp_err_t err = importCredentialsJson(json);
    if (err == ESP_ERR_INVALID_ARG) {
        return redirectRoot(req, "err=badjson");
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "import failed: %s", esp_err_to_name(err));
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "import failed");
    }
    embed::scheduleReboot(1500);
    return redirectRoot(req, "imported=1");
}

esp_err_t handleExport(httpd_req_t* req)
{
    char buf[2048]{};
    const esp_err_t err = exportCredentialsJson(buf, sizeof(buf), true);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "export failed");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"credentials.json\"");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, buf);
}

esp_err_t handleRestore(httpd_req_t* req)
{
    const esp_err_t err = restoreDeviceSettingsBackup();
    if (err == ESP_ERR_NOT_FOUND) {
        return redirectRoot(req, "err=nobak");
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "restore failed: %s", esp_err_to_name(err));
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "restore failed");
    }
    embed::scheduleReboot(1500);
    return redirectRoot(req, "restored=1");
}

esp_err_t handleOtaRollback(httpd_req_t* req)
{
    const esp_err_t err = embed::rollbackFirmware();
    if (err == ESP_ERR_NOT_FOUND) {
        return redirectRoot(req, "err=noslot");
    }
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "rollback failed");
    }
    embed::scheduleReboot(1500);
    return redirectRoot(req, "rollback=1");
}

esp_err_t handleReset(httpd_req_t* req)
{
    const esp_err_t err = factoryResetSettings();
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "reset failed");
    }
    embed::scheduleReboot(1500);
    return redirectRoot(req, "reset=1");
}

} // namespace

ConfigPortalService::ConfigPortalService() : impl_(std::make_unique<Impl>()) {}

ConfigPortalService::~ConfigPortalService()
{
    stop();
}

void ConfigPortalService::start()
{
    auto* wifi = embed::ServiceRegistry::instance().getService<embed::WifiService>();
    const bool ap = wifi && wifi->softApEnabled();
#if !defined(CONFIG_EMBED_CONFIG_HTTP_STA) || CONFIG_EMBED_CONFIG_HTTP_STA
    const bool staHttp = true;
#else
    const bool staHttp = false;
#endif
    if (!ap && !staHttp) {
        ESP_LOGI(TAG, "HTTP config disabled (not portal, STA HTTP off)");
        return;
    }

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
#ifdef CONFIG_EMBED_CONFIG_HTTP_PORT
    cfg.server_port = CONFIG_EMBED_CONFIG_HTTP_PORT;
#endif
    cfg.stack_size = 8192;
    cfg.max_uri_handlers = 12;
    cfg.lru_purge_enable = true;

    if (httpd_start(&impl_->server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed");
        impl_->server = nullptr;
        return;
    }

    const httpd_uri_t indexUri{.uri = "/", .method = HTTP_GET, .handler = handleIndex, .user_ctx = nullptr};
    const httpd_uri_t saveUri{.uri = "/save", .method = HTTP_POST, .handler = handleSave, .user_ctx = nullptr};
    const httpd_uri_t importUri{.uri = "/import", .method = HTTP_POST, .handler = handleImport, .user_ctx = nullptr};
    const httpd_uri_t exportUri{.uri = "/credentials.json", .method = HTTP_GET, .handler = handleExport, .user_ctx = nullptr};
    const httpd_uri_t restoreUri{.uri = "/restore", .method = HTTP_POST, .handler = handleRestore, .user_ctx = nullptr};
    const httpd_uri_t otaRollbackUri{.uri = "/ota_rollback", .method = HTTP_POST, .handler = handleOtaRollback, .user_ctx = nullptr};
    const httpd_uri_t resetUri{.uri = "/reset", .method = HTTP_POST, .handler = handleReset, .user_ctx = nullptr};
    const httpd_uri_t gen204{.uri = "/generate_204", .method = HTTP_GET, .handler = handleCaptive, .user_ctx = nullptr};
    const httpd_uri_t hotspot{.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = handleCaptive, .user_ctx = nullptr};
    const httpd_uri_t ncsi{.uri = "/ncsi.txt", .method = HTTP_GET, .handler = handleCaptive, .user_ctx = nullptr};

    httpd_register_uri_handler(impl_->server, &indexUri);
    httpd_register_uri_handler(impl_->server, &saveUri);
    httpd_register_uri_handler(impl_->server, &importUri);
    httpd_register_uri_handler(impl_->server, &exportUri);
    httpd_register_uri_handler(impl_->server, &restoreUri);
    httpd_register_uri_handler(impl_->server, &otaRollbackUri);
    httpd_register_uri_handler(impl_->server, &resetUri);
    httpd_register_uri_handler(impl_->server, &gen204);
    httpd_register_uri_handler(impl_->server, &hotspot);
    httpd_register_uri_handler(impl_->server, &ncsi);

    ESP_LOGI(TAG, "config HTTP on :%d (%s)", cfg.server_port, ap ? "SoftAP" : "STA");
}

void ConfigPortalService::stop()
{
    if (!impl_ || !impl_->server) {
        return;
    }
    httpd_stop(impl_->server);
    impl_->server = nullptr;
}

} // namespace cogitor::iot
