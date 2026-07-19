//
// Created by Ivan Kishchenko on 16/2/26.
//

#include "alicloud_oss_client.h"
#include "alicloud_oss_auth.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "OSS_CLIENT";
static const char* ENDPOINT_TEMPLATE = "%s.oss-%s.aliyuncs.com";

esp_err_t alicloud_oss_create(alicloud_oss_config_t* config, alicloud_oss_handler_t* handler) {
    if (!config || !handler) {
        return ESP_ERR_INVALID_ARG;
    }

    alicloud_oss_client_t* client = malloc(sizeof(alicloud_oss_client_t));
    if (!client) {
        return ESP_ERR_NO_MEM;
    }

    memset(client, 0, sizeof(alicloud_oss_client_t));
    client->config = *config;

    // Initialize HTTP client with HTTPS support
    http_client_config_t http_config = HTTP_CLIENT_CONFIG_DEFAULT();
    http_config.use_global_ca_store = true;
    http_config.timeout_ms = 30000;
    http_config.keep_alive = true;
    
    client->client = http_client_init(&http_config);
    if (!client->client) {
        free(client);
        return ESP_ERR_NO_MEM;
    }

    // Build endpoint URL
    int req_size = snprintf(nullptr, 0, ENDPOINT_TEMPLATE, config->bucket, config->region);
    client->endpoint = malloc(req_size + 1);
    if (!client->endpoint) {
        http_client_cleanup(client->client);
        free(client);
        return ESP_ERR_NO_MEM;
    }
    snprintf(client->endpoint, req_size + 1, ENDPOINT_TEMPLATE, config->bucket, config->region);

    ESP_LOGD(TAG, "Client created: %s", client->endpoint);

    *handler = client;
    return ESP_OK;
}

esp_err_t alicloud_oss_open(alicloud_oss_handler_t handler, http_method_t method, uri_t* uri, http_headers_t* headers) {
    if (!handler || !uri || !headers) {
        return ESP_ERR_INVALID_ARG;
    }

    // Ensure clean state - close previous request if exists
    if (handler->request) {
        ESP_LOGD(TAG, "Closing previous request before opening new one");
        alicloud_oss_close(handler);
    }
    
    // Double-check state is clean
    if (handler->response) {
        ESP_LOGW(TAG, "Response still exists after close, forcing cleanup");
        http_response_free(handler->response);
        free(handler->response);
        handler->response = nullptr;
    }

    // Build URL from URI
    ESP_LOGD(TAG, "Building URL for endpoint: %s, path: %s", handler->endpoint, uri->path);
    url_t* url = url_from_uri(HTTP_PROTOCOL_HTTPS, handler->endpoint, 443, uri);
    if (!url) {
        ESP_LOGE(TAG, "Failed to create URL from URI");
        return ESP_ERR_NO_MEM;
    }

    url_set_method(url, method);

    // Set Host header
    http_headers_set(headers, "Host", url->host);

    // Sign request with OSS V4 signature
    esp_err_t err = alicloud_oss_sign_v4(&handler->config, method, uri, headers);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sign request: %s", esp_err_to_name(err));
        url_free(url);
        return err;
    }

    // Create HTTP request
    handler->request = http_request_create(url);
    if (!handler->request) {
        ESP_LOGE(TAG, "Failed to create HTTP request");
        url_free(url);
        return ESP_ERR_NO_MEM;
    }

    // Copy headers to request
    http_headers_t* req_headers = http_request_get_headers(handler->request);
    for (size_t i = 0; i < headers->count; i++) {
        http_headers_set(req_headers, headers->headers[i].name, headers->headers[i].value);
    }

    // Open connection
    err = http_client_open(handler->client, url);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Connection failed: %s", esp_err_to_name(err));
        http_request_free(handler->request);
        handler->request = nullptr;
        return err;
    }
    
    // Note: URL is owned by request, will be freed by http_request_free()

    // For GET, HEAD, and DELETE requests, send request and receive headers immediately
    if (method == E_HTTP_METHOD_GET || method == E_HTTP_METHOD_HEAD || method == E_HTTP_METHOD_DELETE) {
        err = http_client_send_request(handler->client, handler->request);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Request failed: %s", esp_err_to_name(err));
            http_client_close(handler->client);
            http_request_free(handler->request);
            handler->request = nullptr;
            return err;
        }

        // Initialize response
        handler->response = malloc(sizeof(http_response_t));
        if (!handler->response) {
            http_client_close(handler->client);
            http_request_free(handler->request);
            handler->request = nullptr;
            return ESP_ERR_NO_MEM;
        }
        http_response_init(handler->response);

        // Receive response headers
        err = http_client_recv_headers(handler->client, handler->response);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to receive headers: %s", esp_err_to_name(err));
            http_response_free(handler->response);
            free(handler->response);
            handler->response = nullptr;
            http_client_close(handler->client);
            http_request_free(handler->request);
            handler->request = nullptr;
            return err;
        }

        int status = handler->response->status_code;
        
        // For GET requests, expect 200
        // For HEAD requests, expect 200
        // For DELETE requests, expect 204 (No Content) or 200
        bool is_success = false;
        if (method == E_HTTP_METHOD_DELETE) {
            is_success = (status == 204 || status == 200);
        } else {
            is_success = (status == 200);
        }
        
        if (!is_success) {
            ESP_LOGE(TAG, "Request failed: HTTP %d", status);
            
            // Read and discard response body to properly close connection
            char discard_buffer[256];
            size_t bytes_read;
            while (http_client_recv_data(handler->client, discard_buffer, 
                                        sizeof(discard_buffer), &bytes_read) == ESP_OK 
                   && bytes_read > 0) {
                // Discard data
            }
            
            // Clean up resources on error
            http_response_free(handler->response);
            free(handler->response);
            handler->response = nullptr;
            http_client_close(handler->client);
            http_request_free(handler->request);
            handler->request = nullptr;
            
            return ESP_FAIL;
        }
    }

    ESP_LOGD(TAG, "Request opened successfully");
    return ESP_OK;
}

