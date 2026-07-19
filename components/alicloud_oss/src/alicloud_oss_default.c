//
// Created by Ivan Kishchenko on 14/2/26.
//

#include "alicloud_oss_default.h"
#include "alicloud_oss_client.h"
#include "alicloud_oss_response_parser.h"
#include "net_tools.h"
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "OSS_DEFAULT";

// Helper macro to build OSS object path
// Note: OSS API expects path without bucket name, as bucket is in the Host header
#define BUILD_OSS_PATH(buf, size, bucket, file_id) \
    snprintf(buf, size, "/%s", \
             (file_id)[0] == '/' ? (file_id) + 1 : (file_id))

// ============================================================================
// Writer Implementation
// ============================================================================

typedef struct {
    struct alicloud_oss_writer_t base;
    alicloud_oss_handler_t oss;
    char *file_id;
    size_t file_size;
    size_t bytes_written;
    bool is_open;
    bool http_opened;
} alicloud_oss_default_writer_t;

esp_err_t w_open(struct alicloud_oss_writer_t *writer, const char *fileId, size_t file_size) {
    alicloud_oss_default_writer_t *self = __containerof(writer, alicloud_oss_default_writer_t, base);

    if (!fileId) {
        return ESP_ERR_INVALID_ARG;
    }

    // If already open, clean up previous state (but don't free self!)
    if (self->is_open) {
        ESP_LOGW(TAG, "Writer already open, closing previous file");

        // Close HTTP connection if open
        if (self->http_opened) {
            alicloud_oss_close(self->oss);
            self->http_opened = false;
        }

        // Free previous file_id
        if (self->file_id) {
            free(self->file_id);
            self->file_id = nullptr;
        }
    }

    self->file_id = strdup(fileId);
    if (!self->file_id) return ESP_ERR_NO_MEM;

    self->file_size = file_size;
    self->bytes_written = 0;
    self->is_open = true;
    self->http_opened = false;

    ESP_LOGD(TAG, "Writer opened: %s (%zu bytes)", fileId, file_size);
    return ESP_OK;
}

esp_err_t w_write(struct alicloud_oss_writer_t *writer, void *data, size_t data_len) {
    alicloud_oss_default_writer_t *self = __containerof(writer, alicloud_oss_default_writer_t, base);

    if (!self->is_open) {
        ESP_LOGE(TAG, "Writer not open");
        return ESP_ERR_INVALID_STATE;
    }

    if (!data || data_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;

    // First write: open HTTP connection and send headers
    if (!self->http_opened) {
        // Build OSS path
        char path[512];
        BUILD_OSS_PATH(path, sizeof(path), self->oss->config.bucket, self->file_id);

        uri_t *uri = uri_init();
        if (!uri) return ESP_ERR_NO_MEM;
        uri_set_path(uri, path);

        http_headers_t *headers = http_headers_init();
        if (!headers) {
            uri_free(uri);
            return ESP_ERR_NO_MEM;
        }

        // Set Content-Length if file size is known, otherwise use chunked encoding
        if (self->file_size > 0) {
            http_headers_add_int(headers, "Content-Length", self->file_size);
            ESP_LOGD(TAG, "Using Content-Length: %zu", self->file_size);
        } else {
            http_headers_set(headers, "Transfer-Encoding", "chunked");
            ESP_LOGD(TAG, "Using chunked encoding");
        }

        http_headers_set(headers, "Content-Type", "application/octet-stream");

        // Open connection (without sending headers yet)
        err = alicloud_oss_open(self->oss, E_HTTP_METHOD_PUT, uri, headers);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            http_headers_free(headers);
            uri_free(uri);
            return err;
        }

        // Send headers for streaming upload
        err = http_client_send_headers(self->oss->client, self->oss->request);

        http_headers_free(headers);
        uri_free(uri);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send headers: %s", esp_err_to_name(err));
            alicloud_oss_close(self->oss);
            return err;
        }

        self->http_opened = true;
        ESP_LOGD(TAG, "HTTP connection opened and headers sent for streaming upload");
    }

    // Write data chunk
    err = alicloud_oss_write_chunk(self->oss, data, data_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write chunk: %s", esp_err_to_name(err));
        
        // Close HTTP connection on error to reset state
        if (self->http_opened) {
            ESP_LOGW(TAG, "Closing HTTP connection due to write error");
            alicloud_oss_close(self->oss);
            self->http_opened = false;
        }
        
        return err;
    }

    self->bytes_written += data_len;
    ESP_LOGD(TAG, "Wrote chunk: %zu bytes (total: %zu)", data_len, self->bytes_written);

    return ESP_OK;
}

