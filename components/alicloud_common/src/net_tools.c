//
// Created by Ivan Kishchenko on 16/2/26.
//

#include "net_tools.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <esp_log.h>

time_t timegm(struct tm *tm) {
    time_t ret;
    char *tz;

    tz = getenv("TZ");
    putenv("TZ=");
    tzset();
    ret = mktime(tm);
    if (tz) {
        char env[256];
        snprintf(env, sizeof(env), "TZ=%s", tz);
        putenv(env);
    } else {
        unsetenv("TZ");
    }
    tzset();
    return ret;
}

#define INITIAL_CAPACITY 8

// C23: static_assert for compile-time checks
static_assert(INITIAL_CAPACITY > 0, "INITIAL_CAPACITY must be positive");
static_assert(sizeof(size_t) >= sizeof(int), "size_t must be at least as large as int");

// ============================================================================
// HTTP Method Functions
// ============================================================================

// Convert HTTP method enum to string
const char* http_method_to_string(http_method_t method) {
    switch (method) {
        case E_HTTP_METHOD_GET:     return "GET";
        case E_HTTP_METHOD_POST:    return "POST";
        case E_HTTP_METHOD_PUT:     return "PUT";
        case E_HTTP_METHOD_DELETE:  return "DELETE";
        case E_HTTP_METHOD_PATCH:   return "PATCH";
        case E_HTTP_METHOD_HEAD:    return "HEAD";
        case E_HTTP_METHOD_OPTIONS: return "OPTIONS";
        case E_HTTP_METHOD_CONNECT: return "CONNECT";
        case E_HTTP_METHOD_TRACE:   return "TRACE";
        default:                  return nullptr;
    }
}

// Parse HTTP method string to enum (returns HTTP_METHOD_GET if not found)
http_method_t http_method_from_string(const char* method_str) {
    if (!method_str) return E_HTTP_METHOD_GET;

    if (strcasecmp(method_str, "GET") == 0) {
        return E_HTTP_METHOD_GET;
    } else if (strcasecmp(method_str, "POST") == 0) {
        return E_HTTP_METHOD_POST;
    } else if (strcasecmp(method_str, "PUT") == 0) {
        return E_HTTP_METHOD_PUT;
    } else if (strcasecmp(method_str, "DELETE") == 0) {
        return E_HTTP_METHOD_DELETE;
    } else if (strcasecmp(method_str, "PATCH") == 0) {
        return E_HTTP_METHOD_PATCH;
    } else if (strcasecmp(method_str, "HEAD") == 0) {
        return E_HTTP_METHOD_HEAD;
    } else if (strcasecmp(method_str, "OPTIONS") == 0) {
        return E_HTTP_METHOD_OPTIONS;
    } else if (strcasecmp(method_str, "CONNECT") == 0) {
        return E_HTTP_METHOD_CONNECT;
    } else if (strcasecmp(method_str, "TRACE") == 0) {
        return E_HTTP_METHOD_TRACE;
    }

    return E_HTTP_METHOD_GET; // Default
}

// Helper function to convert time_t to ISO8601 string
static char* time_to_iso8601(time_t time)
{
    struct tm* tm_info = gmtime(&time);
    if (!tm_info) return nullptr;

    char* buffer = malloc(25); // "YYYY-MM-DDTHH:MM:SSZ" + null terminator
    if (!buffer) return nullptr;

    strftime(buffer, 25, "%Y-%m-%dT%H:%M:%SZ", tm_info);
    return buffer;
}

// Helper function to parse ISO8601 string to time_t
static esp_err_t iso8601_to_time(const char* iso8601, time_t* out_time)
{
    if (!iso8601 || !out_time) return ESP_ERR_INVALID_ARG;

    struct tm tm_info = {0};

    // Parse ISO8601 format: YYYY-MM-DDTHH:MM:SSZ
    if (sscanf(iso8601, "%d-%d-%dT%d:%d:%dZ",
               &tm_info.tm_year, &tm_info.tm_mon, &tm_info.tm_mday,
               &tm_info.tm_hour, &tm_info.tm_min, &tm_info.tm_sec) != 6) {
        return ESP_FAIL; // Parse failed
    }

    // Adjust values for struct tm
    tm_info.tm_year -= 1900; // Years since 1900
    tm_info.tm_mon -= 1;     // Months since January (0-11)
    tm_info.tm_isdst = 0;    // Not daylight saving time

    // Convert to time_t (UTC)
    *out_time = timegm(&tm_info);
    if (*out_time == -1) {
        return ESP_FAIL; // Conversion failed
    }

    return ESP_OK;
}

// Helper function to resize dynamic array
// Helper function to ensure capacity for dynamic arrays
static esp_err_t ensure_capacity(void** array, size_t* capacity, size_t count, size_t element_size)
{
    if (count < *capacity) {
        return ESP_OK; // No resize needed
    }
    size_t new_capacity = *capacity * 2;
    void* new_array = realloc(*array, new_capacity * element_size);
    if (!new_array) {
        return ESP_ERR_NO_MEM;
    }
    *array = new_array;
    *capacity = new_capacity;
    return ESP_OK;
}

// ============================================================================
// Helper Functions
// ============================================================================

// Base64 encoding table
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Helper function to encode data to Base64
static char* base64_encode(const char* data, size_t len) {
    if (!data || len == 0) return nullptr;

    // Calculate required buffer size
    size_t encoded_len = ((len + 2) / 3) * 4;
    char* result = malloc(encoded_len + 1);
    if (!result) return nullptr;

    const unsigned char* input = (const unsigned char*)data;
    char* output = result;

    for (size_t i = 0; i < len; i += 3) {
        uint32_t triple = (input[i] << 16) |
                         (i + 1 < len ? input[i + 1] << 8 : 0) |
                         (i + 2 < len ? input[i + 2] : 0);

        *output++ = base64_table[(triple >> 18) & 0x3F];
        *output++ = base64_table[(triple >> 12) & 0x3F];
        *output++ = (i + 1 < len) ? base64_table[(triple >> 6) & 0x3F] : '=';
        *output++ = (i + 2 < len) ? base64_table[triple & 0x3F] : '=';
    }

    *output = '\0';
    return result;
}

