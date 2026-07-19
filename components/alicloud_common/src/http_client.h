//
// Created by Ivan Kishchenko on 17/2/26.
//
// HTTP/HTTPS Client Library for ESP-IDF
//
// This library provides a simple HTTP/HTTPS client implementation with support for:
// - HTTP and HTTPS protocols
// - Custom headers and query parameters
// - Request/response management
// - Event-driven callbacks
// - TLS/SSL with certificate verification
// - Client certificate authentication (mutual TLS)
//
// Example usage (HTTPS):
//
//   // Configure client with TLS
//   http_client_config_t config = HTTP_CLIENT_CONFIG_DEFAULT();
//   config.cert_pem = server_cert_pem;  // Optional: CA certificate for verification
//   config.timeout_ms = 10000;
//   
//   http_client_t* client = http_client_init(&config);
//   
//   // Perform HTTPS GET request
//   http_response_t response;
//   http_response_init(&response);
//   
//   esp_err_t err = http_client_get(client, "https://api.example.com/data", &response);
//   if (err == ESP_OK) {
//       printf("Status: %d\n", response.status_code);
//       printf("Body: %s\n", response.body);
//   }
//   
//   http_response_free(&response);
//   http_client_cleanup(client);
//

#ifndef HELPER_HTTP_CLIENT_H
#define HELPER_HTTP_CLIENT_H

#include "net_tools.h"
#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

// Forward declarations
typedef struct http_client_s http_client_t;
typedef struct http_request_s http_request_t;
typedef struct http_response_s http_response_t;

// ============================================================================
// HTTP Client Events (ESP-IDF style)
// ============================================================================

/**
 * @brief HTTP client event types
 */
typedef enum {
    HTTP_EVENT_ERROR,              // Error occurred
    HTTP_EVENT_ON_CONNECTED,       // Connected to server
    HTTP_EVENT_HEADERS_SENT,       // Request headers sent
    HTTP_EVENT_ON_HEADER,          // Response header received
    HTTP_EVENT_ON_DATA,            // Response data chunk received
    HTTP_EVENT_ON_FINISH,          // Request completed successfully
    HTTP_EVENT_DISCONNECTED        // Disconnected from server
} http_event_id_t;

/**
 * @brief HTTP client event data
 */
typedef struct {
    http_event_id_t event_id;      // Event type
    http_client_t* client;         // Client instance
    void* user_data;               // User data from config
    
    // Event-specific data
    union {
        struct {
            const char* header_key;
            const char* header_value;
        } header;
        
        struct {
            const char* data;
            size_t data_len;
        } data;
        
        struct {
            esp_err_t error_code;
            const char* error_msg;
        } error;
    };
} http_event_t;

/**
 * @brief HTTP client event handler callback
 * 
 * @param evt Event data
 * @return ESP_OK to continue, error code to abort
 */
typedef esp_err_t (*http_event_handler_t)(http_event_t* evt);

// ============================================================================
// HTTP Request/Response Structures
// ============================================================================

/**
 * @brief HTTP request structure
 */
struct http_request_s {
    url_t* url;                    // Request URL (owns the pointer)
    http_headers_t* headers;       // Request headers (owns the pointer)
    const char* body;              // Request body (does not own, user manages)
    size_t body_len;               // Body length
};

/**
 * @brief HTTP response structure
 */
struct http_response_s {
    int status_code;               // HTTP status code
    http_headers_t* headers;       // Response headers (owns the pointer)
    char* body;                    // Response body (owns the pointer)
    size_t body_len;               // Body length
    size_t body_capacity;          // Allocated capacity
};

// ============================================================================
// HTTP Client Configuration
// ============================================================================

/**
 * @brief HTTP client configuration
 */
typedef struct {
    http_event_handler_t event_handler;  // Event callback
    void* user_data;                     // User data passed to callbacks
    
    // Timeouts (milliseconds)
    int timeout_ms;                      // Overall operation timeout
    int connect_timeout_ms;              // Connection timeout
    
    // Buffer settings
    size_t buffer_size;                  // Read buffer size
    size_t max_response_size;            // Max response body size (0 = unlimited)
    
    // Connection settings
    bool keep_alive;                     // Keep connection alive
    bool disable_auto_redirect;          // Disable automatic redirects
    int max_redirects;                   // Max redirect count
    
    // TLS/SSL settings
    bool use_global_ca_store;            // Use global CA certificate store
    const char* cert_pem;                // PEM-formatted certificate (nullptr to skip verification)
    size_t cert_len;                     // Certificate length (0 for null-terminated)
    const char* client_cert_pem;         // Client certificate for mutual authentication
    size_t client_cert_len;              // Client certificate length
    const char* client_key_pem;          // Client private key
    size_t client_key_len;               // Client key length
    bool skip_cert_common_name_check;    // Skip certificate common name check
} http_client_config_t;

/**
 * @brief Default HTTP client configuration
 */
#define HTTP_CLIENT_CONFIG_DEFAULT() { \
    .event_handler = NULL, \
    .user_data = NULL, \
    .timeout_ms = 30000, \
    .connect_timeout_ms = 10000, \
    .buffer_size = 4096, \
    .max_response_size = 0, \
    .keep_alive = true, \
    .disable_auto_redirect = false, \
    .max_redirects = 5 \
}

// ============================================================================
// HTTP Client Context (Opaque)
// ============================================================================

/**
 * @brief HTTP client context (internal structure)
 */
struct http_client_s {
    http_client_config_t config;   // Configuration
    http_request_t* request;       // Current request
    http_response_t* response;     // Current response
    
    // Connection state
    int socket_fd;                 // Socket file descriptor (-1 if closed)
    void* tls_handle;              // TLS handle (esp_tls_t*) for HTTPS
    bool is_connected;             // Connection status
    bool is_https;                 // HTTPS connection flag
    char* current_host;            // Current connected host
    int current_port;              // Current connected port
    