esp_err_t w_close(struct alicloud_oss_writer_t *writer) {
    alicloud_oss_default_writer_t *self = __containerof(writer, alicloud_oss_default_writer_t, base);

    esp_err_t err = ESP_OK;

    // Finalize upload if HTTP connection was opened
    if (self->http_opened) {
        // For chunked encoding, send final chunk
        if (self->file_size == 0) {
            const char *final_chunk = "0\r\n\r\n";
            err = http_client_send_data(self->oss->client, final_chunk, strlen(final_chunk));
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send final chunk: %s", esp_err_to_name(err));
            }
        }

        // Finalize upload and get response
        err = alicloud_oss_finalize_upload(self->oss);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to finalize upload: %s", esp_err_to_name(err));
        } else {
            ESP_LOGD(TAG, "Upload finalized successfully: %zu bytes", self->bytes_written);
        }

        // Close OSS request
        alicloud_oss_close(self->oss);
    }

    // Free resources
    if (self->file_id) {
        free(self->file_id);
        self->file_id = nullptr;
    }

    self->is_open = false;
    self->http_opened = false;
    free(self);

    ESP_LOGD(TAG, "Writer closed");
    return err;
}

esp_err_t alicloud_oss_writer_create(alicloud_oss_handler_t oss,
                                     alicloud_oss_writer_handler_t *handler) {
    if (!oss || !handler) {
        return ESP_ERR_INVALID_ARG;
    }

    alicloud_oss_default_writer_t *writer = malloc(sizeof(alicloud_oss_default_writer_t));
    if (!writer) {
        return ESP_ERR_NO_MEM;
    }

    memset(writer, 0, sizeof(alicloud_oss_default_writer_t));
    writer->oss = oss;
    writer->base.open = w_open;
    writer->base.write = w_write;
    writer->base.close = w_close;

    *handler = &writer->base;
    return ESP_OK;
}

// ============================================================================
// Reader Implementation
// ============================================================================

typedef struct {
    struct alicloud_oss_reader_t base;
    alicloud_oss_handler_t oss;
    bool is_open;
} alicloud_oss_default_reader_t;

