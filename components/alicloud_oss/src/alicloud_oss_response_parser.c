//
// Created by darvik on 03.02.2026.
//

#include "alicloud_oss_response_parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <esp_log.h>

bool alicloud_oss_parse_xml_tag(const char *xml, const char *tag, char *output, size_t output_size, const char **next_pos) {
    if (!xml || !tag || !output) {
        return false;
    }

    char start_tag[64];
    char end_tag[64];
    snprintf(start_tag, sizeof(start_tag), "<%s>", tag);
    snprintf(end_tag, sizeof(end_tag), "</%s>", tag);
    
    const char *start = strstr(next_pos ? *next_pos : xml, start_tag);
    if (!start) {
        return false;
    }
    
    start += strlen(start_tag);
    const char *end = strstr(start, end_tag);
    if (!end) {
        return false;
    }
    
    size_t len = end - start;
    if (len >= output_size) {
        len = output_size - 1;
    }
    
    memcpy(output, start, len);
    output[len] = '\0';
    
    if (next_pos) {
        *next_pos = end + strlen(end_tag);
    }
    
    return true;
}

static char* parse_xml_tag_alloc(const char *xml, const char *tag, const char **next_pos) {
    char buffer[512];
    if (alicloud_oss_parse_xml_tag(xml, tag, buffer, sizeof(buffer), next_pos)) {
        return strdup(buffer);
    }
    return nullptr;
}

esp_err_t alicloud_oss_parse_list_response(const char *xml_response, alicloud_oss_list_result_t *result) {
    if (!xml_response || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(alicloud_oss_list_result_t));

    // Parse bucket name
    result->bucket_name = parse_xml_tag_alloc(xml_response, "Name", nullptr);
    
    // Parse prefix
    result->prefix = parse_xml_tag_alloc(xml_response, "Prefix", nullptr);
    
    // Parse IsTruncated
    char buffer[16];
    if (alicloud_oss_parse_xml_tag(xml_response, "IsTruncated", buffer, sizeof(buffer), nullptr)) {
        result->is_truncated = (strcmp(buffer, "true") == 0);
    }
    
    // Parse NextMarker
    result->next_marker = parse_xml_tag_alloc(xml_response, "NextMarker", nullptr);

    // Count objects
    const char *pos = xml_response;
    size_t count = 0;
    while (strstr(pos, "<Contents>")) {
        count++;
        pos = strstr(pos, "<Contents>") + 10;
    }
    
    if (count > 0) {
        result->objects = calloc(count, sizeof(alicloud_oss_object_t));
        if (!result->objects) {
            alicloud_oss_free_list_result(result);
            return ESP_ERR_NO_MEM;
        }
        
        // Parse each object
        pos = xml_response;
        size_t index = 0;
        while (index < count) {
            const char *contents_start = strstr(pos, "<Contents>");
            if (!contents_start) break;
            
            const char *contents_end = strstr(contents_start, "</Contents>");
            if (!contents_end) break;
            
            const char *content_pos = contents_start;
            
            // Parse Key
            result->objects[index].key = parse_xml_tag_alloc(content_pos, "Key", &content_pos);
            
            // Parse Size
            if (alicloud_oss_parse_xml_tag(content_pos, "Size", buffer, sizeof(buffer), &content_pos)) {
                result->objects[index].size = atol(buffer);
            }
            
            // Parse LastModified
            result->objects[index].last_modified = parse_xml_tag_alloc(content_pos, "LastModified", &content_pos);
            
            // Parse ETag
            result->objects[index].etag = parse_xml_tag_alloc(content_pos, "ETag", &content_pos);
            
            // Parse StorageClass
            result->objects[index].storage_class = parse_xml_tag_alloc(content_pos, "StorageClass", &content_pos);
            
            index++;
            pos = contents_end + 11;
        }
        
        result->object_count = index;
    }

    // Count directories
    pos = xml_response;
    count = 0;
    while (strstr(pos, "<CommonPrefixes>")) {
        count++;
        pos = strstr(pos, "<CommonPrefixes>") + 16;
    }
    
    if (count > 0) {
        result->directories = calloc(count, sizeof(char*));
        if (!result->directories) {
            alicloud_oss_free_list_result(result);
            return ESP_ERR_NO_MEM;
        }
        
        // Parse each directory
        pos = xml_response;
        size_t index = 0;
        while (index < count) {
            const char *prefix_start = strstr(pos, "<CommonPrefixes>");
            if (!prefix_start) break;
            
            const char *prefix_end = strstr(prefix_start, "</CommonPrefixes>");
            if (!prefix_end) break;
            
            result->directories[index] = parse_xml_tag_alloc(prefix_start, "Prefix", nullptr);
            
            index++;
            pos = prefix_end + 17;
        }
        
        result->directory_count = index;
    }

    return ESP_OK;
}