    // Internal buffers
    char* read_buffer;             // Read buffer
    size_t read_buffer_size;       // Buffer size
};

// ============================================================================
// High-Level API (perform)
// ============================================================================

/**
 * @brief Initialize HTTP client with configuration
 * 
 * @param config Client configuration
 * @return Client handle or nullptr on failure
 */
http_client_t* http_client_init(const http_client_config_t* config);

/**
 * @brief Perform HTTP request (high-level API)
 * 
 * This function handles the complete request lifecycle:
 * - Opens connection
 * - Sends request
 * - Receives response
 * - Closes connection (unless keep-alive)
 * 
 * @param client Client handle
 * @param request Request to perform
 * @param response Response structure to fill (allocated by caller)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_perform(http_client_t* client, 
                              http_request_t* request,
                              http_response_t* response);

/**
 * @brief Perform HTTP GET request (convenience function)
 * 
 * @param client Client handle
 * @param url URL string
 * @param response Response structure to fill
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_get(http_client_t* client,
                          const char* url,
                          http_response_t* response);

/**
 * @brief Perform HTTP POST request (convenience function)
 * 
 * @param client Client handle
 * @param url URL string
 * @param body Request body
 * @param body_len Body length
 * @param response Response structure to fill
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_post(http_client_t* client,
                           const char* url,
                           const char* body,
                           size_t body_len,
                           http_response_t* response);

// ============================================================================
// Low-Level API (open/send/recv/close)
// ============================================================================

/**
 * @brief Open connection to server (low-level API)
 * 
 * @param client Client handle
 * @param url Target URL
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_open(http_client_t* client, const url_t* url);

/**
 * @brief Send only HTTP headers without body (low-level API for streaming)
 * 
 * This function sends request line and headers without body.
 * Used for streaming uploads where body will be sent separately.
 * Connection must be opened first with http_client_open()
 * 
 * @param client Client handle
 * @param request Request with headers to send
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_send_headers(http_client_t* client, const http_request_t* request);

/**
 * @brief Send raw data through connection (low-level API for streaming)
 * 
 * This function sends data directly without HTTP framing.
 * Used for streaming body data after headers have been sent.
 * Connection must be opened first with http_client_open()
 * 
 * @param client Client handle
 * @param data Data to send
 * @param data_len Data length
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_send_data(http_client_t* client, const char* data, size_t data_len);

/**
 * @brief Send HTTP request (low-level API)
 * 
 * Connection must be opened first with http_client_open()
 * 
 * @param client Client handle
 * @param request Request to send
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_send_request(http_client_t* client, 
                                   const http_request_t* request);

/**
 * @brief Receive HTTP response headers (low-level API)
 * 
 * @param client Client handle
 * @param response Response structure to fill
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_recv_headers(http_client_t* client,
                                   http_response_t* response);

/**
 * @brief Receive HTTP response body chunk (low-level API)
 * 
 * Call repeatedly until returns 0 bytes read or error
 * 
 * @param client Client handle
 * @param buffer Buffer to receive data
 * @param buffer_size Buffer size
 * @param bytes_read Number of bytes actually read
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_recv_data(http_client_t* client,
                               char* buffer,
                               size_t buffer_size,
                               size_t* bytes_read);

/**
 * @brief Close connection (low-level API)
 * 
 * @param client Client handle
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_close(http_client_t* client);

/**
 * @brief Check if client is connected
 * 
 * @param client Client handle
 * @return true if connected, false otherwise
 */
bool http_client_is_connected(const http_client_t* client);

// ============================================================================
// Request/Response Management
// ============================================================================

/**
 * @brief Create HTTP request
 * 
 * @param url Request URL (takes ownership)
 * @return Request handle or nullptr on failure
 */
http_request_t* http_request_create(url_t* url);

/**
 * @brief Set request body
 * 
 * @param request Request handle
 * @param body Body data (does not take ownership)
 * @param body_len Body length
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_request_set_body(http_request_t* request,
                               const char* body,
                               size_t body_len);

/**
 * @brief Get request headers (for modification)
 * 
 * @param request Request handle
 * @return Headers handle or nullptr
 */
http_headers_t* http_request_get_headers(http_request_t* request);

/**
 * @brief Free HTTP request
 * 
 * @param request Request handle
 */
void http_request_free(http_request_t* request);

/**
 * @brief Initialize HTTP response structure
 * 
 * @param response Response structure to initialize
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_response_init(http_response_t* response);

/**
 * @brief Get response body as string (null-terminated)
 * 
 * @param response Response handle
 * @return Body string or nullptr
 */
const char* http_response_get_body(const http_response_t* response);

/**
 * @brief Get response body length
 * 
 * @param response Response handle
 * @return Body length in bytes
 */
size_t http_response_get_body_len(const http_response_t* response);

/**
 * @brief Get response status code
 * 
 * @param response Response handle
 * @return HTTP status code
 */
int http_response_get_status(const http_response_t* response);

/**
 * @brief Get response headers
 * 
 * @param response Response handle
 * @return Headers handle or nullptr
 */
http_headers_t* http_response_get_headers(http_response_t* response);

/**
 * @brief Clear response data (for reuse)
 * 
 * @param response Response structure to clear
 */
void http_response_clear(http_response_t* response);

/**
 * @brief Free HTTP response resources
 * 
 * @param response Response structure to free
 */
void http_response_free(http_response_t* response);

/**
 * @brief Cleanup HTTP client and free resources
 * 
 * @param client Client handle
 */
void http_client_cleanup(http_client_t* client);

#endif //HELPER_HTTP_CLIENT_H