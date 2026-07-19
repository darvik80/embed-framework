//
// Created by darvik on 03.02.2026.
//

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * OSS object metadata
 */
typedef struct {
    char *key;              // Object key (path)
    size_t size;            // Object size in bytes
    char *last_modified;    // Last modified timestamp (ISO8601)
    char *etag;             // Object ETag
    char *storage_class;    // Storage class (Standard, IA, Archive, etc.)
} alicloud_oss_object_t;

/**
 * OSS list objects result
 */
typedef struct {
    char *bucket_name;              // Bucket name
    char *prefix;                   // Prefix filter used
    alicloud_oss_object_t *objects; // Array of objects
    size_t object_count;            // Number of objects
    char **directories;             // Array of directory prefixes
    size_t directory_count;         // Number of directories
    bool is_truncated;              // Whether there are more results
    char *next_marker;              // Marker for next page
} alicloud_oss_list_result_t;

/**
 * OSS error response
 */
typedef struct {
    char *code;         // Error code
    char *message;      // Error message
    char *request_id;   // Request ID
    char *host_id;      // Host ID
} alicloud_oss_error_t;

/**
 * Parse XML tag content
 * @param xml XML string to parse
 * @param tag Tag name to find
 * @param output Output buffer for tag content
 * @param output_size Size of output buffer
 * @param next_pos Optional pointer to update with position after tag
 * @return true if tag found and parsed
 */
bool alicloud_oss_parse_xml_tag(const char *xml, const char *tag, char *output, size_t output_size, const char **next_pos);

/**
 * Parse LIST objects XML response
 * @param xml_response XML response from LIST operation
 * @param result Output list result structure
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_parse_list_response(const char *xml_response, alicloud_oss_list_result_t *result);

/**
 * Parse error XML response
 * @param xml_response XML error response
 * @param error Output error structure
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_parse_error_response(const char *xml_response, alicloud_oss_error_t *error);

/**
 * Free list result structure
 * @param result List result to free
 */
void alicloud_oss_free_list_result(alicloud_oss_list_result_t *result);

/**
 * Free error structure
 * @param error Error structure to free
 */
void alicloud_oss_free_error(alicloud_oss_error_t *error);

/**
 * Format file size to human-readable string
 * @param size Size in bytes
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 */
void alicloud_oss_format_size(size_t size, char *buffer, size_t buffer_size);

/**
 * Log list result to console (for debugging)
 * @param result List result to log
 * @param tag Log tag to use
 */
void alicloud_oss_log_list_result(const alicloud_oss_list_result_t *result, const char *tag);

/**
 * Log error to console (for debugging)
 * @param error Error to log
 * @param tag Log tag to use
 */
void alicloud_oss_log_error(const alicloud_oss_error_t *error, const char *tag);

#ifdef __cplusplus
}
#endif
