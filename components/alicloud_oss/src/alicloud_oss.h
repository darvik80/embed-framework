//
// Created by Ivan Kishchenko on 11/2/26.
//

#ifndef ALICLOUD_ESP_IDF_ALICLOUD_OSS_H
#define ALICLOUD_ESP_IDF_ALICLOUD_OSS_H

#ifdef __cplusplus
extern "C" {



#endif

#include "alicloud_oss_response_parser.h"

#define OSS_ACCESS_KEY_SIZE 64
#define OSS_ACCESS_KEY_SECRET 64
#define OSS_REGION 32
#define OSS_BUCKET 64

typedef struct {
    char access_key_id[OSS_ACCESS_KEY_SIZE];
    char access_key_secret[OSS_ACCESS_KEY_SECRET];
    char region[OSS_REGION];
    char bucket[OSS_BUCKET];
} alicloud_oss_config_t;

typedef struct alicloud_oss_writer_t *alicloud_oss_writer_handler_t;

struct alicloud_oss_writer_t {
    esp_err_t (*open)(struct alicloud_oss_writer_t *writer, const char *fileId, size_t fileSize);

    esp_err_t (*write)(struct alicloud_oss_writer_t *client, void *data, size_t data_len);

    esp_err_t (*close)(struct alicloud_oss_writer_t *client);
};

esp_err_t alicloud_oss_writer_open(struct alicloud_oss_writer_t *writer, const char *fileId, size_t file_size);

esp_err_t alicloud_oss_writer_write(struct alicloud_oss_writer_t *writer, void *data, size_t data_len);

esp_err_t alicloud_oss_writer_close(struct alicloud_oss_writer_t *client);

typedef struct alicloud_oss_reader_t *alicloud_oss_read_handler_t;

struct alicloud_oss_reader_t {
    esp_err_t (*open)(struct alicloud_oss_reader_t *writer, const char *fileId);

    esp_err_t (*read)(struct alicloud_oss_reader_t *client, void *data, int data_len, int *read_len);

    esp_err_t (*close)(struct alicloud_oss_reader_t *client);
};

esp_err_t alicloud_oss_reader_open(struct alicloud_oss_reader_t *reader, const char *fileId);

esp_err_t alicloud_oss_reader_read(struct alicloud_oss_reader_t *reader, void *data, int data_len, int *read_len);

esp_err_t alicloud_oss_reader_close(struct alicloud_oss_reader_t *reader);

struct alicloud_oss_dir_walker_callback_t {
    esp_err_t (*on_elem)(struct alicloud_oss_dir_walker_callback_t *client, const char *prefix,
                         alicloud_oss_object_t *elem);

    esp_err_t (*on_dir)(struct alicloud_oss_dir_walker_callback_t *client, const char *prefix, const char *dir_name);
};

typedef struct alicloud_oss_dir_walker_t *alicloud_oss_dir_walker_handler_t;

struct alicloud_oss_dir_walker_t {
    esp_err_t (*open)(struct alicloud_oss_dir_walker_t *walker, const char *prefix,
                      struct alicloud_oss_dir_walker_callback_t *callback);

    esp_err_t (*walk)(struct alicloud_oss_dir_walker_t *walker, struct alicloud_oss_dir_walker_callback_t *callback);

    esp_err_t (*close)(struct alicloud_oss_dir_walker_t *walker);
};

esp_err_t alicloud_oss_dir_walker_open(struct alicloud_oss_dir_walker_t *walker, const char *prefix,
                                       struct alicloud_oss_dir_walker_callback_t *callback);

esp_err_t alicloud_oss_dir_walker_walk(struct alicloud_oss_dir_walker_t *walker,
                                       struct alicloud_oss_dir_walker_callback_t *callback);

esp_err_t alicloud_oss_dir_walker_close(struct alicloud_oss_dir_walker_t *walker);


#ifdef __cplusplus
}
#endif

#endif //ALICLOUD_ESP_IDF_ALICLOUD_OSS_H
