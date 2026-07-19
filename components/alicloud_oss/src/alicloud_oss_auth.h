//
// Created by darvik on 29.12.2024.
//

#pragma once

#include <esp_err.h>
#include "alicloud_oss.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "net_tools.h"

/**
  * @brief  sign oss request
**/
esp_err_t alicloud_oss_sign_v4(alicloud_oss_config_t* config, http_method_t method, uri_t* uri, http_headers_t* headers);

#ifdef __cplusplus
}
#endif
