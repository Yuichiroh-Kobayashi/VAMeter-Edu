#pragma once

#include "esp_http_server.h"

#define ESP_LOGE(...) ((void)0)

#ifdef __cplusplus
extern "C"
{
#endif
    const char* esp_err_to_name(esp_err_t error);
#ifdef __cplusplus
}
#endif