esp_err_t r_open(struct alicloud_oss_reader_t *reader, const char *fileId) {
    alicloud_oss_default_reader_t *self = __containerof(reader, alicloud_oss_default_reader_t, base);

    if (!fileId) {
        return ESP_ERR_INVALID_ARG;
    }

    if (self->is_open) {
        ESP_LOGW(TAG, "Reader already open");
        alicloud_oss_close(self->oss);
    }

    // Build path - remove leading slash from fileId if present
    const char *clean_file_id = fileId;
    if (clean_file_id[0] == '/') {
        clean_file_id++;
    }

    char path[512];
    snprintf(path, sizeof(path), "/%s", clean_file_id);

    uri_t *uri = uri_init();
    if (!uri) return ESP_ERR_NO_MEM;
    uri_set_path(uri, path);

    http_headers_t *hdr = http_headers_init();
    if (!hdr) {
        uri_free(uri);
        return ESP_ERR_NO_MEM;
    }

    auto err = alicloud_oss_open(self->oss, E_HTTP_METHOD_GET, uri, hdr);

    http_headers_free(hdr);
    uri_free(uri);

    if (err == ESP_OK) {
        self->is_open = true;
        ESP_LOGD(TAG, "Reader opened: %s (path: %s)", fileId, path);
    } else {
        ESP_LOGE(TAG, "GET open failed: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t r_read(struct alicloud_oss_reader_t *reader, void *data, int data_len, int *read_len) {
    alicloud_oss_default_reader_t *self = __containerof(reader, alicloud_oss_default_reader_t, base);

    if (!self->is_open) {
        ESP_LOGE(TAG, "Reader not open");
        return ESP_ERR_INVALID_STATE;
    }

    return alicloud_oss_read(self->oss, data, data_len, read_len);
}

esp_err_t r_close(struct alicloud_oss_reader_t *reader) {
    alicloud_oss_default_reader_t *self = __containerof(reader, alicloud_oss_default_reader_t, base);

    if (self->is_open) {
        alicloud_oss_close(self->oss);
        self->is_open = false;
    }

    free(self);
    ESP_LOGD(TAG, "Reader closed");
    return ESP_OK;
}

esp_err_t alicloud_oss_reader_create(alicloud_oss_handler_t oss, alicloud_oss_read_handler_t *handler) {
    if (!oss || !handler) {
        return ESP_ERR_INVALID_ARG;
    }

    alicloud_oss_default_reader_t *reader = malloc(sizeof(alicloud_oss_default_reader_t));
    if (!reader) {
        return ESP_ERR_NO_MEM;
    }

    memset(reader, 0, sizeof(alicloud_oss_default_reader_t));
    reader->oss = oss;
    reader->base.open = r_open;
    reader->base.read = r_read;
    reader->base.close = r_close;

    *handler = &reader->base;
    return ESP_OK;
}

// ============================================================================
// Directory Walker Implementation
// ============================================================================

typedef struct {
    struct alicloud_oss_dir_walker_t base;
    alicloud_oss_handler_t oss;
    char *prefix;
    char *marker;
    bool is_truncated;
    bool is_open;
} alicloud_oss_default_dir_walker_t;

esp_err_t dw_open(struct alicloud_oss_dir_walker_t *walker, const char *prefix,
                  struct alicloud_oss_dir_walker_callback_t *callback) {
    alicloud_oss_default_dir_walker_t *self = __containerof(walker, alicloud_oss_default_dir_walker_t, base);

    if (self->is_open) {
        ESP_LOGW(TAG, "Walker already open");
        alicloud_oss_dir_walker_close(walker);
    }

    self->prefix = (prefix && *prefix) ? strdup(prefix) : nullptr;
    if (prefix && *prefix && !self->prefix) {
        return ESP_ERR_NO_MEM;
    }

    self->marker = nullptr;
    self->is_truncated = true;
    self->is_open = true;

    ESP_LOGD(TAG, "Walker opened: %s", prefix ? prefix : "(root)");
    return ESP_OK;
}

esp_err_t dw_walk(struct alicloud_oss_dir_walker_t *walker, struct alicloud_oss_dir_walker_callback_t *callback) {
    alicloud_oss_default_dir_walker_t *self = __containerof(walker, alicloud_oss_default_dir_walker_t, base);

    if (!self->is_open) {
        ESP_LOGE(TAG, "Walker not open");
        return ESP_ERR_INVALID_STATE;
    }

    if (!callback) {
        return ESP_ERR_INVALID_ARG;
    }

    // Build URI with query parameters
    // Path should be "/" for ListObjects API (bucket is in Host header)
    uri_t *uri = uri_init();
    if (!uri) {
        return ESP_ERR_NO_MEM;
    }
    uri_set_path(uri, "/");

    query_params_t *params = uri_get_params(uri);

    // Add prefix if specified
    if (self->prefix) {
        query_params_add(params, "prefix", self->prefix);
    }

    // Add delimiter to get directory structure
    query_params_add(params, "delimiter", "/");

    // Add marker for pagination
    if (self->marker) {
        query_params_add(params, "marker", self->marker);
    }

    // Set max-keys
    query_params_add_int(params, "max-keys", 1000);

    // Create headers
    http_headers_t *headers = http_headers_init();
    if (!headers) {
        uri_free(uri);
        return ESP_ERR_NO_MEM;
    }

    auto err = alicloud_oss_open(self->oss, E_HTTP_METHOD_GET, uri, headers);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ListObjects open failed: %s", esp_err_to_name(err));
        http_headers_free(headers);
        uri_free(uri);
        return err;
    }

    // Read response body
    char *response_body = malloc(4096);
    size_t response_len = 0;
    size_t response_capacity = 4096;

    if (!response_body) {
        alicloud_oss_close(self->oss);
        http_headers_free(headers);
        uri_free(uri);
        return ESP_ERR_NO_MEM;
    }

    // Read all data
    while (true) {
        int read_len;
        err = alicloud_oss_read(self->oss, response_body + response_len,
                                response_capacity - response_len - 1, &read_len);
        if (err != ESP_OK || read_len == 0) {
            break;
        }

        response_len += read_len;

        // Expand buffer if needed
        if (response_len + 1024 >= response_capacity) {
            response_capacity *= 2;
            char *new_body = realloc(response_body, response_capacity);
            if (!new_body) {
                free(response_body);
                alicloud_oss_close(self->oss);
                http_headers_free(headers);
                uri_free(uri);
                return ESP_ERR_NO_MEM;
            }
            response_body = new_body;
        }
    }

    response_body[response_len] = '\0';

    int status = alicloud_oss_get_status(self->oss);
    if (status != 200) {
        ESP_LOGE(TAG, "ListObjects failed: HTTP %d", status);
        ESP_LOGD(TAG, "Response: %s", response_body);
        free(response_body);
        alicloud_oss_close(self->oss);
        http_headers_free(headers);
        uri_free(uri);
        return ESP_FAIL;
    }

    alicloud_oss_list_result_t result;
    err = alicloud_oss_parse_list_response(response_body, &result);
    free(response_body);
    alicloud_oss_close(self->oss);
    http_headers_free(headers);
    uri_free(uri);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Parse failed: %s", esp_err_to_name(err));
        return err;
    }

    // Process objects
    for (size_t i = 0; i < result.object_count; i++) {
        if (callback->on_elem && result.objects) {
            callback->on_elem(callback, self->prefix, &result.objects[i]);
        }
    }

    // Process directories
    for (size_t i = 0; i < result.directory_count; i++) {
        // Check if directory name is not NULL before calling callback
        if (callback->on_dir && result.directories && result.directories[i]) {
            callback->on_dir(callback, self->prefix, result.directories[i]);
        }
    }

    // Update pagination
    self->is_truncated = result.is_truncated;
    free(self->marker);
    self->marker = (result.next_marker && *result.next_marker) ? strdup(result.next_marker) : nullptr;

    ESP_LOGD(TAG, "Listed: %zu objects, %zu dirs", result.object_count, result.directory_count);

    alicloud_oss_free_list_result(&result);
    return ESP_OK;
}