// Helper function to URL encode a string
static char* url_encode_string(const char* str) {
    if (!str) return nullptr;

    // Calculate required buffer size
    size_t len = strlen(str);
    size_t encoded_len = 0;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = str[i];
        // Allowed: A-Z a-z 0-9 - _ . ~
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded_len += 1;
        } else {
            encoded_len += 3; // %XX
        }
    }

    char* result = malloc(encoded_len + 1);
    if (!result) return nullptr;

    char* ptr = result;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = str[i];
        // Allowed: A-Z a-z 0-9 - _ . ~
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            *ptr++ = c;
        } else {
            // Encode as %XX
            *ptr++ = '%';
            *ptr++ = "0123456789ABCDEF"[c >> 4];
            *ptr++ = "0123456789ABCDEF"[c & 0x0F];
        }
    }

    *ptr = '\0';
    return result;
}

char* get_host_from_url(const char* url)
{
    if (!url) return nullptr;

    const char* host_start = url;

    // Skip protocol
    if (strstr(url, "://")) {
        host_start = strstr(url, "://") + 3;
    }

    // Find end of host
    const char* host_end = host_start;
    while (*host_end && *host_end != ':' && *host_end != '/' && *host_end != '?') {
        host_end++;
    }

    // Copy host
    size_t host_len = host_end - host_start;
    if (host_len == 0) return nullptr;

    char* host = strndup(host_start, host_len);
    if (!host) return nullptr;

    return host;
}

// Initialize query parameters list
query_params_t* query_params_init(void)
{
    query_params_t* qp = malloc(sizeof(query_params_t));
    if (!qp) return nullptr;

    qp->params = malloc(INITIAL_CAPACITY * sizeof(query_param_t));
    if (!qp->params) {
        free(qp);
        return nullptr;
    }

    qp->count = 0;
    qp->capacity = INITIAL_CAPACITY;
    return qp;
}

// Add a parameter to the list
esp_err_t query_params_add(query_params_t* qp, const char* name, const char* value)
{
    if (!qp || !name) return ESP_ERR_INVALID_ARG;

    // Ensure capacity
    esp_err_t err = ensure_capacity((void**)&qp->params, &qp->capacity, qp->count, sizeof(query_param_t));
    if (err != ESP_OK) {
        return err;
    }

    qp->params[qp->count].name = strdup(name);
    qp->params[qp->count].value = value ? strdup(value) : nullptr;

    if (!qp->params[qp->count].name || (value && !qp->params[qp->count].value)) {
        free(qp->params[qp->count].name);
        free(qp->params[qp->count].value);
        return ESP_ERR_NO_MEM;
    }

    qp->count++;
    return ESP_OK;
}

// Set a parameter value (replaces existing value if parameter exists)
esp_err_t query_params_set(query_params_t* qp, const char* name, const char* value)
{
    if (!qp || !name) return ESP_ERR_INVALID_ARG;

    // Try to find existing parameter
    for (size_t i = 0; i < qp->count; i++) {
        if (strcmp(qp->params[i].name, name) == 0) {
            // Found existing parameter, replace value
            free(qp->params[i].value);
            qp->params[i].value = value ? strdup(value) : nullptr;

            if (value && !qp->params[i].value) {
                return ESP_ERR_NO_MEM;
            }

            return ESP_OK;
        }
    }

    // Parameter not found, add new one
    return query_params_add(qp, name, value);
}

// Add a parameter with formatted value
esp_err_t query_params_add_fmt(query_params_t* qp, const char* name, const char* fmt, ...)
{
    if (!qp || !name || !fmt) return ESP_ERR_INVALID_ARG;

    va_list args;
    va_start(args, fmt);

    // Calculate required buffer size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);
        return ESP_FAIL;
    }

    // Allocate buffer and format string
    char* value = malloc(size + 1);
    if (!value) {
        va_end(args);
        return ESP_ERR_NO_MEM;
    }

    vsnprintf(value, size + 1, fmt, args);
    va_end(args);

    // Add parameter
    esp_err_t result = query_params_add(qp, name, value);
    free(value);

    return result;
}

// Add a parameter with integer value
esp_err_t query_params_add_int(query_params_t* qp, const char* name, int value)
{
    if (!qp || !name) return ESP_ERR_INVALID_ARG;

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);

    return query_params_add(qp, name, buffer);
}

// Add a parameter with float value
esp_err_t query_params_add_float(query_params_t* qp, const char* name, float value)
{
    if (!qp || !name) return ESP_ERR_INVALID_ARG;

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%g", value);

    return query_params_add(qp, name, buffer);
}

// Add a parameter with time_t value (stored as ISO8601)
esp_err_t query_params_add_time(query_params_t* qp, const char* name, time_t time)
{
    if (!qp || !name) return ESP_ERR_INVALID_ARG;

    char* iso8601 = time_to_iso8601(time);
    if (!iso8601) return ESP_ERR_NO_MEM;

    esp_err_t result = query_params_add(qp, name, iso8601);
    free(iso8601);

    return result;
}

// Get parameter value as string
const char* query_params_get(const query_params_t* qp, const char* name)
{
    if (!qp || !name) return nullptr;

    for (size_t i = 0; i < qp->count; i++) {
        if (strcmp(qp->params[i].name, name) == 0) {
            return qp->params[i].value;
        }
    }

    return nullptr;
}

// Get parameter value as integer
esp_err_t query_params_get_int(const query_params_t* qp, const char* name, int* out_value)
{
    const char* value = query_params_get(qp, name);
    if (!value || !out_value) return ESP_ERR_INVALID_ARG;

    char* endptr;
    long result = strtol(value, &endptr, 10);

    // Check for conversion errors
    if (endptr == value || *endptr != '\0') {
        return ESP_ERR_INVALID_ARG; // Conversion failed
    }

    *out_value = (int)result;
    return ESP_OK;
}

// Get parameter value as float
esp_err_t query_params_get_float(const query_params_t* qp, const char* name, float* out_value)
{
    const char* value = query_params_get(qp, name);
    if (!value || !out_value) return ESP_ERR_INVALID_ARG;

    char* endptr;
    float result = strtof(value, &endptr);

    // Check for conversion errors
    if (endptr == value || *endptr != '\0') {
        return ESP_ERR_INVALID_ARG; // Conversion failed
    }

    *out_value = result;
    return ESP_OK;
}

