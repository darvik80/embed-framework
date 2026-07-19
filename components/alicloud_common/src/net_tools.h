//
// Created by Ivan Kishchenko on 16/2/26.
//

#ifndef HELPER_NET_TOOLS_H
#define HELPER_NET_TOOLS_H

#include <stddef.h>
#include <time.h>
#include "esp_err.h"   // ESP-IDF error codes

// Structure to hold a single query parameter
typedef struct {
    char* name;
    char* value;
} query_param_t;

// Structure to hold a list of query parameters
typedef struct {
    query_param_t* params;
    size_t count;
    size_t capacity;
} query_params_t;

/**
 * @brief Extract host from URL string
 *
 * @param url URL string to parse
 * @return Host string (must be freed by caller) or nullptr on failure
 */
char* get_host_from_url(const char* url);

/**
 * @brief Initialize a new query parameters list
 *
 * @return Pointer to initialized query_params_t structure or nullptr on allocation failure
 */
query_params_t* query_params_init(void);

/**
 * @brief Add a parameter to the query parameters list
 *
 * @param qp Pointer to query parameters list
 * @param name Parameter name
 * @param value Parameter value (can be nullptr)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t query_params_add(query_params_t* qp, const char* name, const char* value);

/**
 * @brief Set a parameter value (replaces existing value if parameter exists)
 *
 * @param qp Pointer to query parameters list
 * @param name Parameter name
 * @param value Parameter value (can be nullptr)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t query_params_set(query_params_t* qp, const char* name, const char* value);

/**
 * @brief Add a parameter with formatted value
 *
 * @param qp Pointer to query parameters list
 * @param name Parameter name
 * @param fmt Format string for value
 * @param ... Variable arguments for format string
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t query_params_add_fmt(query_params_t* qp, const char* name, const char* fmt, ...);

/**
 * @brief Add a parameter with integer value
 *
 * @param qp Pointer to query parameters list
 * @param name Parameter name
 * @param value Integer value to add
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t query_params_add_int(query_params_t* qp, const char* name, int value);

/**
 * @brief Add a parameter with float value
 *
 * @param qp Pointer to query parameters list
 * @param name Parameter name
 * @param value Float value to add
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t query_params_add_float(query_params_t* qp, const char* name, float value);

/**
 * @brief Add a parameter with time_t value (stored as ISO8601 string)
 *
 * @param qp Pointer to query parameters list
 * @param name Parameter name
 * @param time Time value to convert to ISO8601 format
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t query_params_add_time(query_params_t* qp, const char* name, time_t time);

/**
 * @brief Get parameter value as integer
 *
 * @param qp Pointer to query parameters list
 * @param name Parameter name to search for
 * @param out_value Pointer to store the integer result
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NOT_FOUND on failure
 */
esp_err_t query_params_get_int(const query_params_t* qp, const char* name, int* out_value);

/**
 * @brief Get parameter value as float
 *
 * @param qp Pointer to query parameters list
 * @param name Parameter name to search for
 * @param out_value Pointer to store the float result
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NOT_FOUND on failure
 */
esp_err_t query_params_get_float(const query_params_t* qp, const char* name, float* out_value);

/**
 * @brief Get parameter value as time_t (parsed from ISO8601)
 *
 * @param qp Pointer to query parameters list
 * @param name Parameter name to search for
 * @param out_value Pointer to store the time_t result
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NOT_FOUND on failure
 */
esp_err_t query_params_get_time(const query_params_t* qp, const char* name, time_t* out_value);

/**
 * @brief Get parameter value as string
 *
 * @param qp Pointer to query parameters list
 * @param name Parameter name to search for
 * @return Parameter value string or nullptr if not found
 */
const char* query_params_get(const query_params_t* qp, const char* name);

/**
 * @brief Check if parameter exists in the list
 *
 * @param qp Pointer to query parameters list
 * @param name Parameter name to check
 * @return true if parameter exists, false otherwise
 */
bool query_params_has(const query_params_t* qp, const char* name);

/**
 * @brief Get the number of parameters in the list
 *
 * @param qp Pointer to query parameters list
 * @return Number of parameters
 */
size_t query_params_count(const query_params_t* qp);

/**
 * @brief Clear all parameters from the list
 *
 * @param qp Pointer to query parameters list
 */
void query_params_clear(query_params_t* qp);

/**
 * @brief Parse query string from URL and return sorted parameters
 *
 * @param url URL string containing query parameters
 * @return Pointer to new query_params_t structure or nullptr on failure
 */
query_params_t* query_params_parse(const char* url);

/**
 * @brief Sort parameters alphabetically by name
 *
 * @param qp Pointer to query parameters list
 */
void query_params_sort(query_params_t* qp);

/**
 * @brief Convert parameters back to query string (without URL encoding)
 *
 * @param qp Pointer to query parameters list
 * @return Query string (must be freed by caller) or nullptr on failure
 */