esp_err_t dw_close(struct alicloud_oss_dir_walker_t *walker) {
    alicloud_oss_default_dir_walker_t *self = __containerof(walker, alicloud_oss_default_dir_walker_t, base);

    if (self->prefix) {
        free(self->prefix);
        self->prefix = nullptr;
    }

    if (self->marker) {
        free(self->marker);
        self->marker = nullptr;
    }

    self->is_open = false;
    free(self);

    ESP_LOGD(TAG, "Walker closed");
    return ESP_OK;
}

esp_err_t alicloud_oss_dir_walker_create(alicloud_oss_handler_t oss,
                                         alicloud_oss_dir_walker_handler_t *handler) {
    if (!oss || !handler) {
        return ESP_ERR_INVALID_ARG;
    }

    alicloud_oss_default_dir_walker_t *walker = malloc(sizeof(alicloud_oss_default_dir_walker_t));
    if (!walker) {
        return ESP_ERR_NO_MEM;
    }

    memset(walker, 0, sizeof(alicloud_oss_default_dir_walker_t));
    walker->oss = oss;
    walker->base.open = dw_open;
    walker->base.walk = dw_walk;
    walker->base.close = dw_close;

    *handler = &walker->base;
    return ESP_OK;
}

// ============================================================================
// Delete Object Implementation
// ============================================================================

