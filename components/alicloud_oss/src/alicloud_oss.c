//
// Created by Ivan Kishchenko on 14/2/26.
//

#include "alicloud_oss.h"

#include <esp_check.h>

static const char *TAG = "ali_oss";

esp_err_t alicloud_oss_writer_open(struct alicloud_oss_writer_t *writer, const char *fileId, size_t file_size) {
    ESP_RETURN_ON_FALSE(writer, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return writer->open(writer, fileId, file_size);
}

esp_err_t alicloud_oss_writer_write(struct alicloud_oss_writer_t *writer, void *data, size_t data_len) {
    ESP_RETURN_ON_FALSE(writer, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return writer->write(writer, data, data_len);

}

esp_err_t alicloud_oss_writer_close(struct alicloud_oss_writer_t *writer) {
    ESP_RETURN_ON_FALSE(writer, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return writer->close(writer);
}


esp_err_t alicloud_oss_reader_open(struct alicloud_oss_reader_t *reader, const char *fileId) {
    ESP_RETURN_ON_FALSE(reader, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return reader->open(reader, fileId);
}

esp_err_t alicloud_oss_reader_read(struct alicloud_oss_reader_t *reader, void *data, int data_len, int *read_len) {
    ESP_RETURN_ON_FALSE(reader, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return reader->read(reader, data, data_len, read_len);

}

esp_err_t alicloud_oss_reader_close(struct alicloud_oss_reader_t *reader) {
    ESP_RETURN_ON_FALSE(reader, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return reader->close(reader);
}

esp_err_t alicloud_oss_dir_walker_open(struct alicloud_oss_dir_walker_t *walker, const char *prefix,
                                       struct alicloud_oss_dir_walker_callback_t *callback) {
    ESP_RETURN_ON_FALSE(walker, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return walker->open(walker, prefix, callback);
}

esp_err_t alicloud_oss_dir_walker_walk(struct alicloud_oss_dir_walker_t *walker,
                                       struct alicloud_oss_dir_walker_callback_t *callback) {
    ESP_RETURN_ON_FALSE(walker, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return walker->walk(walker, callback);
}

esp_err_t alicloud_oss_dir_walker_close(struct alicloud_oss_dir_walker_t *walker) {
    ESP_RETURN_ON_FALSE(walker, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    return walker->close(walker);
}