/**
 * @brief Send data chunk for streaming upload (does not finalize request)
 * 
 * This function sends a data chunk without waiting for response.
 * Used for streaming uploads with Content-Length or chunked encoding.
 * 
 * @param handler OSS handler
 * @param data Data chunk to send
 * @param data_len Length of data chunk
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_write_chunk(alicloud_oss_handler_t handler, const void* data, size_t data_len) {
    if (!handler) {
        ESP_LOGE(TAG, "Handler is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!data || data_len == 0) {
        ESP_LOGE(TAG, "Invalid data: data=%p, len=%zu", data, data_len);
        return ESP_ERR_INVALID_ARG;
    }

    if (!handler->client) {
        ESP_LOGE(TAG, "Client is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    if (!handler->request) {
        ESP_LOGE(TAG, "No active request");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Sending chunk: handler=%p, client=%p, data=%p, len=%zu", 
             handler, handler->client, data, data_len);

    // Send data chunk directly without finalizing request
    esp_err_t err = http_client_send_data(handler->client, (const char*)data, data_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send data chunk: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "Sent data chunk: %zu bytes", data_len);
    return ESP_OK;
}

/**
 * @brief Finalize upload and receive response
 * 
 * This function finalizes the upload by receiving the server response.
 * Should be called after all data chunks have been sent.
 * 
 * @param handler OSS handler
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_finalize_upload(alicloud_oss_handler_t handler) {
    if (!handler) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handler->request) {
        ESP_LOGE(TAG, "No active request to finalize");
        return ESP_ERR_INVALID_STATE;
    }

    // Initialize response
    handler->response = malloc(sizeof(http_response_t));
    if (!handler->response) {
        return ESP_ERR_NO_MEM;
    }
    http_response_init(handler->response);

    // Receive response headers
    esp_err_t err = http_client_recv_headers(handler->client, handler->response);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive response headers: %s", esp_err_to_name(err));
        http_response_free(handler->response);
        free(handler->response);
        handler->response = nullptr;
        return err;
    }

    // Drain response body
    char buffer[512];
    while (true) {
        size_t bytes_read;
        err = http_client_recv_data(handler->client, buffer, sizeof(buffer), &bytes_read);
        if (err != ESP_OK || bytes_read == 0) break;
    }

    int status = handler->response->status_code;
    if (status < 200 || status >= 300) {
        ESP_LOGE(TAG, "Upload failed: HTTP %d", status);
        if (handler->response->body) {
            ESP_LOGD(TAG, "Response: %s", handler->response->body);
        }
        
        // Clean up resources on error
        http_response_free(handler->response);
        free(handler->response);
        handler->response = nullptr;
        http_client_close(handler->client);
        http_request_free(handler->request);
        handler->request = nullptr;
        
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Upload finalized successfully: HTTP %d", status);
    return ESP_OK;
}

/**
 * @brief Legacy write function - sends entire body and waits for response
 * 
 * This is the old implementation that sends all data at once.
 * For streaming uploads, use alicloud_oss_write_chunk() instead.
 */
esp_err_t alicloud_oss_write(alicloud_oss_handler_t handler, const void* data, size_t data_len) {
    if (!handler || !data || data_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handler->request) {
        ESP_LOGE(TAG, "No active request");
        return ESP_ERR_INVALID_STATE;
    }

    // Send request with body
    http_request_set_body(handler->request, (const char*)data, data_len);

    auto err = http_client_send_request(handler->client, handler->request);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Send failed: %s", esp_err_to_name(err));
        return err;
    }

    // Finalize and get response
    return alicloud_oss_finalize_upload(handler);
}

esp_err_t alicloud_oss_read(alicloud_oss_handler_t handler, void* data, int data_len, int* read_len) {
    if (!handler || !data || !read_len) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handler->response) {
        ESP_LOGE(TAG, "No active response");
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes_read;
    auto err = http_client_recv_data(handler->client, (char*)data, data_len, &bytes_read);
    if (err != ESP_OK) {
        *read_len = 0;
        return err;
    }

    *read_len = (int)bytes_read;
    return ESP_OK;
}

esp_err_t alicloud_oss_close(alicloud_oss_handler_t handler) {
    if (!handler) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handler->response) {
        http_response_free(handler->response);
        free(handler->response);
        handler->response = nullptr;
    }

    if (handler->request) {
        http_request_free(handler->request);
        handler->request = nullptr;
    }

    return ESP_OK;
}

int alicloud_oss_get_status(alicloud_oss_handler_t handler) {
    if (!handler || !handler->response) {
        return 0;
    }
    return handler->response->status_code;
}

const char* alicloud_oss_get_response_body(alicloud_oss_handler_t handler) {
    if (!handler || !handler->response) {
        return nullptr;
    }
    return handler->response->body;
}

esp_err_t alicloud_oss_destroy(alicloud_oss_handler_t handler) {
    if (!handler) {
        return ESP_ERR_INVALID_ARG;
    }

    // Close any active request
    alicloud_oss_close(handler);

    // Close HTTP client connection
    if (handler->client) {
        http_client_close(handler->client);
        http_client_cleanup(handler->client);
        handler->client = nullptr;
    }

    if (handler->endpoint) {
        free(handler->endpoint);
        handler->endpoint = nullptr;
    }

    free(handler);

    ESP_LOGD(TAG, "Client destroyed");
    return ESP_OK;
}