esp_err_t alicloud_oss_delete_object(alicloud_oss_handler_t oss, const char* object_key) {
    if (!oss || !object_key) {
        return ESP_ERR_INVALID_ARG;
    }

    // Build path - remove leading slash from object_key if present
    const char* clean_key = object_key;
    if (clean_key[0] == '/') {
        clean_key++;
    }

    char path[512];
    snprintf(path, sizeof(path), "/%s", clean_key);

    // Create URI
    uri_t* uri = uri_init();
    if (!uri) {
        return ESP_ERR_NO_MEM;
    }
    uri_set_path(uri, path);

    // Create headers
    http_headers_t* headers = http_headers_init();
    if (!headers) {
        uri_free(uri);
        return ESP_ERR_NO_MEM;
    }

    // Open connection with DELETE method
    esp_err_t err = alicloud_oss_open(oss, E_HTTP_METHOD_DELETE, uri, headers);
    
    http_headers_free(headers);
    uri_free(uri);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DELETE request failed: %s", esp_err_to_name(err));
        return err;
    }

    int status = alicloud_oss_get_status(oss);
    
    // OSS returns 204 (No Content) on successful deletion
    // or 200 if versioning is enabled
    if (status != 204 && status != 200) {
        ESP_LOGE(TAG, "Delete object failed: HTTP %d", status);
        const char* response_body = alicloud_oss_get_response_body(oss);
        if (response_body) {
            ESP_LOGD(TAG, "Response: %s", response_body);
        }
        alicloud_oss_close(oss);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Object deleted successfully: %s (HTTP %d)", object_key, status);
    
    // Close request
    alicloud_oss_close(oss);
    
    return ESP_OK;
}

// ============================================================================
// Head Object Implementation
// ============================================================================

esp_err_t alicloud_oss_head_object(alicloud_oss_handler_t oss, const char* object_key, 
                                   alicloud_oss_object_metadata_t* metadata) {
    if (!oss || !object_key || !metadata) {
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize metadata structure
    memset(metadata, 0, sizeof(alicloud_oss_object_metadata_t));

    // Build path - remove leading slash from object_key if present
    const char* clean_key = object_key;
    if (clean_key[0] == '/') {
        clean_key++;
    }

    char path[512];
    snprintf(path, sizeof(path), "/%s", clean_key);

    // Create URI
    uri_t* uri = uri_init();
    if (!uri) {
        return ESP_ERR_NO_MEM;
    }
    uri_set_path(uri, path);

    // Create headers
    http_headers_t* headers = http_headers_init();
    if (!headers) {
        uri_free(uri);
        return ESP_ERR_NO_MEM;
    }

    // Open connection with HEAD method
    esp_err_t err = alicloud_oss_open(oss, E_HTTP_METHOD_HEAD, uri, headers);
    
    http_headers_free(headers);
    uri_free(uri);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HEAD request failed: %s", esp_err_to_name(err));
        return err;
    }

    int status = alicloud_oss_get_status(oss);
    
    // OSS returns 200 on successful HEAD request
    if (status != 200) {
        ESP_LOGE(TAG, "Head object failed: HTTP %d", status);
        alicloud_oss_close(oss);
        return ESP_FAIL;
    }

    // Extract metadata from response headers
    if (oss->response && oss->response->headers) {
        http_headers_t* resp_headers = oss->response->headers;
        
        // Get Content-Type
        const char* content_type = http_headers_get(resp_headers, "Content-Type");
        if (content_type) {
            metadata->content_type = strdup(content_type);
        }
        
        // Get Content-Length
        const char* content_length_str = http_headers_get(resp_headers, "Content-Length");
        if (content_length_str) {
            metadata->content_length = (size_t)atoll(content_length_str);
        }
        
        // Get ETag
        const char* etag = http_headers_get(resp_headers, "ETag");
        if (etag) {
            metadata->etag = strdup(etag);
        }
        
        // Get Last-Modified
        const char* last_modified = http_headers_get(resp_headers, "Last-Modified");
        if (last_modified) {
            metadata->last_modified = strdup(last_modified);
        }
        
        // Get x-oss-storage-class
        const char* storage_class = http_headers_get(resp_headers, "x-oss-storage-class");
        if (storage_class) {
            metadata->storage_class = strdup(storage_class);
        }
        
        // Get x-oss-object-type
        const char* object_type = http_headers_get(resp_headers, "x-oss-object-type");
        if (object_type) {
            metadata->object_type = strdup(object_type);
        }
    }

    ESP_LOGD(TAG, "Object metadata retrieved: %s (size: %zu, type: %s)", 
             object_key, metadata->content_length, 
             metadata->content_type ? metadata->content_type : "unknown");
    
    // Close request
    alicloud_oss_close(oss);
    
    return ESP_OK;
}

