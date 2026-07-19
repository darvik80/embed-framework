//
// Created by Ivan Kishchenko on 14/2/26.
//

#ifndef ALICLOUD_ESP_IDF_ALICLOUD_OSS_DEFAULT_H
#define ALICLOUD_ESP_IDF_ALICLOUD_OSS_DEFAULT_H

#include "alicloud_oss.h"
#include "alicloud_oss_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Object metadata structure
 * 
 * Contains metadata information retrieved from HEAD Object operation
 */
typedef struct {
    char* content_type;           // Content-Type header
    size_t content_length;        // Content-Length header
    char* etag;                   // ETag header
    char* last_modified;          // Last-Modified header
    char* storage_class;          // x-oss-storage-class header
    char* object_type;            // x-oss-object-type header (Normal, Multipart, Appendable)
} alicloud_oss_object_metadata_t;

/**
 * @brief Create OSS writer handler
 * 
 * @param oss OSS client handler
 * @param handler Output writer handler
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_writer_create(alicloud_oss_handler_t oss, alicloud_oss_writer_handler_t *handler);

/**
 * @brief Create OSS reader handler
 * 
 * @param oss OSS client handler
 * @param handler Output reader handler
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_reader_create(alicloud_oss_handler_t oss, alicloud_oss_read_handler_t *handler);

/**
 * @brief Create OSS directory walker handler
 * 
 * @param oss OSS client handler
 * @param handler Output walker handler
 * @return ESP_OK on success
 */
esp_err_t alicloud_oss_dir_walker_create(alicloud_oss_handler_t oss, alicloud_oss_dir_walker_handler_t *handler);

/**
 * @brief Delete an object from OSS
 * 
 * Implements DELETE Object operation as per:
 * https://www.alibabacloud.com/help/en/oss/developer-reference/deleteobject
 * 
 * @param oss OSS client handler
 * @param object_key Object key (path) to delete
 * @return ESP_OK on success, ESP_FAIL if object doesn't exist or other error
 */
esp_err_t alicloud_oss_delete_object(alicloud_oss_handler_t oss, const char* object_key);

/**
 * @brief Get object metadata without downloading content
 * 
 * Implements HEAD Object operation as per:
 * https://www.alibabacloud.com/help/en/oss/developer-reference/headobject
 * 
 * @param oss OSS client handler
 * @param object_key Object key (path) to query
 * @param metadata Output metadata structure (caller must free with alicloud_oss_free_object_metadata)
 * @return ESP_OK on success, ESP_FAIL if object doesn't exist
 */
esp_err_t alicloud_oss_head_object(alicloud_oss_handler_t oss, const char* object_key, 
                                   alicloud_oss_object_metadata_t* metadata);

/**
 * @brief Free object metadata structure
 * 
 * @param metadata Metadata structure to free
 */
void alicloud_oss_free_object_metadata(alicloud_oss_object_metadata_t* metadata);

/**
 * @brief Create a directory in OSS
 * 
 * Implements CreateDirectory operation as per:
 * https://www.alibabacloud.com/help/en/oss/developer-reference/createdirectory
 * 
 * Creates a directory by uploading an empty object with a trailing slash.
 * 
 * @param oss OSS client handler
 * @param dir_name Directory name (path), must end with '/'
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t alicloud_oss_create_directory(alicloud_oss_handler_t oss, const char* dir_name);

/**
 * @brief Rename an object or directory in OSS
 * 
 * Implements Rename operation as per:
 * https://www.alibabacloud.com/help/en/oss/developer-reference/rename
 * 
 * Renames an object or directory. For directories, all objects under the directory
 * will be renamed recursively.
 * 
 * @param oss OSS client handler
 * @param source_key Source object/directory key (path)
 * @param dest_key Destination object/directory key (path)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t alicloud_oss_rename(alicloud_oss_handler_t oss, const char* source_key, const char* dest_key);

/**
 * @brief Delete a directory in OSS
 * 
 * Implements DeleteDirectory operation as per:
 * https://www.alibabacloud.com/help/en/oss/developer-reference/deletedirectory
 * 
 * Deletes a directory and all objects under it recursively.
 * 
 * @param oss OSS client handler
 * @param dir_name Directory name (path), must end with '/'
 * @param recursive If true, deletes all objects under the directory
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t alicloud_oss_delete_directory(alicloud_oss_handler_t oss, const char* dir_name, bool recursive);

#ifdef __cplusplus
}
#endif

#endif //ALICLOUD_ESP_IDF_ALICLOUD_OSS_DEFAULT_H