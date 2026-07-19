//
// Created by Ivan Kishchenko on 17/2/26.
//

#include "http_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <esp_crt_bundle.h>

#include "esp_tls.h"
#include "esp_log.h"

static const char* TAG = "http_client";

// ============================================================================
// Internal Helper Functions
// ============================================================================

// Dispatch event to user callback
static esp_err_t dispatch_event(http_client_t* client, http_event_t* evt) {
    if (!client || !evt) return ESP_ERR_INVALID_ARG;
    
    evt->client = client;
    evt->user_data = client->config.user_data;
    
    if (client->config.event_handler) {
        return client->config.event_handler(evt);
    }
    
    return ESP_OK;
}

// Dispatch error event
static void dispatch_error(http_client_t* client, esp_err_t error_code, const char* error_msg) {
    http_event_t evt = {
        .event_id = HTTP_EVENT_ERROR,
        .error = {
            .error_code = error_code,
            .error_msg = error_msg
        }
    };
    dispatch_event(client, &evt);
}

// Connect to server via socket (HTTP only)
static esp_err_t connect_to_host(const char* host, int port, int timeout_ms, int* out_fd) {
    if (!host || !out_fd) return ESP_ERR_INVALID_ARG;
    
    // Resolve hostname
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    struct addrinfo* result = nullptr;
    int ret = getaddrinfo(host, port_str, &hints, &result);
    if (ret != 0) {
        return ESP_FAIL;
    }
    
    // Try to connect
    int sock_fd = -1;
    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock_fd == -1) continue;
        
        // Set timeout
        if (timeout_ms > 0) {
            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        }
        
        if (connect(sock_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; // Success
        }
        
        close(sock_fd);
        sock_fd = -1;
    }
    
    freeaddrinfo(result);
    
    if (sock_fd == -1) {
        ESP_LOGE(TAG, "Failed to connect to %s:%d", host, port);
        return ESP_FAIL;
    }
    
    *out_fd = sock_fd;
    return ESP_OK;
}

// Connect to server via TLS (HTTPS)
static esp_err_t connect_to_host_tls(http_client_t* client, const char* host, int port, esp_tls_t** out_tls) {
    if (!client || !host || !out_tls) return ESP_ERR_INVALID_ARG;
    
    esp_tls_cfg_t tls_cfg = {
        .timeout_ms = client->config.connect_timeout_ms,
        .skip_common_name = client->config.skip_cert_common_name_check,
    };
    
    // Configure certificate verification
    if (!client->config.skip_cert_common_name_check) {
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        // Use ESP-IDF certificate bundle (Mozilla root CA certificates)
        tls_cfg.crt_bundle_attach = esp_crt_bundle_attach;
        ESP_LOGI(TAG, "Using certificate bundle for TLS verification");
#else
        ESP_LOGW(TAG, "Certificate bundle not enabled, skipping cert verification");
        tls_cfg.skip_common_name = true;
#endif
    }
    
    // Set CA certificate if provided
    if (client->config.cert_pem) {
        tls_cfg.cacert_buf = (const unsigned char*)client->config.cert_pem;
        tls_cfg.cacert_bytes = client->config.cert_len > 0 ? 
                               client->config.cert_len : 
                               strlen(client->config.cert_pem) + 1;
    }
    
    // Set client certificate for mutual authentication
    if (client->config.client_cert_pem) {
        tls_cfg.clientcert_buf = (const unsigned char*)client->config.client_cert_pem;
        tls_cfg.clientcert_bytes = client->config.client_cert_len > 0 ?
                                   client->config.client_cert_len :
                                   strlen(client->config.client_cert_pem) + 1;
    }
    
    // Set client private key
    if (client->config.client_key_pem) {
        tls_cfg.clientkey_buf = (const unsigned char*)client->config.client_key_pem;
        tls_cfg.clientkey_bytes = client->config.client_key_len > 0 ?
                                  client->config.client_key_len :
                                  strlen(client->config.client_key_pem) + 1;
    }
    
    esp_tls_t* tls = esp_tls_init();
    if (!tls) {
        ESP_LOGE(TAG, "Failed to initialize TLS");
        return ESP_ERR_NO_MEM;
    }
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    int ret = esp_tls_conn_new_sync(host, strlen(host), port, &tls_cfg, tls);
    if (ret != 1) {
        ESP_LOGE(TAG, "TLS connection failed to %s:%d (error: 0x%x)", host, port, ret);
        esp_tls_conn_destroy(tls);
        return ESP_FAIL;
    }
    
    *out_tls = tls;
    return ESP_OK;
}