void alicloud_oss_free_object_metadata(alicloud_oss_object_metadata_t* metadata) {
    if (!metadata) {
        return;
    }
    
    if (metadata->content_type) {
        free(metadata->content_type);
        metadata->content_type = nullptr;
    }
    
    if (metadata->etag) {
        free(metadata->etag);
        metadata->etag = nullptr;
    }
    
    if (metadata->last_modified) {
        free(metadata->last_modified);
        metadata->last_modified = nullptr;
    }
    
    if (metadata->storage_class) {
        free(metadata->storage_class);
        metadata->storage_class = nullptr;
    }
    
    if (metadata->object_type) {
        free(metadata->object_type);
        metadata->object_type = nullptr;
    }
    
    metadata->content_length = 0;
}

// ============================================================================
// Create Directory Implementation
// ============================================================================

esp_err_t alicloud_oss_create_directory(alicloud_oss_handler_t oss, const char* dir_name) {
    if (!oss || !dir_name) {
        return ESP_ERR_INVALID_ARG;
    }

    // Ensure directory name ends with '/'
    size_t dir_len = strlen(dir_name);
    char* normalized_dir = nullptr;
    bool needs_slash = (dir_len == 0 || dir_name[dir_len - 1] != '/');
    
    if (needs_slash) {
        normalized_dir = malloc(dir_len + 2);
        if (!normalized_dir) {
            return ESP_ERR_NO_MEM;
        }
        strcpy(normalized_dir, dir_name);
        strcat(normalized_dir, "/");
    } else {
        normalized_dir = strdup(dir_name);
        if (!normalized_dir) {
            return ESP_ERR_NO_MEM;
        }
    }

    // Build path - remove leading slash if present
    const char* clean_dir = normalized_dir;
    if (clean_dir[0] == '/') {
        clean_dir++;
    }

    char path[512];
    snprintf(path, sizeof(path), "/%s", clean_dir);

    // Create URI
    uri_t* uri = uri_init();
    if (!uri) {
        free(normalized_dir);
        return ESP_ERR_NO_MEM;
    }
    uri_set_path(uri, path);

    // Create headers
    http_headers_t* headers = http_headers_init();
    if (!headers) {
        uri_free(uri);
        free(normalized_dir);
        return ESP_ERR_NO_MEM;
    }

    // Set Content-Length to 0 for empty directory object
    http_headers_set(headers, "Content-Length", "0");
    http_headers_set(headers, "Content-Type", "application/x-directory");

    // Open connection with PUT method
    esp_err_t err = alicloud_oss_open(oss, E_HTTP_METHOD_PUT, uri, headers);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open connection: %s", esp_err_to_name(err));
        http_headers_free(headers);
        uri_free(uri);
        free(normalized_dir);
        return err;
    }

    // Send empty body
    err = http_client_send_headers(oss->client, oss->request);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send headers: %s", esp_err_to_name(err));
        http_headers_free(headers);
        uri_free(uri);
        free(normalized_dir);
        alicloud_oss_close(oss);
        return err;
    }

    // Finalize upload
    err = alicloud_oss_finalize_upload(oss);
    
    http_headers_free(headers);
    uri_free(uri);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create directory: %s", esp_err_to_name(err));
        free(normalized_dir);
        alicloud_oss_close(oss);
        return err;
    }

    int status = alicloud_oss_get_status(oss);
    if (status != 200) {
        ESP_LOGE(TAG, "Create directory failed: HTTP %d", status);
        const char* response_body = alicloud_oss_get_response_body(oss);
        if (response_body) {
            ESP_LOGD(TAG, "Response: %s", response_body);
        }
        free(normalized_dir);
        alicloud_oss_close(oss);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Directory created successfully: %s", normalized_dir);
    free(normalized_dir);
    
    // Close request
    alicloud_oss_close(oss);
    
    return ESP_OK;
}