// Get parameter value as time_t (parsed from ISO8601)
esp_err_t query_params_get_time(const query_params_t* qp, const char* name, time_t* out_value)
{
    const char* value = query_params_get(qp, name);
    if (!value || !out_value) return ESP_ERR_INVALID_ARG;

    return iso8601_to_time(value, out_value);
}

// Check if parameter exists
bool query_params_has(const query_params_t* qp, const char* name) {
    return query_params_get(qp, name) != nullptr;
}

// Get parameter count
size_t query_params_count(const query_params_t* qp) {
    return qp ? qp->count : 0;
}

// Clear all parameters
void query_params_clear(query_params_t* qp) {
    if (!qp) return;
    for (size_t i = 0; i < qp->count; i++) {
        free(qp->params[i].name);
        free(qp->params[i].value);
    }
    qp->count = 0;
}

// Comparison function for qsort
static int compare_params(const void* a, const void* b)
{
    const query_param_t* param_a = (const query_param_t*)a;
    const query_param_t* param_b = (const query_param_t*)b;
    return strcmp(param_a->name, param_b->name);
}

// Sort parameters by name
void query_params_sort(query_params_t* qp)
{
    if (!qp || qp->count == 0) return;
    qsort(qp->params, qp->count, sizeof(query_param_t), compare_params);
}

// Parse query string from URL and return sorted parameters
query_params_t* query_params_parse(const char* url)
{
    if (!url) return nullptr;

    query_params_t* qp = query_params_init();
    if (!qp) return nullptr;

    // Find query string start
    const char* query_start = strchr(url, '?');
    if (!query_start) {
        return qp; // No query string, return empty list
    }
    query_start++; // Skip '?'

    // Find fragment start (if any)
    const char* fragment = strchr(query_start, '#');
    size_t query_len = fragment ? (size_t)(fragment - query_start) : strlen(query_start);

    // Parse query string
    const char* current = query_start;
    const char* query_end = query_start + query_len;

    while (current < query_end) {
        // Find parameter name
        const char* equals = current;
        while (equals < query_end && *equals != '=' && *equals != '&') {
            equals++;
        }

        size_t name_len = equals - current;
        if (name_len == 0) {
            // Skip empty parameter
            current = equals + 1;
            continue;
        }

        char* name = strndup(current, name_len);
        if (!name) {
            query_params_free(qp);
            return nullptr;
        }

        // Find parameter value
        char* value = nullptr;
        if (*equals == '=') {
            const char* value_start = equals + 1;
            const char* ampersand = value_start;
            while (ampersand < query_end && *ampersand != '&') {
                ampersand++;
            }

            size_t value_len = ampersand - value_start;
            value = strndup(value_start, value_len);
            if (!value) {
                free(name);
                query_params_free(qp);
                return nullptr;
            }

            current = ampersand + 1;
        } else {
            current = equals + 1;
        }

        // Add parameter
        if (query_params_add(qp, name, value) != 0) {
            free(name);
            free(value);
            query_params_free(qp);
            return nullptr;
        }

        free(name);
        free(value);
    }

    // Sort parameters by name
    query_params_sort(qp);

    return qp;
}

// Convert parameters back to query string
char* query_params_to_string(const query_params_t* qp)
{
    if (!qp || qp->count == 0) return nullptr;

    // Calculate required buffer size
    size_t total_len = 0;
    for (size_t i = 0; i < qp->count; i++) {
        total_len += strlen(qp->params[i].name);
        if (qp->params[i].value) {
            total_len += 1 + strlen(qp->params[i].value); // '=' + value
        }
        if (i < qp->count - 1) {
            total_len += 1; // '&'
        }
    }

    char* result = malloc(total_len + 1);
    if (!result) return nullptr;

    char* ptr = result;
    for (size_t i = 0; i < qp->count; i++) {
        // Copy name
        size_t name_len = strlen(qp->params[i].name);
        memcpy(ptr, qp->params[i].name, name_len);
        ptr += name_len;

        // Copy value if exists
        if (qp->params[i].value) {
            *ptr++ = '=';
            size_t value_len = strlen(qp->params[i].value);
            memcpy(ptr, qp->params[i].value, value_len);
            ptr += value_len;
        }

        // Add separator
        if (i < qp->count - 1) {
            *ptr++ = '&';
        }
    }

    *ptr = '\0';
    return result;
}

// Convert parameters back to URL-encoded query string
char* query_params_encode(const query_params_t* qp)
{
    if (!qp || qp->count == 0) return nullptr;

    // Calculate buffer size with encoding
    size_t total_len = 0;
    for (size_t i = 0; i < qp->count; i++) {
        char* encoded_name = url_encode_string(qp->params[i].name);
        char* encoded_value = url_encode_string(qp->params[i].value);

        total_len += strlen(encoded_name) + 1; // name + '='
        if (encoded_value) {
            total_len += strlen(encoded_value);
        }
        if (i < qp->count - 1) {
            total_len += 1; // '&'
        }

        free(encoded_name);
        free(encoded_value);
    }

    char* result = malloc(total_len + 1);
    if (!result) return nullptr;

    char* ptr = result;
    for (size_t i = 0; i < qp->count; i++) {
        char* encoded_name = url_encode_string(qp->params[i].name);
        char* encoded_value = url_encode_string(qp->params[i].value);

        ptr += sprintf(ptr, "%s", encoded_name);
        if (encoded_value) {
            ptr += sprintf(ptr, "=%s", encoded_value);
        }
        if (i < qp->count - 1) {
            *ptr++ = '&';
        }

        free(encoded_name);
        free(encoded_value);
    }

    *ptr = '\0';
    return result;
}

// Free query parameters list
void query_params_free(query_params_t* qp)
{
    if (!qp) return;

    for (size_t i = 0; i < qp->count; i++) {
        free(qp->params[i].name);
        free(qp->params[i].value);
    }

    free(qp->params);
    free(qp);
}

// Initialize HTTP headers list
http_headers_t* http_headers_init(void)
{
    http_headers_t* hh = malloc(sizeof(http_headers_t));
    if (!hh) return nullptr;

    hh->headers = malloc(INITIAL_CAPACITY * sizeof(http_header_t));
    if (!hh->headers) {
        free(hh);
        return nullptr;
    }

    hh->count = 0;
    hh->capacity = INITIAL_CAPACITY;
    return hh;
}