char* query_params_to_string(const query_params_t* qp);

/**
 * @brief Encode parameters to URL-encoded query string
 *
 * @param qp Pointer to query parameters list
 * @return URL-encoded query string (must be freed by caller) or nullptr on failure
 */
char* query_params_encode(const query_params_t* qp);

/**
 * @brief Free query parameters list and all associated memory
 *
 * @param qp Pointer to query parameters list
 */
void query_params_free(query_params_t* qp);

/**
 * @brief Structure to hold a single HTTP header
 */
typedef struct {
    char* name;
    char* value;
} http_header_t;

/**
 * @brief Structure to hold a list of HTTP headers
 */
typedef struct {
    http_header_t* headers;
    size_t count;
    size_t capacity;
} http_headers_t;

/**
 * @brief Initialize a new HTTP headers list
 *
 * @return Pointer to initialized http_headers_t structure or nullptr on allocation failure
 */
http_headers_t* http_headers_init(void);

/**
 * @brief Add a header to the list
 *
 * @param hh Pointer to HTTP headers list
 * @param name Header name
 * @param value Header value (can be nullptr)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t http_headers_add(http_headers_t* hh, const char* name, const char* value);

/**
 * @brief Set a header value (replaces existing value if header exists, case-insensitive)
 *
 * @param hh Pointer to HTTP headers list
 * @param name Header name
 * @param value Header value (can be nullptr)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t http_headers_set(http_headers_t* hh, const char* name, const char* value);

/**
 * @brief Add a header with formatted value
 *
 * @param hh Pointer to HTTP headers list
 * @param name Header name
 * @param fmt Format string for value
 * @param ... Variable arguments for format string
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t http_headers_add_fmt(http_headers_t* hh, const char* name, const char* fmt, ...);

/**
 * @brief Add a header with integer value
 *
 * @param hh Pointer to HTTP headers list
 * @param name Header name
 * @param value Integer value to add
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t http_headers_add_int(http_headers_t* hh, const char* name, int value);

/**
 * @brief Add a header with float value
 *
 * @param hh Pointer to HTTP headers list
 * @param name Header name
 * @param value Float value to add
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t http_headers_add_float(http_headers_t* hh, const char* name, float value);

/**
 * @brief Add a header with time_t value (stored as ISO8601 string)
 *
 * @param hh Pointer to HTTP headers list
 * @param name Header name
 * @param time Time value to convert to ISO8601 format
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t http_headers_add_time(http_headers_t* hh, const char* name, time_t time);

/**
 * @brief Get header value as integer (case-insensitive search)
 *
 * @param hh Pointer to HTTP headers list
 * @param name Header name to search for
 * @param out_value Pointer to store the integer result
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NOT_FOUND on failure
 */
esp_err_t http_headers_get_int(const http_headers_t* hh, const char* name, int* out_value);

/**
 * @brief Get header value as float (case-insensitive search)
 *
 * @param hh Pointer to HTTP headers list
 * @param name Header name to search for
 * @param out_value Pointer to store the float result
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NOT_FOUND on failure
 */
esp_err_t http_headers_get_float(const http_headers_t* hh, const char* name, float* out_value);

/**
 * @brief Get header value as time_t (parsed from ISO8601, case-insensitive search)
 *
 * @param hh Pointer to HTTP headers list
 * @param name Header name to search for
 * @param out_value Pointer to store the time_t result
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NOT_FOUND on failure
 */
esp_err_t http_headers_get_time(const http_headers_t* hh, const char* name, time_t* out_value);

/**
 * @brief Set Bearer token authorization header
 *
 * @param hh Pointer to HTTP headers list
 * @param token Bearer token string
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t http_headers_set_bearer_token(http_headers_t* hh, const char* token);

/**
 * @brief Set Basic authorization header
 *
 * @param hh Pointer to HTTP headers list
 * @param username Username for authentication
 * @param password Password for authentication
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t http_headers_set_basic_auth(http_headers_t* hh, const char* username, const char* password);

/**
 * @brief Sort headers alphabetically by name (case-insensitive)
 *
 * @param hh Pointer to HTTP headers list
 */
void http_headers_sort(http_headers_t* hh);

/**
 * @brief Get header value by name (case-insensitive search)
 *
 * @param hh Pointer to HTTP headers list
 * @param name Header name to search for
 * @return Header value string or nullptr if not found
 */
const char* http_headers_get(const http_headers_t* hh, const char* name);

/**
 * @brief Remove header by name (case-insensitive search)
 *
 * @param hh Pointer to HTTP headers list
 * @param name Header name to remove
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NOT_FOUND on failure
 */
esp_err_t http_headers_remove(http_headers_t* hh, const char* name);