// ============================================================================
// Rename Implementation
// ============================================================================

esp_err_t alicloud_oss_rename(alicloud_oss_handler_t oss, const char* source_key, const char* dest_key) {
    if (!oss || !source_key || !dest_key) {
        return ESP_ERR_INVALID_ARG;
    }

    // Build destination path - remove leading slash if present
    const char* clean_dest = dest_key;
    if (clean_dest[0] == '/') {
        clean_dest++;
    }

    char dest_path[512];
    snprintf(dest_path, sizeof(dest_path), "/%s", clean_dest);

    // Create URI for destination
    uri_t* uri = uri_init();
    if (!uri) {
        return ESP_ERR_NO_MEM;
    }
    uri_set_path(uri, dest_path);

    // Create headers
    http_headers_t* headers = http_headers_init();
    if (!headers) {
        uri_free(uri);
        return ESP_ERR_NO_MEM;
    }

    // Build source path for x-oss-copy-source header
    const char* clean_source = source_key;
    if (clean_source[0] == '/') {
        clean_source++;
    }

    char source_header[512];
    snprintf(source_header, sizeof(source_header), "/%s/%s", 
             oss->config.bucket, clean_source);

    // Set copy source header
    http_headers_set(headers, "x-oss-copy-source", source_header);
    
    // Set metadata directive to COPY to preserve metadata
    http_headers_set(headers, "x-oss-metadata-directive", "COPY");

    // Open connection with PUT method (copy operation)
    esp_err_t err = alicloud_oss_open(oss, E_HTTP_METHOD_PUT, uri, headers);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open connection for copy: %s", esp_err_to_name(err));
        http_headers_free(headers);
        uri_free(uri);
        return err;
    }

    // Send headers (no body for copy operation)
    err = http_client_send_headers(oss->client, oss->request);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send headers: %s", esp_err_to_name(err));
        http_headers_free(headers);
        uri_free(uri);
        alicloud_oss_close(oss);
        return err;
    }

    // Finalize copy operation
    err = alicloud_oss_finalize_upload(oss);
    
    http_headers_free(headers);
    uri_free(uri);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to copy object: %s", esp_err_to_name(err));
        alicloud_oss_close(oss);
        return err;
    }

    int status = alicloud_oss_get_status(oss);
    if (status != 200) {
        ESP_LOGE(TAG, "Copy failed: HTTP %d", status);
        const char* response_body = alicloud_oss_get_response_body(oss);
        if (response_body) {
            ESP_LOGD(TAG, "Response: %s", response_body);
        }
        alicloud_oss_close(oss);
        return ESP_FAIL;
    }

    // Close copy request
    alicloud_oss_close(oss);

    ESP_LOGD(TAG, "Object copied successfully: %s -> %s", source_key, dest_key);

    // Now delete the source object
    err = alicloud_oss_delete_object(oss, source_key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete source object after copy: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "Rename completed successfully: %s -> %s", source_key, dest_key);
    
    return ESP_OK;
}