// Add a header to the list
esp_err_t http_headers_add(http_headers_t* hh, const char* name, const char* value)
{
    if (!hh || !name) return ESP_ERR_INVALID_ARG;

    // Ensure capacity
    if (ensure_capacity((void**)&hh->headers, &hh->capacity, hh->count, sizeof(http_header_t)) != ESP_OK) {
        return ESP_ERR_NO_MEM;
    }

    // Allocate and copy name
    hh->headers[hh->count].name = strdup(name);
    if (!hh->headers[hh->count].name) return ESP_ERR_NO_MEM;

    // Allocate and copy value
    if (value) {
        hh->headers[hh->count].value = strdup(value);
        if (!hh->headers[hh->count].value) {
            free(hh->headers[hh->count].name);
            return ESP_ERR_NO_MEM;
        }
    } else {
        hh->headers[hh->count].value = nullptr;
    }

    hh->count++;
    return ESP_OK;
}

// Set a header value (replaces existing value if header exists, case-insensitive)
esp_err_t http_headers_set(http_headers_t* hh, const char* name, const char* value)
{
    if (!hh || !name) return ESP_ERR_INVALID_ARG;

    // Try to find existing header (case-insensitive)
    for (size_t i = 0; i < hh->count; i++) {
        if (strcasecmp(hh->headers[i].name, name) == 0) {
            // Found existing header, replace value
            free(hh->headers[i].value);
            hh->headers[i].value = value ? strdup(value) : nullptr;

            if (value && !hh->headers[i].value) {
                return ESP_ERR_NO_MEM;
            }

            return ESP_OK;
        }
    }

    // Header not found, add new one
    return http_headers_add(hh, name, value);
}

// Add a header with formatted value
esp_err_t http_headers_add_fmt(http_headers_t* hh, const char* name, const char* fmt, ...)
{
    if (!hh || !name || !fmt) return ESP_ERR_INVALID_ARG;

    va_list args;
    va_start(args, fmt);

    // Calculate required buffer size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);
        return ESP_FAIL;
    }

    // Allocate buffer and format value
    char* value = malloc(size + 1);
    if (!value) {
        va_end(args);
        return ESP_ERR_NO_MEM;
    }

    vsnprintf(value, size + 1, fmt, args);
    va_end(args);

    // Add header
    esp_err_t result = http_headers_add(hh, name, value);
    free(value);

    return result;
}

// Add a header with integer value
esp_err_t http_headers_add_int(http_headers_t* hh, const char* name, int value)
{
    return http_headers_add_fmt(hh, name, "%d", value);
}

// Add a header with float value
esp_err_t http_headers_add_float(http_headers_t* hh, const char* name, float value)
{
    return http_headers_add_fmt(hh, name, "%g", value);
}

// Add a header with time_t value (stored as ISO8601)
esp_err_t http_headers_add_time(http_headers_t* hh, const char* name, time_t time)
{
    char* iso8601 = time_to_iso8601(time);
    if (!iso8601) return ESP_ERR_NO_MEM;

    esp_err_t result = http_headers_add(hh, name, iso8601);
    free(iso8601);
    return result;
}

// Get header value as integer (case-insensitive)
esp_err_t http_headers_get_int(const http_headers_t* hh, const char* name, int* out_value)
{
    const char* value = http_headers_get(hh, name);
    if (!value || !out_value) return ESP_ERR_INVALID_ARG;

    char* endptr;
    long result = strtol(value, &endptr, 10);

    // Check for conversion errors
    if (endptr == value || *endptr != '\0') {
        return ESP_ERR_INVALID_ARG; // Conversion failed
    }

    *out_value = (int)result;
    return ESP_OK;
}

// Get header value as float (case-insensitive)
esp_err_t http_headers_get_float(const http_headers_t* hh, const char* name, float* out_value)
{
    const char* value = http_headers_get(hh, name);
    if (!value || !out_value) return ESP_ERR_INVALID_ARG;

    char* endptr;
    float result = strtof(value, &endptr);

    // Check for conversion errors
    if (endptr == value || *endptr != '\0') {
        return ESP_ERR_INVALID_ARG; // Conversion failed
    }

    *out_value = result;
    return ESP_OK;
}

// Get header value as time_t (parsed from ISO8601, case-insensitive)
esp_err_t http_headers_get_time(const http_headers_t* hh, const char* name, time_t* out_value)
{
    const char* value = http_headers_get(hh, name);
    if (!value || !out_value) return ESP_ERR_INVALID_ARG;

    return iso8601_to_time(value, out_value);
}

// Set Bearer token authorization
esp_err_t http_headers_set_bearer_token(http_headers_t* hh, const char* token) {
    if (!hh || !token) return ESP_ERR_INVALID_ARG;
    return http_headers_add_fmt(hh, "Authorization", "Bearer %s", token);
}

// Set Basic authorization
esp_err_t http_headers_set_basic_auth(http_headers_t* hh, const char* username, const char* password) {
    if (!hh || !username || !password) return ESP_ERR_INVALID_ARG;
    
    // Create credentials string: username:password
    size_t creds_len = strlen(username) + 1 + strlen(password);
    char* creds = malloc(creds_len + 1);
    if (!creds) return ESP_ERR_NO_MEM;
    
    snprintf(creds, creds_len + 1, "%s:%s", username, password);
    
    // Encode to base64
    char* encoded = base64_encode(creds, creds_len);
    free(creds);
    
    if (!encoded) return ESP_ERR_NO_MEM;
    
    // Set the Basic auth header
    esp_err_t result = http_headers_add_fmt(hh, "Authorization", "Basic %s", encoded);
    free(encoded);
    
    return result;
}

// Comparison function for qsort (case-insensitive)
static int compare_headers(const void* a, const void* b)
{
    const http_header_t* header_a = (const http_header_t*)a;
    const http_header_t* header_b = (const http_header_t*)b;
    return strcasecmp(header_a->name, header_b->name);
}

// Sort headers by name
void http_headers_sort(http_headers_t* hh)
{
    if (!hh || hh->count == 0) return;
    qsort(hh->headers, hh->count, sizeof(http_header_t), compare_headers);
}

