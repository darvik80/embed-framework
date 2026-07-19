//
// Created by Ivan Kishchenko on 16/2/26.
//

#ifndef ALICLOUD_ESP_IDF_ALICLOUD_OSS_CLIENT_H
#define ALICLOUD_ESP_IDF_ALICLOUD_OSS_CLIENT_H

#include "alicloud_oss.h"
#include "net_tools.h"
#include "http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    alicloud_oss_config_t config;
    http_client_t* client;
    http_request_t* request;
    http_response_t* response;
    char* endpoint;
} alicloud_oss_client_t;

typedef alicloud_oss_client_t* alicloud_oss_handler_t;

/**
 * @brief Create OSS client handler
 * 
 * @param config OSS configuration
 * @param handler Output handler
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_create(alicloud_oss_config_t* config, alicloud_oss_handler_t* handler);

/**
 * @brief Open connection and send request headers
 * 
 * @param handler OSS handler
 * @param method HTTP method
 * @param uri Request URI
 * @param headers Request headers
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_open(alicloud_oss_handler_t handler, http_method_t method, uri_t* uri, http_headers_t* headers);

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
esp_err_t alicloud_oss_write_chunk(alicloud_oss_handler_t handler, const void* data, size_t data_len);

/**
 * @brief Finalize upload and receive response
 * 
 * This function finalizes the upload by receiving the server response.
 * Should be called after all data chunks have been sent.
 * 
 * @param handler OSS handler
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_finalize_upload(alicloud_oss_handler_t handler);

/**
 * @brief Write data chunk to OSS (for streaming uploads)
 * 
 * This function sends data directly without finalizing the request.
 * Used for streaming body data after headers have been sent.
 * 
 * @param handler OSS handler
 * @param data Data to write
 * @param data_len Data length
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_write_chunk(alicloud_oss_handler_t handler, const void* data, size_t data_len);

/**
 * @brief Finalize upload and receive response
 * 
 * This function receives the response headers and body after all chunks have been sent.
 * 
 * @param handler OSS handler
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_finalize_upload(alicloud_oss_handler_t handler);

/**
 * @brief Write data to OSS (for PUT/POST operations)
 * 
 * Legacy function that sends all data at once.
 * 
 * @param handler OSS handler
 * @param data Data to write
 * @param data_len Data length
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_write(alicloud_oss_handler_t handler, const void* data, size_t data_len);

/**
 * @brief Read data from OSS (for GET operations)
 * 
 * @param handler OSS handler
 * @param data Buffer to read into
 * @param data_len Buffer size
 * @param read_len Actual bytes read
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_read(alicloud_oss_handler_t handler, void* data, int data_len, int* read_len);

/**
 * @brief Close current request
 * 
 * @param handler OSS handler
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_close(alicloud_oss_handler_t handler);

/**
 * @brief Get response status code
 * 
 * @param handler OSS handler
 * @return HTTP status code
 */
int alicloud_oss_get_status(alicloud_oss_handler_t handler);

/**
 * @brief Get response body
 * 
 * @param handler OSS handler
 * @return Response body string (valid until next request)
 */
const char* alicloud_oss_get_response_body(alicloud_oss_handler_t handler);

/**
 * @brief Destroy OSS client handler
 * 
 * @param handler OSS handler
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_destroy(alicloud_oss_handler_t handler);

#ifdef __cplusplus
}
#endif

#endif //ALICLOUD_ESP_IDF_ALICLOUD_OSS_CLIENT_H