/**
 * @brief Convert headers to string format (for HTTP request/response)
 *
 * @param hh Pointer to HTTP headers list
 * @return Formatted header string (must be freed by caller) or nullptr on failure
 */
char* http_headers_to_string(const http_headers_t* hh);

/**
 * @brief Dump HTTP headers to log for debugging
 *
 * @param hh Pointer to HTTP headers list
 */
void http_headers_dump(const http_headers_t* hh);

/**
 * @brief Free HTTP headers list and all associated memory
 *
 * @param hh Pointer to HTTP headers list
 */
void http_headers_free(http_headers_t* hh);

/**
 * @brief HTTP protocol constants
 */
#define HTTP_PROTOCOL_HTTP  "http"
#define HTTP_PROTOCOL_HTTPS "https"
#define HTTP_PROTOCOL_FTP   "ftp"
#define HTTP_PROTOCOL_WS    "ws"
#define HTTP_PROTOCOL_WSS   "wss"

/**
 * @brief HTTP method enumeration
 */
typedef enum {
    E_HTTP_METHOD_GET,
    E_HTTP_METHOD_POST,
    E_HTTP_METHOD_PUT,
    E_HTTP_METHOD_DELETE,
    E_HTTP_METHOD_PATCH,
    E_HTTP_METHOD_HEAD,
    E_HTTP_METHOD_OPTIONS,
    E_HTTP_METHOD_CONNECT,
    E_HTTP_METHOD_TRACE
} http_method_t;

/**
 * @brief Convert HTTP method enum to string
 *
 * @param method HTTP method enum value
 * @return Method string or nullptr if invalid
 */
const char* http_method_to_string(http_method_t method);

/**
 * @brief Parse HTTP method string to enum
 *
 * @param method_str Method string (case-insensitive)
 * @return Parsed method enum, or HTTP_METHOD_GET if not found
 */
http_method_t http_method_from_string(const char* method_str);

/**
 * @brief Structure to hold URI components (path + query + fragment)
 */
typedef struct {
    char* path;
    query_params_t* params;
    char* fragment;
} uri_t;

/**
 * @brief Structure to hold URL components
 */
typedef struct {
    http_method_t method;  // HTTP method
    char* protocol;        // http, https, etc.
    char* host;
    int port;              // -1 if not specified
    char* path;
    query_params_t* params;
    char* fragment;
} url_t;

// ============================================================================
// URI Functions (path + query + fragment)
// ============================================================================

/**
 * @brief Initialize a new URI object
 *
 * @return Pointer to initialized uri_t structure or nullptr on allocation failure
 */
uri_t* uri_init(void);

/**
 * @brief Parse URI string into URI object
 *
 * @param uri_string URI string to parse (e.g., "/path?query=value#fragment")
 * @return Pointer to new uri_t structure or nullptr on failure
 */
uri_t* uri_parse(const char* uri_string);

/**
 * @brief Build URI string from URI object (without URL encoding)
 *
 * @param uri Pointer to URI object
 * @return URI string (must be freed by caller) or nullptr on failure
 */
char* uri_to_string(const uri_t* uri);

/**
 * @brief Build URI string from URI object with URL encoding
 *
 * @param uri Pointer to URI object
 * @return URL-encoded URI string (must be freed by caller) or nullptr on failure
 */
char* uri_encode(const uri_t* uri);

/**
 * @brief Set URI path
 *
 * @param uri Pointer to URI object
 * @param path Path string
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t uri_set_path(uri_t* uri, const char* path);

/**
 * @brief Set URI fragment
 *
 * @param uri Pointer to URI object
 * @param fragment Fragment string
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t uri_set_fragment(uri_t* uri, const char* fragment);

/**
 * @brief Get URI query parameters (direct access to query_params_t)
 *
 * @param uri Pointer to URI object
 * @return Pointer to query parameters list or nullptr if uri is nullptr
 */
query_params_t* uri_get_params(uri_t* uri);

/**
 * @brief Set URI query parameters (replaces existing params)
 *
 * @param uri Pointer to URI object
 * @param params Pointer to new query parameters list
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on failure
 */
esp_err_t uri_set_params(uri_t* uri, query_params_t* params);

/**
 * @brief Get URI path
 *
 * @param uri Pointer to URI object
 * @return Path string or nullptr if not set
 */
const char* uri_get_path(const uri_t* uri);

/**
 * @brief Get URI fragment
 *
 * @param uri Pointer to URI object
 * @return Fragment string or nullptr if not set
 */
const char* uri_get_fragment(const uri_t* uri);

/**
 * @brief Clone URI object (deep copy)
 *
 * @param uri Pointer to URI object to clone
 * @return Pointer to new uri_t structure or nullptr on failure
 */
uri_t* uri_clone(const uri_t* uri);

/**
 * @brief Merge query parameters from another URI into destination URI
 *
 * @param dest Pointer to destination URI object
 * @param src Pointer to source URI object
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on failure
 */