// Get header value by name (case-insensitive)
const char* http_headers_get(const http_headers_t* hh, const char* name)
{
    if (!hh || !name) return nullptr;

    for (size_t i = 0; i < hh->count; i++) {
        if (strcasecmp(hh->headers[i].name, name) == 0) {
            return hh->headers[i].value;
        }
    }

    return nullptr;
}

// Remove header by name (case-insensitive)
esp_err_t http_headers_remove(http_headers_t* hh, const char* name)
{
    if (!hh || !name) return ESP_ERR_INVALID_ARG;

    for (size_t i = 0; i < hh->count; i++) {
        if (strcasecmp(hh->headers[i].name, name) == 0) {
            // Free memory for this header
            free(hh->headers[i].name);
            free(hh->headers[i].value);

            // Shift remaining headers
            for (size_t j = i; j < hh->count - 1; j++) {
                hh->headers[j] = hh->headers[j + 1];
            }

            hh->count--;
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND; // Header not found
}

// Convert headers to string format (for HTTP request/response)
char* http_headers_to_string(const http_headers_t* hh)
{
    if (!hh || hh->count == 0) return nullptr;

    // Calculate required buffer size
    size_t total_len = 0;
    for (size_t i = 0; i < hh->count; i++) {
        total_len += strlen(hh->headers[i].name);
        total_len += 2; // ": "
        if (hh->headers[i].value) {
            total_len += strlen(hh->headers[i].value);
        }
        total_len += 2; // "\r\n"
    }

    char* result = malloc(total_len + 1);
    if (!result) return nullptr;

    char* ptr = result;
    for (size_t i = 0; i < hh->count; i++) {
        // Copy name
        size_t name_len = strlen(hh->headers[i].name);
        memcpy(ptr, hh->headers[i].name, name_len);
        ptr += name_len;

        // Add separator
        *ptr++ = ':';
        *ptr++ = ' ';

        // Copy value if exists
        if (hh->headers[i].value) {
            size_t value_len = strlen(hh->headers[i].value);
            memcpy(ptr, hh->headers[i].value, value_len);
            ptr += value_len;
        }

        // Add line ending
        *ptr++ = '\r';
        *ptr++ = '\n';
    }

    *ptr = '\0';
    return result;
}

// Dump HTTP headers to log for debugging
void http_headers_dump(const http_headers_t* hh)
{
    if (!hh) {
        ESP_LOGW("HTTP_HEADERS", "Headers list is NULL");
        return;
    }

    if (hh->count == 0) {
        ESP_LOGI("HTTP_HEADERS", "No headers");
        return;
    }

    ESP_LOGI("HTTP_HEADERS", "Total headers: %zu", hh->count);
    for (size_t i = 0; i < hh->count; i++) {
        ESP_LOGI("HTTP_HEADERS", "  [%zu] %s: %s", 
                 i, 
                 hh->headers[i].name ? hh->headers[i].name : "(null)",
                 hh->headers[i].value ? hh->headers[i].value : "(null)");
    }
}

// Free HTTP headers list
void http_headers_free(http_headers_t* hh)
{
    if (!hh) return;

    for (size_t i = 0; i < hh->count; i++) {
        free(hh->headers[i].name);
        free(hh->headers[i].value);
    }

    free(hh->headers);
    free(hh);
}

// ============================================================================
// URI Functions (path + query + fragment)
// ============================================================================

// Initialize URI object
uri_t* uri_init(void)
{
    uri_t* uri = malloc(sizeof(uri_t));
    if (!uri) return nullptr;

    uri->path = nullptr;
    uri->params = query_params_init();
    uri->fragment = nullptr;

    if (!uri->params) {
        free(uri);
        return nullptr;
    }

    return uri;
}

// Parse URI string into URI object
uri_t* uri_parse(const char* uri_string)
{
    if (!uri_string) return nullptr;

    uri_t* uri = uri_init();
    if (!uri) return nullptr;

    const char* ptr = uri_string;

    // Parse path
    if (*ptr == '/') {
        const char* path_end = ptr;
        while (*path_end && *path_end != '?' && *path_end != '#') {
            path_end++;
        }

        size_t path_len = path_end - ptr;
        uri->path = strndup(ptr, path_len);
        ptr = path_end;
    }

    // Parse query parameters
    if (*ptr == '?') {
        ptr++;
        const char* query_end = ptr;
        while (*query_end && *query_end != '#') {
            query_end++;
        }

        // Parse each parameter
        const char* current = ptr;
        while (current < query_end) {
            const char* equals = current;
            while (equals < query_end && *equals != '=' && *equals != '&') {
                equals++;
            }

            size_t name_len = equals - current;
            if (name_len > 0) {
                char* name = strndup(current, name_len);
                if (name) {
                    char* value = nullptr;
                    if (*equals == '=') {
                        const char* value_start = equals + 1;
                        const char* ampersand = value_start;
                        while (ampersand < query_end && *ampersand != '&') {
                            ampersand++;
                        }

                        size_t value_len = ampersand - value_start;
                        value = strndup(value_start, value_len);
                        current = ampersand + 1;
                    } else {
                        current = equals + 1;
                    }

                    query_params_add(uri->params, name, value);
                    free(name);
                    free(value);
                }
            } else {
                current++;
            }
        }

        ptr = query_end;
    }

    // Parse fragment
    if (*ptr == '#') {
        ptr++;
        uri->fragment = strdup(ptr);
    }

    return uri;
}

// Build URI string from URI object (without encoding)
char* uri_to_string(const uri_t* uri)
{
    if (!uri) return nullptr;

    size_t total_len = 0;

    // Calculate required buffer size
    if (uri->path) {
        total_len += strlen(uri->path);
    }

    if (uri->params && uri->params->count > 0) {
        char* query_string = query_params_to_string(uri->params);
        if (query_string) {
            total_len += 1 + strlen(query_string); // '?' + query string
            free(query_string);
        }
    }

    if (uri->fragment) {
        total_len += 1 + strlen(uri->fragment); // '#' + fragment
    }

    char* result = malloc(total_len + 1);
    if (!result) return nullptr;

    char* ptr = result;

    // Build URI string
    if (uri->path) {
        ptr += sprintf(ptr, "%s", uri->path);
    }

    if (uri->params && uri->params->count > 0) {
        char* query_string = query_params_to_string(uri->params);
        if (query_string) {
            *ptr++ = '?';
            size_t query_len = strlen(query_string);
            memcpy(ptr, query_string, query_len);
            ptr += query_len;
            free(query_string);
        }
    }

    if (uri->fragment) {
        ptr += sprintf(ptr, "#%s", uri->fragment);
    }

    *ptr = '\0';
    return result;
}

// Build URI string from URI object (with URL encoding)
char* uri_encode(const uri_t* uri)
{
    if (!uri) return nullptr;

    // Pre-calculate maximum possible size
    size_t max_len = 0;
    
    if (uri->path) max_len += strlen(uri->path);
    
    // For params: worst case is all chars need encoding (3x size)
    if (uri->params && uri->params->count > 0) {
        max_len += 1; // '?'
        for (size_t i = 0; i < uri->params->count; i++) {
            max_len += strlen(uri->params->params[i].name) * 3 + 1;
            if (uri->params->params[i].value) {
                max_len += strlen(uri->params->params[i].value) * 3;
            }
            if (i < uri->params->count - 1) max_len += 1;
        }
    }
    
    if (uri->fragment) {
        max_len += 1 + strlen(uri->fragment) * 3;
    }

    char* result = malloc(max_len + 1);
    if (!result) return nullptr;

    char* ptr = result;

    // Build URI string
    if (uri->path) {
        ptr += sprintf(ptr, "%s", uri->path);
    }

    if (uri->params && uri->params->count > 0) {
        bool first_param = true;
        for (size_t i = 0; i < uri->params->count; i++) {
            char* enc_name = url_encode_string(uri->params->params[i].name);
            if (!enc_name) continue;
            
            if (first_param) {
                *ptr++ = '?';
                first_param = false;
            } else {
                *ptr++ = '&';
            }
            
            size_t len = strlen(enc_name);
            memcpy(ptr, enc_name, len);
            ptr += len;
            free(enc_name);
            
            *ptr++ = '=';
            
            if (uri->params->params[i].value) {
                char* enc_val = url_encode_string(uri->params->params[i].value);
                if (enc_val) {
                    size_t val_len = strlen(enc_val);
                    memcpy(ptr, enc_val, val_len);
                    ptr += val_len;
                    free(enc_val);
                }
            }
        }
    }

    if (uri->fragment) {
        *ptr++ = '#';
        char* enc_frag = url_encode_string(uri->fragment);
        if (enc_frag) {
            size_t len = strlen(enc_frag);
            memcpy(ptr, enc_frag, len);
            ptr += len;
            free(enc_frag);
        }
    }

    *ptr = '\0';
    
    size_t actual_len = ptr - result;
    char* final = realloc(result, actual_len + 1);
    return final ? final : result;
}

// Set URI path
esp_err_t uri_set_path(uri_t* uri, const char* path)
{
    if (!uri) return ESP_ERR_INVALID_ARG;

    free(uri->path);
    uri->path = path ? strdup(path) : nullptr;
    return uri->path || !path ? ESP_OK : ESP_ERR_NO_MEM;
}

// Set URI fragment
esp_err_t uri_set_fragment(uri_t* uri, const char* fragment)
{
    if (!uri) return ESP_ERR_INVALID_ARG;

    free(uri->fragment);
    uri->fragment = fragment ? strdup(fragment) : nullptr;
    return uri->fragment || !fragment ? ESP_OK : ESP_ERR_NO_MEM;
}

// Get URI query parameters
query_params_t* uri_get_params(uri_t* uri)
{
    return uri ? uri->params : nullptr;
}

// Set URI query parameters
esp_err_t uri_set_params(uri_t* uri, query_params_t* params)
{
    if (!uri) return ESP_ERR_INVALID_ARG;
    
    if (uri->params) {
        query_params_free(uri->params);
    }
    
    uri->params = params;
    return ESP_OK;
}

// Get URI path
const char* uri_get_path(const uri_t* uri)
{
    return uri ? uri->path : nullptr;
}

// Get URI fragment
const char* uri_get_fragment(const uri_t* uri)
{
    return uri ? uri->fragment : nullptr;
}

// Clone URI object
uri_t* uri_clone(const uri_t* uri)
{
    if (!uri) return nullptr;
    
    uri_t* clone = uri_init();
    if (!clone) return nullptr;
    
    if (uri->path) uri_set_path(clone, uri->path);
    if (uri->fragment) uri_set_fragment(clone, uri->fragment);
    
    if (uri->params) {
        for (size_t i = 0; i < uri->params->count; i++) {
            query_params_add(clone->params, 
                           uri->params->params[i].name,
                           uri->params->params[i].value);
        }
    }
    
    return clone;
}

// Merge query parameters from another URI
esp_err_t uri_merge_params(uri_t* dest, const uri_t* src)
{
    if (!dest || !src || !src->params) return ESP_ERR_INVALID_ARG;
    
    for (size_t i = 0; i < src->params->count; i++) {
        esp_err_t err = query_params_add(dest->params,
                                        src->params->params[i].name,
                                        src->params->params[i].value);
        if (err != ESP_OK) return err;
    }
    
    return ESP_OK;
}

// Free URI object
void uri_free(uri_t* uri)
{
    if (!uri) return;

    free(uri->path);
    free(uri->fragment);
    query_params_free(uri->params);
    free(uri);
}

// ============================================================================
// URL Functions
// ============================================================================

// Initialize URL object
url_t* url_init(void)
{
    url_t* url = malloc(sizeof(url_t));
    if (!url) return nullptr;

    url->protocol = nullptr;
    url->host = nullptr;
    url->port = -1;
    url->path = nullptr;
    url->params = query_params_init();
    url->fragment = nullptr;
    url->method = E_HTTP_METHOD_GET;  // Default to GET

    if (!url->params) {
        free(url);
        return nullptr;
    }

    return url;
}

// Parse URL string into URL object
url_t* url_parse(const char* url_string)
{
    if (!url_string) return nullptr;

    url_t* url = url_init();
    if (!url) return nullptr;

    const char* ptr = url_string;

    // Parse protocol
    const char* protocol_end = strstr(ptr, "://");
    if (protocol_end) {
        size_t protocol_len = protocol_end - ptr;
        url->protocol = strndup(ptr, protocol_len);
        ptr = protocol_end + 3;
    }

    // Parse host and port
    const char* host_end = ptr;
    while (*host_end && *host_end != ':' && *host_end != '/' && *host_end != '?' && *host_end != '#') {
        host_end++;
    }

    size_t host_len = host_end - ptr;
    if (host_len > 0) {
        url->host = strndup(ptr, host_len);
    }

    ptr = host_end;

    // Parse port
    if (*ptr == ':') {
        ptr++;
        url->port = atoi(ptr);
        while (*ptr && *ptr >= '0' && *ptr <= '9') {
            ptr++;
        }
    }

    // Parse path
    if (*ptr == '/') {
        const char* path_end = ptr;
        while (*path_end && *path_end != '?' && *path_end != '#') {
            path_end++;
        }

        size_t path_len = path_end - ptr;
        url->path = strndup(ptr, path_len);
        ptr = path_end;
    }

    // Parse query parameters
    if (*ptr == '?') {
        ptr++;
        const char* query_end = ptr;
        while (*query_end && *query_end != '#') {
            query_end++;
        }

        // Parse each parameter
        const char* current = ptr;
        while (current < query_end) {
            const char* equals = current;
            while (equals < query_end && *equals != '=' && *equals != '&') {
                equals++;
            }

            size_t name_len = equals - current;
            if (name_len > 0) {
                char* name = strndup(current, name_len);
                if (name) {
                    char* value = nullptr;
                    if (*equals == '=') {
                        const char* value_start = equals + 1;
                        const char* ampersand = value_start;
                        while (ampersand < query_end && *ampersand != '&') {
                            ampersand++;
                        }

                        size_t value_len = ampersand - value_start;
                        value = strndup(value_start, value_len);
                        current = ampersand + 1;
                    } else {
                        current = equals + 1;
                    }

                    query_params_add(url->params, name, value);
                    free(name);
                    free(value);
                }
            } else {
                current++;
            }
        }

        ptr = query_end;
    }

    // Parse fragment
    if (*ptr == '#') {
        ptr++;
        url->fragment = strdup(ptr);
    }

    return url;
}

// Build URL string from URL object (without encoding)
char* url_to_string(const url_t* url)
{
    if (!url) return nullptr;

    size_t total_len = 0;

    // Calculate required buffer size
    if (url->protocol) {
        total_len += strlen(url->protocol) + 3; // "://"
    }

    if (url->host) {
        total_len += strlen(url->host);
    }

    if (url->port > 0) {
        total_len += 6; // ":65535"
    }

    if (url->path) {
        total_len += strlen(url->path);
    }

    if (url->params && url->params->count > 0) {
        char* query_string = query_params_to_string(url->params);
        if (query_string) {
            total_len += 1 + strlen(query_string); // '?' + query string
            free(query_string);
        }
    }

    if (url->fragment) {
        total_len += 1 + strlen(url->fragment); // '#' + fragment
    }

    char* result = malloc(total_len + 1);
    if (!result) return nullptr;

    char* ptr = result;

    // Build URL string
    if (url->protocol) {
        ptr += sprintf(ptr, "%s://", url->protocol);
    }

    if (url->host) {
        ptr += sprintf(ptr, "%s", url->host);
    }

    if (url->port > 0) {
        ptr += sprintf(ptr, ":%d", url->port);
    }

    if (url->path) {
        ptr += sprintf(ptr, "%s", url->path);
    }

    if (url->params && url->params->count > 0) {
        char* query_string = query_params_to_string(url->params);
        if (query_string) {
            *ptr++ = '?';
            size_t query_len = strlen(query_string);
            memcpy(ptr, query_string, query_len);
            ptr += query_len;
            free(query_string);
        }
    }

    if (url->fragment) {
        ptr += sprintf(ptr, "#%s", url->fragment);
    }

    *ptr = '\0';
    return result;
}

// Build URL string from URL object (with URL encoding)
char* url_encode(const url_t* url)
{
    if (!url) return nullptr;

    // Pre-calculate maximum possible size (worst case: all chars encoded)
    size_t max_len = 0;
    
    if (url->protocol) max_len += strlen(url->protocol) + 3;
    if (url->host) max_len += strlen(url->host);
    if (url->port > 0) max_len += 6;
    if (url->path) max_len += strlen(url->path);
    
    // For params: worst case is all chars need encoding (3x size)
    if (url->params && url->params->count > 0) {
        max_len += 1; // '?'
        for (size_t i = 0; i < url->params->count; i++) {
            max_len += strlen(url->params->params[i].name) * 3 + 1; // name + '='
            if (url->params->params[i].value) {
                max_len += strlen(url->params->params[i].value) * 3;
            }
            if (i < url->params->count - 1) max_len += 1; // '&'
        }
    }
    
    if (url->fragment) {
        max_len += 1 + strlen(url->fragment) * 3; // '#' + fragment
    }

    // Allocate buffer with worst-case size
    char* result = malloc(max_len + 1);
    if (!result) return nullptr;

    char* ptr = result;

    // Build URL string in one pass
    if (url->protocol) {
        ptr += sprintf(ptr, "%s://", url->protocol);
    }

    if (url->host) {
        ptr += sprintf(ptr, "%s", url->host);
    }

    if (url->port > 0) {
        ptr += sprintf(ptr, ":%d", url->port);
    }

    if (url->path) {
        ptr += sprintf(ptr, "%s", url->path);
    }

    if (url->params && url->params->count > 0) {
        bool first_param = true;
        for (size_t i = 0; i < url->params->count; i++) {
            // Encode name
            char* enc_name = url_encode_string(url->params->params[i].name);
            if (!enc_name) continue; // Skip if encoding fails
            
            // Add separator
            if (first_param) {
                *ptr++ = '?';
                first_param = false;
            } else {
                *ptr++ = '&';
            }
            
            // Add encoded name
            size_t len = strlen(enc_name);
            memcpy(ptr, enc_name, len);
            ptr += len;
            free(enc_name);
            
            *ptr++ = '=';
            
            // Encode value
            if (url->params->params[i].value) {
                char* enc_val = url_encode_string(url->params->params[i].value);
                if (enc_val) {
                    size_t val_len = strlen(enc_val);
                    memcpy(ptr, enc_val, val_len);
                    ptr += val_len;
                    free(enc_val);
                }
            }
        }
    }

    if (url->fragment) {
        *ptr++ = '#';
        char* enc_frag = url_encode_string(url->fragment);
        if (enc_frag) {
            size_t len = strlen(enc_frag);
            memcpy(ptr, enc_frag, len);
            ptr += len;
            free(enc_frag);
        }
    }

    *ptr = '\0';
    
    // Shrink buffer to actual size (optional optimization)
    size_t actual_len = ptr - result;
    char* final = realloc(result, actual_len + 1);
    return final ? final : result;
}

// Set URL HTTP method
esp_err_t url_set_method(url_t* url, http_method_t method) {
    if (!url) return ESP_ERR_INVALID_ARG;
    url->method = method;
    return ESP_OK;
}

// Get URL HTTP method
http_method_t url_get_method(const url_t* url) {
    return url ? url->method : E_HTTP_METHOD_GET;
}

// Set URL protocol
esp_err_t url_set_protocol(url_t* url, const char* protocol)
{
    if (!url) return ESP_ERR_INVALID_ARG;

    free(url->protocol);
    url->protocol = protocol ? strdup(protocol) : nullptr;
    return url->protocol || !protocol ? ESP_OK : ESP_ERR_NO_MEM;
}

// Set URL host
esp_err_t url_set_host(url_t* url, const char* host)
{
    if (!url) return ESP_ERR_INVALID_ARG;

    free(url->host);
    url->host = host ? strdup(host) : nullptr;
    return url->host || !host ? ESP_OK : ESP_ERR_NO_MEM;
}

// Set URL port
esp_err_t url_set_port(url_t* url, int port)
{
    if (!url) return ESP_ERR_INVALID_ARG;
    url->port = port;
    return ESP_OK;
}

// Set URL path
esp_err_t url_set_path(url_t* url, const char* path)
{
    if (!url) return ESP_ERR_INVALID_ARG;

    free(url->path);
    url->path = path ? strdup(path) : nullptr;
    return url->path || !path ? ESP_OK : ESP_ERR_NO_MEM;
}

// Set URL fragment
esp_err_t url_set_fragment(url_t* url, const char* fragment)
{
    if (!url) return ESP_ERR_INVALID_ARG;

    free(url->fragment);
    url->fragment = fragment ? strdup(fragment) : nullptr;
    return url->fragment || !fragment ? ESP_OK : ESP_ERR_NO_MEM;
}

// Get URL query parameters (direct access to query_params_t)
query_params_t* url_get_params(url_t* url)
{
    return url ? url->params : nullptr;
}

// Set URL query parameters (replaces existing params)
esp_err_t url_set_params(url_t* url, query_params_t* params)
{
    if (!url) return ESP_ERR_INVALID_ARG;
    
    // Free old params if exists
    if (url->params) {
        query_params_free(url->params);
    }
    
    // Set new params
    url->params = params;
    return ESP_OK;
}

// Get URL protocol
const char* url_get_protocol(const url_t* url)
{
    return url ? url->protocol : nullptr;
}

// Get URL host
const char* url_get_host(const url_t* url)
{
    return url ? url->host : nullptr;
}

// Get URL port
int url_get_port(const url_t* url)
{
    return url ? url->port : -1;
}

// Get URL path
const char* url_get_path(const url_t* url)
{
    return url ? url->path : nullptr;
}

// Get URL fragment
const char* url_get_fragment(const url_t* url)
{
    return url ? url->fragment : nullptr;
}

// Free URL object
void url_free(url_t* url)
{
    if (!url) return;

    free(url->protocol);
    free(url->host);
    free(url->path);
    free(url->fragment);
    query_params_free(url->params);
    free(url);
}

// Clone URL object
url_t* url_clone(const url_t* url) {
    if (!url) return nullptr;
    
    url_t* clone = url_init();
    if (!clone) return nullptr;
    
    // Copy all fields
    if (url->protocol) url_set_protocol(clone, url->protocol);
    if (url->host) url_set_host(clone, url->host);
    clone->port = url->port;
    if (url->path) url_set_path(clone, url->path);
    if (url->fragment) url_set_fragment(clone, url->fragment);
    clone->method = url->method;
    
    // Clone params
    if (url->params) {
        for (size_t i = 0; i < url->params->count; i++) {
            query_params_add(clone->params, 
                           url->params->params[i].name,
                           url->params->params[i].value);
        }
    }
    
    return clone;
}

// Merge query parameters from another URL
esp_err_t url_merge_params(url_t* dest, const url_t* src) {
    if (!dest || !src || !src->params) return ESP_ERR_INVALID_ARG;
    
    for (size_t i = 0; i < src->params->count; i++) {
        esp_err_t err = query_params_add(dest->params,
                                        src->params->params[i].name,
                                        src->params->params[i].value);
        if (err != ESP_OK) return err;
    }
    
    return ESP_OK;
}

// Create URL from URI and additional components
url_t* url_from_uri(const char* protocol, const char* host, int port, const uri_t* uri) {
    if (!uri) return nullptr;
    
    url_t* url = url_init();
    if (!url) return nullptr;
    
    // Set protocol and host
    if (protocol) {
        if (url_set_protocol(url, protocol) != ESP_OK) {
            url_free(url);
            return nullptr;
        }
    }
    
    if (host) {
        if (url_set_host(url, host) != ESP_OK) {
            url_free(url);
            return nullptr;
        }
    }
    
    // Set port
    url->port = port;
    
    // Set default method
    url->method = E_HTTP_METHOD_GET;  // Default to GET
    
    // Copy path from URI
    if (uri->path) {
        if (url_set_path(url, uri->path) != ESP_OK) {
            url_free(url);
            return nullptr;
        }
    }
    
    // Copy fragment from URI
    if (uri->fragment) {
        if (url_set_fragment(url, uri->fragment) != ESP_OK) {
            url_free(url);
            return nullptr;
        }
    }
    
    // Copy query parameters from URI
    if (uri->params) {
        for (size_t i = 0; i < uri->params->count; i++) {
            esp_err_t err = query_params_add(url->params,
                                            uri->params->params[i].name,
                                            uri->params->params[i].value);
            if (err != ESP_OK) {
                url_free(url);
                return nullptr;
            }
        }
    }
    
    return url;
}