esp_err_t alicloud_oss_parse_error_response(const char *xml_response, alicloud_oss_error_t *error) {
    if (!xml_response || !error) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(error, 0, sizeof(alicloud_oss_error_t));

    error->code = parse_xml_tag_alloc(xml_response, "Code", nullptr);
    error->message = parse_xml_tag_alloc(xml_response, "Message", nullptr);
    error->request_id = parse_xml_tag_alloc(xml_response, "RequestId", nullptr);
    error->host_id = parse_xml_tag_alloc(xml_response, "HostId", nullptr);

    return ESP_OK;
}

void alicloud_oss_free_list_result(alicloud_oss_list_result_t *result) {
    if (!result) {
        return;
    }

    free(result->bucket_name);
    free(result->prefix);
    free(result->next_marker);

    if (result->objects) {
        for (size_t i = 0; i < result->object_count; i++) {
            free(result->objects[i].key);
            free(result->objects[i].last_modified);
            free(result->objects[i].etag);
            free(result->objects[i].storage_class);
        }
        free(result->objects);
    }

    if (result->directories) {
        for (size_t i = 0; i < result->directory_count; i++) {
            free(result->directories[i]);
        }
        free(result->directories);
    }

    memset(result, 0, sizeof(alicloud_oss_list_result_t));
}

void alicloud_oss_free_error(alicloud_oss_error_t *error) {
    if (!error) {
        return;
    }

    free(error->code);
    free(error->message);
    free(error->request_id);
    free(error->host_id);

    memset(error, 0, sizeof(alicloud_oss_error_t));
}

void alicloud_oss_format_size(size_t size, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return;
    }

    if (size < 1024) {
        snprintf(buffer, buffer_size, "%zu bytes", size);
    } else if (size < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.2f KB", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.2f MB", size / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, buffer_size, "%.2f GB", size / (1024.0 * 1024.0 * 1024.0));
    }
}

void alicloud_oss_log_list_result(const alicloud_oss_list_result_t *result, const char *tag) {
    if (!result || !tag) {
        return;
    }

    ESP_LOGI(tag, "========== Object List ==========");
    ESP_LOGI(tag, "Bucket: %s", result->bucket_name ? result->bucket_name : "N/A");
    ESP_LOGI(tag, "Prefix: %s", result->prefix && result->prefix[0] ? result->prefix : "(root)");
    
    if (result->object_count > 0) {
        ESP_LOGI(tag, "\nObjects:");
        for (size_t i = 0; i < result->object_count; i++) {
            const alicloud_oss_object_t *obj = &result->objects[i];
            char size_str[32];
            alicloud_oss_format_size(obj->size, size_str, sizeof(size_str));
            
            ESP_LOGI(tag, "[%zu] %s (%s)", i + 1, obj->key ? obj->key : "N/A", size_str);
            if (obj->last_modified) {
                ESP_LOGI(tag, "    Modified: %s", obj->last_modified);
            }
        }
    }
    
    if (result->directory_count > 0) {
        ESP_LOGI(tag, "\nDirectories: %zu", result->directory_count);
        for (size_t i = 0; i < result->directory_count; i++) {
            ESP_LOGI(tag, "[DIR] %s", result->directories[i]);
        }
    }
    
    ESP_LOGI(tag, "\nTotal: %zu objects, %zu directories", result->object_count, result->directory_count);
    if (result->is_truncated) {
        ESP_LOGI(tag, "More results available (truncated), next marker: %s", result->next_marker);
    }
    ESP_LOGI(tag, "==================================");
}

void alicloud_oss_log_error(const alicloud_oss_error_t *error, const char *tag) {
    if (!error || !tag) {
        return;
    }

    ESP_LOGE(tag, "========== OSS Error ==========");
    if (error->code) {
        ESP_LOGE(tag, "Code: %s", error->code);
    }
    if (error->message) {
        ESP_LOGE(tag, "Message: %s", error->message);
    }
    if (error->request_id) {
        ESP_LOGE(tag, "Request ID: %s", error->request_id);
    }
    if (error->host_id) {
        ESP_LOGE(tag, "Host ID: %s", error->host_id);
    }
    ESP_LOGE(tag, "===============================");
}