esp_err_t uri_merge_params(uri_t* dest, const uri_t* src);

/**
 * @brief Free URI object and all associated memory
 *
 * @param uri Pointer to URI object
 */
void uri_free(uri_t* uri);

// ============================================================================
// URL Functions (protocol + host + port + URI)
// ============================================================================

/**
 * @brief Initialize a new URL object
 *
 * @return Pointer to initialized url_t structure or nullptr on allocation failure
 */
url_t* url_init(void);

/**
 * @brief Parse URL string into URL object
 *
 * @param url_string URL string to parse
 * @return Pointer to new url_t structure or nullptr on failure
 */
url_t* url_parse(const char* url_string);

/**
 * @brief Build URL string from URL object (without URL encoding)
 *
 * @param url Pointer to URL object
 * @return URL string (must be freed by caller) or nullptr on failure
 */
char* url_to_string(const url_t* url);

/**
 * @brief Build URL string from URL object with URL encoding
 *
 * @param url Pointer to URL object
 * @return URL-encoded string (must be freed by caller) or nullptr on failure
 */
char* url_encode(const url_t* url);

/**
 * @brief Set URL HTTP method
 *
 * @param url Pointer to URL object
 * @param method HTTP method enum value
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on failure
 */
esp_err_t url_set_method(url_t* url, http_method_t method);

/**
 * @brief Set URL protocol
 *
 * @param url Pointer to URL object
 * @param protocol Protocol string (e.g., "http", "https")
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t url_set_protocol(url_t* url, const char* protocol);

/**
 * @brief Set URL host
 *
 * @param url Pointer to URL object
 * @param host Host string
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t url_set_host(url_t* url, const char* host);

/**
 * @brief Set URL port
 *
 * @param url Pointer to URL object
 * @param port Port number (-1 to clear)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on failure
 */
esp_err_t url_set_port(url_t* url, int port);

/**
 * @brief Set URL path
 *
 * @param url Pointer to URL object
 * @param path Path string
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t url_set_path(url_t* url, const char* path);

/**
 * @brief Set URL fragment
 *
 * @param url Pointer to URL object
 * @param fragment Fragment string
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure
 */
esp_err_t url_set_fragment(url_t* url, const char* fragment);

/**
 * @brief Get URL query parameters (direct access to query_params_t)
 *
 * @param url Pointer to URL object
 * @return Pointer to query parameters list or nullptr if url is nullptr
 */
query_params_t* url_get_params(url_t* url);

/**
 * @brief Set URL query parameters (replaces existing params)
 *
 * @param url Pointer to URL object
 * @param params Pointer to new query parameters list
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on failure
 */
esp_err_t url_set_params(url_t* url, query_params_t* params);

/**
 * @brief Get URL HTTP method
 *
 * @param url Pointer to URL object
 * @return HTTP method enum value
 */
http_method_t url_get_method(const url_t* url);

/**
 * @brief Get URL protocol
 *
 * @param url Pointer to URL object
 * @return Protocol string or nullptr if not set
 */
const char* url_get_protocol(const url_t* url);

/**
 * @brief Get URL host
 *
 * @param url Pointer to URL object
 * @return Host string or nullptr if not set
 */
const char* url_get_host(const url_t* url);

/**
 * @brief Get URL port
 *
 * @param url Pointer to URL object
 * @return Port number or -1 if not specified
 */
int url_get_port(const url_t* url);

/**
 * @brief Get URL path
 *
 * @param url Pointer to URL object
 * @return Path string or nullptr if not set
 */
const char* url_get_path(const url_t* url);

/**
 * @brief Get URL fragment
 *
 * @param url Pointer to URL object
 * @return Fragment string or nullptr if not set
 */
const char* url_get_fragment(const url_t* url);

/**
 * @brief Clone URL object (deep copy)
 *
 * @param url Pointer to URL object to clone
 * @return Pointer to new url_t structure or nullptr on failure
 */
url_t* url_clone(const url_t* url);

/**
 * @brief Merge query parameters from another URL into destination URL
 *
 * @param dest Pointer to destination URL object
 * @param src Pointer to source URL object
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on failure
 */
esp_err_t url_merge_params(url_t* dest, const url_t* src);

/**
 * @brief Create URL from URI and additional components
 *
 * @param protocol Protocol string (e.g., "http", "https")
 * @param host Host string
 * @param port Port number (-1 if not specified)
 * @param uri URI object containing path, params, and fragment
 * @return Pointer to new url_t structure or nullptr on failure
 */
url_t* url_from_uri(const char* protocol, const char* host, int port, const uri_t* uri);

/**
 * @brief Free URL object and all associated memory
 *
 * @param url Pointer to URL object
 */
void url_free(url_t* url);

#endif //HELPER_NET_TOOLS_H