// ============================================================================
// Delete Directory Implementation
// ============================================================================

// Callback structure for recursive directory deletion
typedef struct {
    struct alicloud_oss_dir_walker_callback_t base;
    alicloud_oss_handler_t oss;
    esp_err_t error;
} delete_dir_callback_t;

// Callback function to delete each object in directory
static esp_err_t delete_dir_on_object(struct alicloud_oss_dir_walker_callback_t* cb, 
                                      const char* prefix, 
                                      alicloud_oss_object_t* obj) {
    delete_dir_callback_t* dcb = __containerof(cb, delete_dir_callback_t, base);
    
    ESP_LOGD(TAG, "Deleting object: %s", obj->key);
    esp_err_t err = alicloud_oss_delete_object(dcb->oss, obj->key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete object: %s", obj->key);
        dcb->error = err;
        return err;
    }
    return ESP_OK;
}

esp_err_t alicloud_oss_delete_directory(alicloud_oss_handler_t oss, const char* dir_name, bool recursive) {
    if (!oss || !dir_name) {
        return ESP_ERR_INVALID_ARG;
    }

    // Ensure directory name ends with '/'
    size_t dir_len = strlen(dir_name);
    char* normalized_dir = nullptr;
    bool needs_slash = (dir_len == 0 || dir_name[dir_len - 1] != '/');
    
    if (needs_slash) {
        normalized_dir = malloc(dir_len + 2);
        if (!normalized_dir) {
            return ESP_ERR_NO_MEM;
        }
        strcpy(normalized_dir, dir_name);
        strcat(normalized_dir, "/");
    } else {
        normalized_dir = strdup(dir_name);
        if (!normalized_dir) {
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t result = ESP_OK;

    if (recursive) {
        // List all objects under the directory and delete them
        alicloud_oss_dir_walker_handler_t walker;
        esp_err_t err = alicloud_oss_dir_walker_create(oss, &walker);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create walker: %s", esp_err_to_name(err));
            free(normalized_dir);
            return err;
        }

        // Setup callback for deletion
        delete_dir_callback_t callback = {
            .base = {
                .on_elem = delete_dir_on_object,
                .on_dir = nullptr
            },
            .oss = oss,
            .error = ESP_OK
        };

        // Open walker with directory prefix
        err = alicloud_oss_dir_walker_open(walker, normalized_dir, &callback.base);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open walker: %s", esp_err_to_name(err));
            alicloud_oss_dir_walker_close(walker);
            free(normalized_dir);
            return err;
        }

        // Get walker implementation to check is_truncated
        alicloud_oss_default_dir_walker_t* walker_impl = 
            __containerof(walker, alicloud_oss_default_dir_walker_t, base);

        // Walk through all objects and delete them
        do {
            err = alicloud_oss_dir_walker_walk(walker, &callback.base);
            if (err != ESP_OK || callback.error != ESP_OK) {
                ESP_LOGE(TAG, "Failed to walk directory: %s", esp_err_to_name(err));
                result = (callback.error != ESP_OK) ? callback.error : err;
                break;
            }
        } while (walker_impl->is_truncated);

        alicloud_oss_dir_walker_close(walker);

        if (result != ESP_OK) {
            free(normalized_dir);
            return result;
        }
    }

    // Delete the directory object itself
    esp_err_t err = alicloud_oss_delete_object(oss, normalized_dir);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete directory object: %s", normalized_dir);
        free(normalized_dir);
        return err;
    }

    ESP_LOGD(TAG, "Directory deleted successfully: %s", normalized_dir);
    free(normalized_dir);
    
    return ESP_OK;
}