// Send data through socket or TLS
static esp_err_t client_send(http_client_t* client, const char* data, size_t len) {
    if (!client || !data || len == 0) return ESP_ERR_INVALID_ARG;
    
    if (client->is_https && client->tls_handle) {
        // TLS send with retry limit to prevent infinite loops
        esp_tls_t* tls = (esp_tls_t*)client->tls_handle;
        size_t sent = 0;
        int retry_count = 0;
        const int MAX_RETRIES = 10;  // Maximum retries for WANT_WRITE
        
        while (sent < len) {
            ssize_t ret = esp_tls_conn_write(tls, data + sent, len - sent);
            if (ret < 0) {
                if (ret == ESP_TLS_ERR_SSL_WANT_WRITE || ret == ESP_TLS_ERR_SSL_WANT_READ) {
                    retry_count++;
                    if (retry_count >= MAX_RETRIES) {
                        ESP_LOGE(TAG, "TLS write timeout after %d retries (sent %zu/%zu bytes)", 
                                 MAX_RETRIES, sent, len);
                        return ESP_ERR_TIMEOUT;
                    }
                    // Small delay before retry
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
                ESP_LOGE(TAG, "TLS write failed: 0x%x (sent %zu/%zu bytes)", ret, sent, len);
                return ESP_FAIL;
            }
            sent += ret;
            retry_count = 0;  // Reset retry counter on successful write
        }
    } else {
        // Plain socket send with retry limit
        if (client->socket_fd < 0) return ESP_ERR_INVALID_STATE;
        
        size_t sent = 0;
        int retry_count = 0;
        const int MAX_RETRIES = 10;
        
        while (sent < len) {
            ssize_t ret = send(client->socket_fd, data + sent, len - sent, 0);
            if (ret < 0) {
                if (errno == EINTR) {
                    retry_count++;
                    if (retry_count >= MAX_RETRIES) {
                        ESP_LOGE(TAG, "Socket send timeout after %d retries (sent %zu/%zu bytes)", 
                                 MAX_RETRIES, sent, len);
                        return ESP_ERR_TIMEOUT;
                    }
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
                ESP_LOGE(TAG, "Socket send failed: %s (sent %zu/%zu bytes)", strerror(errno), sent, len);
                return ESP_FAIL;
            }
            sent += ret;
            retry_count = 0;  // Reset on successful send
        }
    }
    
    return ESP_OK;
}

// Receive data from socket or TLS
static esp_err_t client_recv(http_client_t* client, char* buffer, size_t buffer_size, size_t* bytes_read) {
    if (!client || !buffer || !bytes_read) return ESP_ERR_INVALID_ARG;
    
    if (client->is_https && client->tls_handle) {
        // TLS receive
        esp_tls_t* tls = (esp_tls_t*)client->tls_handle;
        ssize_t ret = esp_tls_conn_read(tls, buffer, buffer_size);
        if (ret < 0) {
            if (ret == ESP_TLS_ERR_SSL_WANT_READ || ret == ESP_TLS_ERR_SSL_WANT_WRITE) {
                *bytes_read = 0;
                return ESP_OK;
            }
            ESP_LOGE(TAG, "TLS read failed: 0x%x", ret);
            return ESP_FAIL;
        }
        *bytes_read = ret;
    } else {
        // Plain socket receive
        if (client->socket_fd < 0) return ESP_ERR_INVALID_STATE;
        
        ssize_t ret = recv(client->socket_fd, buffer, buffer_size, 0);
        if (ret < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                *bytes_read = 0;
                return ESP_OK;
            }
            ESP_LOGE(TAG, "Socket recv failed: %s", strerror(errno));
            return ESP_FAIL;
        }
        *bytes_read = ret;
    }
    
    return ESP_OK;
}

// Read line from socket or TLS (until \r\n)
static esp_err_t client_read_line(http_client_t* client, char* buffer, size_t buffer_size, size_t* line_len) {
    if (!client || !buffer || !line_len) return ESP_ERR_INVALID_ARG;
    
    size_t pos = 0;
    int retry_count = 0;
    const int MAX_RETRIES = 10;  // Maximum retries for WANT_READ/WRITE
    
    while (pos < buffer_size - 1) {
        char c;
        
        if (client->is_https && client->tls_handle) {
            // TLS read one byte
            esp_tls_t* tls = (esp_tls_t*)client->tls_handle;
            ssize_t ret = esp_tls_conn_read(tls, &c, 1);
            if (ret <= 0) {
                if (ret < 0 && (ret == ESP_TLS_ERR_SSL_WANT_READ || ret == ESP_TLS_ERR_SSL_WANT_WRITE)) {
                    retry_count++;
                    if (retry_count >= MAX_RETRIES) {
                        ESP_LOGE(TAG, "TLS read line timeout after %d retries (read %zu bytes)", 
                                 MAX_RETRIES, pos);
                        return ESP_ERR_TIMEOUT;
                    }
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
                ESP_LOGE(TAG, "TLS read line failed: 0x%x (read %zu bytes)", ret, pos);
                return ESP_FAIL;
            }
            retry_count = 0;  // Reset on successful read
        } else {
            // Plain socket read one byte
            if (client->socket_fd < 0) return ESP_ERR_INVALID_STATE;
            
            ssize_t ret = recv(client->socket_fd, &c, 1, 0);
            if (ret <= 0) {
                if (ret < 0 && errno == EINTR) {
                    retry_count++;
                    if (retry_count >= MAX_RETRIES) {
                        ESP_LOGE(TAG, "Socket read line timeout after %d retries (read %zu bytes)", 
                                 MAX_RETRIES, pos);
                        return ESP_ERR_TIMEOUT;
                    }
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
                ESP_LOGE(TAG, "Socket read line failed: %s (read %zu bytes)", strerror(errno), pos);
                return ESP_FAIL;
            }
            retry_count = 0;  // Reset on successful read
        }
        
        buffer[pos++] = c;
        
        // Check for \r\n
        if (pos >= 2 && buffer[pos-2] == '\r' && buffer[pos-1] == '\n') {
            buffer[pos-2] = '\0';
            *line_len = pos - 2;
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "Line too long (>%zu bytes)", buffer_size);
    return ESP_ERR_NO_MEM; // Line too long
}

// Parse HTTP status line
static esp_err_t parse_status_line(const char* line, int* status_code) {
    if (!line || !status_code) return ESP_ERR_INVALID_ARG;
    
    // Format: HTTP/1.1 200 OK
    int code;
    if (sscanf(line, "HTTP/%*d.%*d %d", &code) == 1) {
        *status_code = code;
        return ESP_OK;
    }
    
    return ESP_FAIL;
}

// ============================================================================
// Request/Response Management
// ============================================================================

http_request_t* http_request_create(url_t* url) {
    if (!url) return nullptr;
    
    http_request_t* request = malloc(sizeof(http_request_t));
    if (!request) return nullptr;
    
    request->url = url;
    request->headers = http_headers_init();
    request->body = nullptr;
    request->body_len = 0;
    
    if (!request->headers) {
        free(request);
        return nullptr;
    }
    
    return request;
}

esp_err_t http_request_set_body(http_request_t* request, const char* body, size_t body_len) {
    if (!request) return ESP_ERR_INVALID_ARG;
    
    request->body = body;
    request->body_len = body_len;
    
    return ESP_OK;
}

http_headers_t* http_request_get_headers(http_request_t* request) {
    return request ? request->headers : nullptr;
}

void http_request_free(http_request_t* request) {
    if (!request) return;
    
    url_free(request->url);
    http_headers_free(request->headers);
    free(request);
}

esp_err_t http_response_init(http_response_t* response) {
    if (!response) return ESP_ERR_INVALID_ARG;
    
    response->status_code = 0;
    response->headers = http_headers_init();
    response->body = nullptr;
    response->body_len = 0;
    response->body_capacity = 0;
    
    return response->headers ? ESP_OK : ESP_ERR_NO_MEM;
}

const char* http_response_get_body(const http_response_t* response) {
    return response ? response->body : nullptr;
}

size_t http_response_get_body_len(const http_response_t* response) {
    return response ? response->body_len : 0;
}

int http_response_get_status(const http_response_t* response) {
    return response ? response->status_code : 0;
}

http_headers_t* http_response_get_headers(http_response_t* response) {
    return response ? response->headers : nullptr;
}

void http_response_clear(http_response_t* response) {
    if (!response) return;
    
    response->status_code = 0;
    response->body_len = 0;
    
    if (response->headers) {
        http_headers_free(response->headers);
        response->headers = http_headers_init();
    }
}

void http_response_free(http_response_t* response) {
    if (!response) return;
    
    http_headers_free(response->headers);
    free(response->body);
}

// ============================================================================
// HTTP Client Implementation
// ============================================================================

http_client_t* http_client_init(const http_client_config_t* config) {
    http_client_t* client = malloc(sizeof(http_client_t));
    if (!client) return nullptr;
    
    // Copy config or use defaults
    if (config) {
        client->config = *config;
    } else {
        http_client_config_t default_config = HTTP_CLIENT_CONFIG_DEFAULT();
        client->config = default_config;
    }
    
    // Initialize state
    client->request = nullptr;
    client->response = nullptr;
    client->socket_fd = -1;
    client->tls_handle = nullptr;
    client->is_connected = false;
    client->is_https = false;
    client->current_host = nullptr;
    client->current_port = -1;
    
    // Allocate read buffer
    client->read_buffer_size = client->config.buffer_size > 0 ? 
                               client->config.buffer_size : 4096;
    client->read_buffer = malloc(client->read_buffer_size);
    
    if (!client->read_buffer) {
        free(client);
        return nullptr;
    }
    
    return client;
}

esp_err_t http_client_open(http_client_t* client, const url_t* url) {
    if (!client || !url) return ESP_ERR_INVALID_ARG;
    
    const char* host = url_get_host(url);
    int port = url_get_port(url);
    const char* protocol = url_get_protocol(url);
    
    if (!host) return ESP_ERR_INVALID_ARG;
    
    // Determine if HTTPS
    bool is_https = (protocol && strcmp(protocol, "https") == 0);
    
    // Use default ports if not specified
    if (port <= 0) {
        port = is_https ? 443 : 80;
    }
    
    // Check if already connected to same host with same protocol
    if (client->is_connected && client->current_host && 
        strcmp(client->current_host, host) == 0 && 
        client->current_port == port &&
        client->is_https == is_https) {
        return ESP_OK; // Already connected
    }
    
    // Close existing connection
    if (client->is_connected) {
        http_client_close(client);
    }
    
    esp_err_t err;
    
    if (is_https) {
        // HTTPS connection
        esp_tls_t* tls = nullptr;
        err = connect_to_host_tls(client, host, port, &tls);
        if (err != ESP_OK) {
            dispatch_error(client, err, "Failed to establish TLS connection");
            return err;
        }
        
        client->tls_handle = tls;
        client->is_https = true;
        client->socket_fd = -1; // Not using raw socket
        
        ESP_LOGI(TAG, "HTTPS connection established to %s:%d", host, port);
    } else {
        // HTTP connection
        int sock_fd;
        err = connect_to_host(host, port, client->config.connect_timeout_ms, &sock_fd);
        if (err != ESP_OK) {
            dispatch_error(client, err, "Failed to connect to server");
            return err;
        }
        
        client->socket_fd = sock_fd;
        client->is_https = false;
        client->tls_handle = nullptr;
        
        ESP_LOGI(TAG, "HTTP connection established to %s:%d", host, port);
    }
    
    client->is_connected = true;
    client->current_host = strdup(host);
    client->current_port = port;
    
    // Dispatch connected event
    http_event_t evt = { .event_id = HTTP_EVENT_ON_CONNECTED };
    dispatch_event(client, &evt);
    
    return ESP_OK;
}

/**
 * @brief Send only HTTP headers without body (for streaming uploads)
 * 
 * This function sends request line and headers without body.
 * Used for streaming uploads where body will be sent separately.
 * 
 * @param client Client handle
 * @param request Request with headers to send
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_send_headers(http_client_t* client, const http_request_t* request) {
    if (!client || !request || !client->is_connected) {
        ESP_LOGE(TAG, "Invalid arguments: client=%p, request=%p, connected=%d", 
                 client, request, client ? client->is_connected : 0);
        return ESP_ERR_INVALID_ARG;
    }
    
    const url_t* url = request->url;
    if (!url) {
        ESP_LOGE(TAG, "Request URL is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Build request line
    const char* method = http_method_to_string(url_get_method(url));
    const char* path = url_get_path(url);
    
    if (!method) method = "GET";
    if (!path) path = "/";
    
    // Build query string
    char* query_string = nullptr;
    query_params_t* params = url_get_params((url_t*)url);
    if (params && query_params_count(params) > 0) {
        query_string = query_params_encode(params);
    }
    
    // Send request line
    char request_line[2048];
    if (query_string) {
        snprintf(request_line, sizeof(request_line), "%s %s?%s HTTP/1.1\r\n", 
                method, path, query_string);
        free(query_string);
    } else {
        snprintf(request_line, sizeof(request_line), "%s %s HTTP/1.1\r\n", 
                method, path);
    }
    
    esp_err_t err = client_send(client, request_line, strlen(request_line));
    if (err != ESP_OK) {
        dispatch_error(client, err, "Failed to send request line");
        return err;
    }
    
    // Send headers
    http_headers_t* headers = request->headers;
    char* headers_str = http_headers_to_string(headers);
    if (headers_str) {
        err = client_send(client, headers_str, strlen(headers_str));
        free(headers_str);
        if (err != ESP_OK) {
            dispatch_error(client, err, "Failed to send headers");
            return err;
        }
    }
    
    // Send empty line to end headers
    err = client_send(client, "\r\n", 2);
    if (err != ESP_OK) {
        dispatch_error(client, err, "Failed to send headers terminator");
        return err;
    }
    
    // Dispatch headers sent event
    http_event_t evt = { .event_id = HTTP_EVENT_HEADERS_SENT };
    dispatch_event(client, &evt);
    
    return ESP_OK;
}

/**
 * @brief Send raw data through connection (for streaming uploads)
 * 
 * This function sends data directly without HTTP framing.
 * Used for streaming body data after headers have been sent.
 * 
 * @param client Client handle
 * @param data Data to send
 * @param data_len Data length
 * @return ESP_OK on success, error code on failure
 */
esp_err_t http_client_send_data(http_client_t* client, const char* data, size_t data_len) {
    if (!client || !data || data_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!client->is_connected) {
        ESP_LOGE(TAG, "Client not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    return client_send(client, data, data_len);
}

esp_err_t http_client_send_request(http_client_t* client, const http_request_t* request) {
    if (!client || !request || !client->is_connected) return ESP_ERR_INVALID_ARG;
    
    const url_t* url = request->url;
    if (!url) return ESP_ERR_INVALID_ARG;
    
    // Build request line
    const char* method = http_method_to_string(url_get_method(url));
    const char* path = url_get_path(url);
    
    if (!method) method = "GET";
    if (!path) path = "/";
    
    // Build query string
    char* query_string = nullptr;
    query_params_t* params = url_get_params((url_t*)url);
    if (params && query_params_count(params) > 0) {
        query_string = query_params_encode(params);
    }
    
    // Send request line
    char request_line[2048];
    if (query_string) {
        snprintf(request_line, sizeof(request_line), "%s %s?%s HTTP/1.1\r\n", 
                method, path, query_string);
        free(query_string);
    } else {
        snprintf(request_line, sizeof(request_line), "%s %s HTTP/1.1\r\n", 
                method, path);
    }
    
    esp_err_t err = client_send(client, request_line, strlen(request_line));
    if (err != ESP_OK) {
        dispatch_error(client, err, "Failed to send request line");
        return err;
    }
    
    // Add required headers if not present
    http_headers_t* headers = request->headers;
    if (!http_headers_get(headers, "Host")) {
        const char* host = url_get_host(url);
        if (host) {
            http_headers_set(headers, "Host", host);
        }
    }
    
    if (!http_headers_get(headers, "User-Agent")) {
        http_headers_set(headers, "User-Agent", "HTTP-Client/1.0");
    }
    
    if (!http_headers_get(headers, "Connection")) {
        http_headers_set(headers, "Connection", 
                        client->config.keep_alive ? "keep-alive" : "close");
    }
    
    if (request->body && request->body_len > 0) {
        if (!http_headers_get(headers, "Content-Length")) {
            http_headers_add_int(headers, "Content-Length", request->body_len);
        }
    }
    
    // Send headers
    char* headers_str = http_headers_to_string(headers);
    if (headers_str) {
        err = client_send(client, headers_str, strlen(headers_str));
        free(headers_str);
        if (err != ESP_OK) {
            dispatch_error(client, err, "Failed to send headers");
            return err;
        }
    }
    
    // Send empty line
    err = client_send(client, "\r\n", 2);
    if (err != ESP_OK) {
        dispatch_error(client, err, "Failed to send headers terminator");
        return err;
    }
    
    // Dispatch headers sent event
    http_event_t evt = { .event_id = HTTP_EVENT_HEADERS_SENT };
    dispatch_event(client, &evt);
    
    // Send body if present
    if (request->body && request->body_len > 0) {
        err = client_send(client, request->body, request->body_len);
        if (err != ESP_OK) {
            dispatch_error(client, err, "Failed to send body");
            return err;
        }
    }
    
    return ESP_OK;
}

esp_err_t http_client_recv_headers(http_client_t* client, http_response_t* response) {
    if (!client || !response || !client->is_connected) return ESP_ERR_INVALID_ARG;
    
    char line_buffer[2048];
    size_t line_len;
    
    // Read status line
    esp_err_t err = client_read_line(client, line_buffer, 
                                     sizeof(line_buffer), &line_len);
    if (err != ESP_OK) {
        dispatch_error(client, err, "Failed to read status line");
        return err;
    }
    
    // Parse status code
    int status_code;
    err = parse_status_line(line_buffer, &status_code);
    if (err != ESP_OK) {
        dispatch_error(client, err, "Failed to parse status line");
        return err;
    }
    
    response->status_code = status_code;
    ESP_LOGI(TAG, "Response status: %d", status_code);
    
    // Read headers
    while (true) {
        err = client_read_line(client, line_buffer, 
                              sizeof(line_buffer), &line_len);
        if (err != ESP_OK) {
            dispatch_error(client, err, "Failed to read header line");
            return err;
        }
        
        // Empty line marks end of headers
        if (line_len == 0) break;
        
        // Parse header (Name: Value)
        char* colon = strchr(line_buffer, ':');
        if (colon) {
            *colon = '\0';
            char* name = line_buffer;
            char* value = colon + 1;
            
            // Skip leading whitespace in value
            while (*value == ' ' || *value == '\t') value++;
            
            http_headers_add(response->headers, name, value);
            
            // Dispatch header event
            http_event_t evt = {
                .event_id = HTTP_EVENT_ON_HEADER,
                .header = {
                    .header_key = name,
                    .header_value = value
                }
            };
            dispatch_event(client, &evt);
        }
    }
    
    return ESP_OK;
}

esp_err_t http_client_recv_data(http_client_t* client, char* buffer, 
                                size_t buffer_size, size_t* bytes_read) {
    if (!client || !buffer || !bytes_read || !client->is_connected) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t err = client_recv(client, buffer, buffer_size, bytes_read);
    if (err != ESP_OK) {
        dispatch_error(client, err, "Failed to receive data");
        return err;
    }
    
    // Dispatch data event if data received
    if (*bytes_read > 0) {
        http_event_t evt = {
            .event_id = HTTP_EVENT_ON_DATA,
            .data = {
                .data = buffer,
                .data_len = *bytes_read
            }
        };
        dispatch_event(client, &evt);
    }
    
    return ESP_OK;
}

esp_err_t http_client_close(http_client_t* client) {
    if (!client) return ESP_ERR_INVALID_ARG;
    
    if (client->is_https && client->tls_handle) {
        // Close TLS connection
        esp_tls_t* tls = (esp_tls_t*)client->tls_handle;
        esp_tls_conn_destroy(tls);
        client->tls_handle = nullptr;
        ESP_LOGI(TAG, "TLS connection closed");
    }
    
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
        client->socket_fd = -1;
    }
    
    client->is_connected = false;
    client->is_https = false;
    free(client->current_host);
    client->current_host = nullptr;
    client->current_port = -1;
    
    // Dispatch disconnected event
    http_event_t evt = { .event_id = HTTP_EVENT_DISCONNECTED };
    dispatch_event(client, &evt);
    
    return ESP_OK;
}

bool http_client_is_connected(const http_client_t* client) {
    return client && client->is_connected;
}

esp_err_t http_client_perform(http_client_t* client, http_request_t* request, 
                              http_response_t* response) {
    if (!client || !request || !response) return ESP_ERR_INVALID_ARG;
    
    esp_err_t err = ESP_OK;
    
    // Open connection
    err = http_client_open(client, request->url);
    if (err != ESP_OK) return err;
    
    // Send request
    err = http_client_send_request(client, request);
    if (err != ESP_OK) {
        http_client_close(client);
        return err;
    }
    
    // Receive headers
    err = http_client_recv_headers(client, response);
    if (err != ESP_OK) {
        http_client_close(client);
        return err;
    }
    
    // Get Content-Length if available
    int content_length = -1;
    http_headers_get_int(response->headers, "Content-Length", &content_length);
    
    // Check max response size
    if (client->config.max_response_size > 0 && content_length > 0 &&
        (size_t)content_length > client->config.max_response_size) {
        http_client_close(client);
        dispatch_error(client, ESP_ERR_NO_MEM, "Response too large");
        return ESP_ERR_NO_MEM;
    }
    
    // Allocate response body buffer
    size_t initial_capacity = content_length > 0 ? content_length + 1 : 4096;
    response->body = malloc(initial_capacity);
    if (!response->body) {
        http_client_close(client);
        return ESP_ERR_NO_MEM;
    }
    response->body_capacity = initial_capacity;
    response->body_len = 0;
    
    // Receive body
    while (true) {
        // Ensure buffer has space
        if (response->body_len + client->read_buffer_size >= response->body_capacity) {
            size_t new_capacity = response->body_capacity * 2;
            
            // Check max size
            if (client->config.max_response_size > 0 && 
                new_capacity > client->config.max_response_size) {
                http_client_close(client);
                dispatch_error(client, ESP_ERR_NO_MEM, "Response too large");
                return ESP_ERR_NO_MEM;
            }
            
            char* new_body = (char*)realloc(response->body, new_capacity);
            if (!new_body) {
                http_client_close(client);
                return ESP_ERR_NO_MEM;
            }
            response->body = new_body;
            response->body_capacity = new_capacity;
        }
        
        // Read chunk
        size_t bytes_read;
        err = http_client_recv_data(client, 
                                    response->body + response->body_len,
                                    client->read_buffer_size, 
                                    &bytes_read);
        if (err != ESP_OK) {
            http_client_close(client);
            return err;
        }
        
        if (bytes_read == 0) break; // End of data
        
        response->body_len += bytes_read;
        
        // Check if we have all data (if Content-Length was specified)
        if (content_length > 0 && response->body_len >= (size_t)content_length) {
            break;
        }
    }
    
    // Null-terminate body
    response->body[response->body_len] = '\0';
    
    // Dispatch finish event
    http_event_t evt = { .event_id = HTTP_EVENT_ON_FINISH };
    dispatch_event(client, &evt);
    
    // Close connection if not keep-alive
    if (!client->config.keep_alive) {
        http_client_close(client);
    }
    
    return ESP_OK;
}

esp_err_t http_client_get(http_client_t* client, const char* url_str, 
                          http_response_t* response) {
    if (!client || !url_str || !response) return ESP_ERR_INVALID_ARG;
    
    url_t* url = url_parse(url_str);
    if (!url) return ESP_ERR_INVALID_ARG;
    
    url_set_method(url, E_HTTP_METHOD_GET);
    
    http_request_t* request = http_request_create(url);
    if (!request) {
        url_free(url);
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t err = http_client_perform(client, request, response);
    
    http_request_free(request);
    
    return err;
}

esp_err_t http_client_post(http_client_t* client, const char* url_str,
                           const char* body, size_t body_len,
                           http_response_t* response) {
    if (!client || !url_str || !response) return ESP_ERR_INVALID_ARG;
    
    url_t* url = url_parse(url_str);
    if (!url) return ESP_ERR_INVALID_ARG;
    
    url_set_method(url, E_HTTP_METHOD_POST);
    
    http_request_t* request = http_request_create(url);
    if (!request) {
        url_free(url);
        return ESP_ERR_NO_MEM;
    }
    
    if (body && body_len > 0) {
        http_request_set_body(request, body, body_len);
    }
    
    esp_err_t err = http_client_perform(client, request, response);
    
    http_request_free(request);
    
    return err;
}

void http_client_cleanup(http_client_t* client) {
    if (!client) return;
    
    http_client_close(client);
    
    free(client->read_buffer);
    free(client->current_host);
    free(client);
}