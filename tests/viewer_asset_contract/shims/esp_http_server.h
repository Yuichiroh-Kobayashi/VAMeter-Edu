#pragma once

#include <cstddef>

typedef int esp_err_t;
typedef void* httpd_handle_t;
struct httpd_req;
typedef struct httpd_req httpd_req_t;
typedef int httpd_method_t;
typedef int httpd_err_code_t;

static const esp_err_t ESP_OK = 0;
static const httpd_method_t HTTP_GET = 0;
static const httpd_err_code_t HTTPD_500_INTERNAL_SERVER_ERROR = 500;

typedef struct
{
    const char* uri;
    httpd_method_t method;
    esp_err_t (*handler)(httpd_req_t*);
    void* user_ctx;
    bool is_websocket;
    bool handle_ws_control_frames;
    const char* supported_subprotocol;
} httpd_uri_t;

#ifdef __cplusplus
extern "C"
{
#endif
    esp_err_t httpd_register_uri_handler(httpd_handle_t server, const httpd_uri_t* uri);
    esp_err_t httpd_unregister_uri_handler(httpd_handle_t server, const char* uri, httpd_method_t method);
    esp_err_t httpd_resp_set_hdr(httpd_req_t* request, const char* field, const char* value);
    esp_err_t httpd_resp_set_status(httpd_req_t* request, const char* status);
    esp_err_t httpd_resp_set_type(httpd_req_t* request, const char* type);
    esp_err_t httpd_resp_send(httpd_req_t* request, const char* bytes, std::size_t size);
    esp_err_t httpd_resp_send_err(httpd_req_t* request, httpd_err_code_t error, const char* message);
#ifdef __cplusplus
}
#endif
