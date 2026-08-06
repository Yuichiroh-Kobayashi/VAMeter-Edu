/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "../hal_vameter.h"
#include "../hal_config.h"
#include "../../../app/assets/assets.h"
#include "libs/local_csv_download/local_csv_download_name.h"
#include "libs/local_csv_download/local_csv_download_selection.h"
#include "libs/local_csv_download/local_csv_stream.h"
#include "libs/web_server_owner/web_server_owner.h"
#include "libs/web_server_owner/web_server_results.h"
#include "libs/web_server_owner/web_server_transaction.h"
#include "d2b_esp_transport.h"
#include "d2b_httpd_stack_diag.h"
#include "d2b_runtime_evidence.h"
#include <mooncake.h>
#include <Arduino.h>
#include <PsychicHttp.h>
#include <FS.h>
#include <vfs_api.h>
#include <cstdlib>
#include <memory>
#include <new>

// class VFS_t : public FS
// {
// public:
//     VFS_t() : FS(FSImplPtr(new VFSImpl())) {}
// };
// static VFS_t VFS;

namespace
{
    class OwnedPsychicHttpServer : public PsychicHttpServer
    {
    public:
        ~OwnedPsychicHttpServer() override
        {
            // PsychicEndpoint has no destructor for its handler pointer in
            // the pinned PsychicHttp revision.  The wrapper therefore owns
            // and releases endpoint/default handlers before the base class
            // releases the endpoint objects themselves.  The wrapper itself
            // remains owned by ESP-IDF global_user_ctx_free_fn or unique_ptr.
            for (std::list<PsychicEndpoint*>::iterator endpoint = _endpoints.begin(); endpoint != _endpoints.end(); ++endpoint)
                delete (*endpoint)->handler();
            delete defaultEndpoint->handler();
        }
    };

    PsychicHttpServer* _http_server = nullptr;
    WEB_SERVER_OWNER::State _http_server_owner;

    RUNTIME_EVIDENCE::Owner RuntimeOwner(WEB_SERVER_OWNER::Owner owner)
    {
        switch (owner)
        {
        case WEB_SERVER_OWNER::Owner::System:
            return RUNTIME_EVIDENCE::Owner::System;
        case WEB_SERVER_OWNER::Owner::Download:
            return RUNTIME_EVIDENCE::Owner::Download;
        case WEB_SERVER_OWNER::Owner::None:
        default:
            return RUNTIME_EVIDENCE::Owner::None;
        }
    }

    struct HttpdTransactionContext
    {
        WEB_SERVER_OWNER::Owner owner;
    };

    httpd_handle_t HttpdHandleFromKey(std::uintptr_t rawServerHandleKey)
    {
        return reinterpret_cast<httpd_handle_t>(rawServerHandleKey);
    }

    bool StopHttpdCallback(void*, std::uintptr_t rawServerHandleKey)
    {
        return httpd_stop(HttpdHandleFromKey(rawServerHandleKey)) == ESP_OK;
    }

    void ClearHttpdWrapperAfterStopCallback(void*, std::uintptr_t)
    {
        // httpd_stop() has already consumed the raw handle.  Clear the
        // published wrapper immediately, before lifecycle callbacks and
        // owner release; no wrapper dereference or delete is performed here.
        _http_server = nullptr;
    }

    void PrepareHttpdStopCallback(void* context, std::uintptr_t rawServerHandleKey)
    {
        const HttpdTransactionContext* transaction = static_cast<const HttpdTransactionContext*>(context);
        if (transaction != nullptr && transaction->owner == WEB_SERVER_OWNER::Owner::System)
            D2B_ESP::PrepareServerStop(HttpdHandleFromKey(rawServerHandleKey));
    }

    void AfterHttpdStopCallback(void* context, std::uintptr_t rawServerHandleKey)
    {
        const HttpdTransactionContext* transaction = static_cast<const HttpdTransactionContext*>(context);
        if (transaction != nullptr && transaction->owner == WEB_SERVER_OWNER::Owner::System)
            D2B_ESP::AfterServerStopped(HttpdHandleFromKey(rawServerHandleKey));
    }

    void FailedHttpdStopCallback(void* context, std::uintptr_t rawServerHandleKey)
    {
        const HttpdTransactionContext* transaction = static_cast<const HttpdTransactionContext*>(context);
        if (transaction != nullptr && transaction->owner == WEB_SERVER_OWNER::Owner::System)
            D2B_ESP::ServerStopFailed(HttpdHandleFromKey(rawServerHandleKey));
    }

    WEB_SERVER_OWNER::StartResult StartOwnedHttpServer(WEB_SERVER_OWNER::Owner owner, RUNTIME_EVIDENCE::Reason reason)
    {
        const RUNTIME_EVIDENCE::Owner runtimeOwner = RuntimeOwner(owner);
        D2B_RUNTIME_EVIDENCE::LogServerRequest(RUNTIME_EVIDENCE::Event::ServerStart,
                                               runtimeOwner,
                                               reason,
                                               _http_server_owner.generation());
        const WEB_SERVER_OWNER::StartPreflightResult preflight =
            WEB_SERVER_OWNER::StartPreflight(_http_server_owner, owner, _http_server != nullptr);
        if (preflight != WEB_SERVER_OWNER::StartPreflightResult::Proceed)
        {
            D2B_RUNTIME_EVIDENCE::LogServerResult(RUNTIME_EVIDENCE::Event::ServerStart,
                                                   runtimeOwner,
                                                   RUNTIME_EVIDENCE::Reason::ServerStartFailed,
                                                   RUNTIME_EVIDENCE::Result::Rejected,
                                                   _http_server_owner.generation());
            spdlog::error("port 80 already owned by another server lifecycle");
            return preflight == WEB_SERVER_OWNER::StartPreflightResult::RetainedServerNeedsStopRetry
                       ? WEB_SERVER_OWNER::StartResult::RetainedServerNeedsStopRetry
                       : WEB_SERVER_OWNER::StartResult::BusyOtherOwner;
        }
        if (!_http_server_owner.acquire(owner))
        {
            D2B_RUNTIME_EVIDENCE::LogServerResult(RUNTIME_EVIDENCE::Event::ServerStart,
                                                   runtimeOwner,
                                                   RUNTIME_EVIDENCE::Reason::ServerStartFailed,
                                                   RUNTIME_EVIDENCE::Result::Rejected,
                                                   _http_server_owner.generation());
            spdlog::error("port 80 owner acquisition raced with another lifecycle");
            return WEB_SERVER_OWNER::StartResult::BusyOtherOwner;
        }
        const std::uint32_t generation = _http_server_owner.generation();
        D2B_RUNTIME_EVIDENCE::SetServerGeneration(generation);

        std::unique_ptr<OwnedPsychicHttpServer> ownedServer(new (std::nothrow) OwnedPsychicHttpServer);
        if (!ownedServer)
        {
            const WEB_SERVER_OWNER::TransactionRequest cleanupRequest = {
                &_http_server_owner,
                owner,
                0,
                0,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
            };
            const WEB_SERVER_OWNER::PartialCleanupOutcome cleanup =
                WEB_SERVER_OWNER::CleanupPartial(cleanupRequest);
            const bool released = cleanup.result == WEB_SERVER_OWNER::StartResult::AllocationOrListenFailure;
            D2B_RUNTIME_EVIDENCE::LogServerResult(RUNTIME_EVIDENCE::Event::ServerStart,
                                                   runtimeOwner,
                                                   RUNTIME_EVIDENCE::Reason::ServerStartFailed,
                                                   RUNTIME_EVIDENCE::Result::Failed,
                                                   generation);
            spdlog::error("failed to allocate port 80 server");
            return released ? WEB_SERVER_OWNER::StartResult::AllocationOrListenFailure
                             : WEB_SERVER_OWNER::StartResult::RetainedServerNeedsStopRetry;
        }

        if (owner == WEB_SERVER_OWNER::Owner::System)
        {
            ownedServer->onClose(
                [](PsychicClient* client) { D2B_ESP::OnClientClosed(client->server(), client->socket()); });
            const std::uint32_t actualStackBytes = static_cast<std::uint32_t>(ownedServer->config.stack_size);
            D2B_HTTPD_STACK_DIAG::SetConfiguredStackBytes(actualStackBytes);
        }

        ownedServer->server = nullptr;
        const esp_err_t listenResult = ownedServer->listen(80);
        if (listenResult != ESP_OK)
        {
            spdlog::error("port 80 listen failed: {}", esp_err_to_name(listenResult));
            const httpd_handle_t partialHandle = ownedServer->server;
            WEB_SERVER_OWNER::PartialCleanupOutcome cleanup = {
                WEB_SERVER_OWNER::StartResult::RetainedServerNeedsStopRetry,
                reinterpret_cast<std::uintptr_t>(ownedServer.get()),
            };
            if (partialHandle == nullptr)
            {
                const WEB_SERVER_OWNER::TransactionRequest cleanupRequest = {
                    &_http_server_owner,
                    owner,
                    0,
                    0,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                };
                cleanup = WEB_SERVER_OWNER::CleanupPartial(cleanupRequest);
            }
            else
            {
                // Once httpd_start returned a handle, ESP-IDF owns the
                // wrapper through global_user_ctx_free_fn.  Transfer that
                // ownership before stopping; never delete the wrapper here.
                OwnedPsychicHttpServer* retainedServer = ownedServer.release();
                const WEB_SERVER_OWNER::TransactionRequest cleanupRequest = {
                    &_http_server_owner,
                    owner,
                    reinterpret_cast<std::uintptr_t>(retainedServer),
                    reinterpret_cast<std::uintptr_t>(partialHandle),
                    nullptr,
                    nullptr,
                    StopHttpdCallback,
                    nullptr,
                    nullptr,
                    nullptr,
                };
                cleanup = WEB_SERVER_OWNER::CleanupPartial(cleanupRequest);
                _http_server = reinterpret_cast<PsychicHttpServer*>(cleanup.wrapperKey);
                if (cleanup.result == WEB_SERVER_OWNER::StartResult::RetainedServerNeedsStopRetry)
                    spdlog::critical("failed to stop partially started port 80 server");
            }
            D2B_RUNTIME_EVIDENCE::LogServerResult(RUNTIME_EVIDENCE::Event::ServerStart,
                                                   runtimeOwner,
                                                   RUNTIME_EVIDENCE::Reason::ServerStartFailed,
                                                   RUNTIME_EVIDENCE::Result::Failed,
                                                   generation);
            return cleanup.result;
        }

        _http_server = ownedServer.release();
        // A successful listen owns a live wrapper; only a stop failure marks
        // this owner as retained.
        D2B_RUNTIME_EVIDENCE::LogServerResult(RUNTIME_EVIDENCE::Event::ServerStart,
                                               runtimeOwner,
                                               reason,
                                               RUNTIME_EVIDENCE::Result::Succeeded,
                                               generation);
        if (owner == WEB_SERVER_OWNER::Owner::System)
            D2B_HTTPD_STACK_DIAG::LogConfiguredStack();
        return WEB_SERVER_OWNER::StartResult::Started;
    }

    WEB_SERVER_OWNER::StartResult MapApStartResult(WEB_SERVER_OWNER::ApStartResult result)
    {
        switch (result)
        {
        case WEB_SERVER_OWNER::ApStartResult::Started:
            return WEB_SERVER_OWNER::StartResult::Started;
        case WEB_SERVER_OWNER::ApStartResult::StaDisconnectFailed:
        case WEB_SERVER_OWNER::ApStartResult::StartFailed:
            return WEB_SERVER_OWNER::StartResult::ApStartFailed;
        case WEB_SERVER_OWNER::ApStartResult::StopRetryRequired:
        default:
            return WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry;
        }
    }

    WEB_SERVER_OWNER::StopResult StopOwnedHttpServer(WEB_SERVER_OWNER::Owner owner, RUNTIME_EVIDENCE::Reason reason)
    {
        const RUNTIME_EVIDENCE::Owner runtimeOwner = RuntimeOwner(owner);
        D2B_RUNTIME_EVIDENCE::LogServerRequest(RUNTIME_EVIDENCE::Event::ServerStop,
                                               runtimeOwner,
                                               reason,
                                               _http_server_owner.generation());
        const std::uint32_t generation = _http_server_owner.generation();
        // Capture the raw handle while the PsychicHttp wrapper is valid.  No
        // wrapper field or method is touched after a successful stop callback.
        const std::uintptr_t wrapperKey = reinterpret_cast<std::uintptr_t>(_http_server);
        std::uintptr_t rawServerHandleKey = 0;
        if (_http_server != nullptr && _http_server_owner.owner() == owner)
            rawServerHandleKey = reinterpret_cast<std::uintptr_t>(_http_server->server);
        HttpdTransactionContext transactionContext = {owner};
        const WEB_SERVER_OWNER::TransactionRequest stopRequest = {
            &_http_server_owner,
            owner,
            wrapperKey,
            rawServerHandleKey,
            &transactionContext,
            PrepareHttpdStopCallback,
            StopHttpdCallback,
            ClearHttpdWrapperAfterStopCallback,
            AfterHttpdStopCallback,
            FailedHttpdStopCallback,
        };
        const WEB_SERVER_OWNER::StopTransactionOutcome outcome = WEB_SERVER_OWNER::StopOwned(stopRequest);
        _http_server = reinterpret_cast<PsychicHttpServer*>(outcome.wrapperKey);
        D2B_RUNTIME_EVIDENCE::LogServerResult(RUNTIME_EVIDENCE::Event::ServerStop,
                                               runtimeOwner,
                                               WEB_SERVER_OWNER::IsStopSuccessful(outcome.result)
                                                   ? reason
                                                   : RUNTIME_EVIDENCE::Reason::ServerStopFailed,
                                               WEB_SERVER_OWNER::IsStopSuccessful(outcome.result)
                                                   ? RUNTIME_EVIDENCE::Result::Completed
                                                   : RUNTIME_EVIDENCE::Result::Failed,
                                               generation);
        return outcome.result;
    }
} // namespace

/* -------------------------------------------------------------------------- */
/*                                    Pages                                   */
/* -------------------------------------------------------------------------- */
class MyChunkResponse : public PsychicResponse
{
private:
    uint8_t* _src = nullptr;
    size_t _size = 0;

public:
    MyChunkResponse(PsychicRequest* request, const String& contentType, uint8_t* src, size_t size)
        : PsychicResponse(request), _src(src), _size(size)
    {
        setContentType(contentType.c_str());
    }

    ~MyChunkResponse() {}

    esp_err_t send()
    {
        esp_err_t err = ESP_OK;

        // just send small files directly
        if (_size < FILE_CHUNK_SIZE)
        {
            this->setContent(_src, _size);
            err = PsychicResponse::send();
        }
        else
        {
            /* Retrieve the pointer to scratch buffer for temporary storage */
            char* chunk = (char*)malloc(FILE_CHUNK_SIZE);
            if (chunk == NULL)
            {
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(this->_request->request(), HTTPD_500_INTERNAL_SERVER_ERROR, "Unable to allocate memory.");
                return ESP_FAIL;
            }

            this->sendHeaders();

            size_t chunk_index = 0;
            size_t chunk_size = FILE_CHUNK_SIZE;
            while (1)
            {
                // Send chunk
                err = this->sendChunk(_src + chunk_index, chunk_size);
                if (err != ESP_OK)
                    break;

                chunk_index += chunk_size;
                if (chunk_index + FILE_CHUNK_SIZE > _size)
                    chunk_size = _size - chunk_index;
                else
                    chunk_size = FILE_CHUNK_SIZE;

                if (chunk_size == 0)
                    break;
            }

            // keep track of our memory
            free(chunk);

            if (err == ESP_OK)
            {
                ESP_LOGI(PH_TAG, "File sending complete");
                this->finishChunking();
            }
        }

        return err;
    }
};

void HAL_VAMeter::_web_server_page_loading()
{
    _http_server->on("/", [&](PsychicRequest* request) { return request->redirect("/syscfg"); });

    _http_server->on("/syscfg",
                    [&](PsychicRequest* request)
                    {
                        MyChunkResponse response(request,
                                                 "text/html",
                                                 (uint8_t*)AssetPool::GetWebPage().syscfg,
                                                 sizeof(AssetPool::GetWebPage().syscfg));
                        return response.send();
                    });

    _http_server->on("/favicon.ico",
                    [&](PsychicRequest* request)
                    {
                        MyChunkResponse response(request,
                                                 "image/x-icon",
                                                 (uint8_t*)AssetPool::GetWebPage().favicon,
                                                 sizeof(AssetPool::GetWebPage().favicon));
                        return response.send();
                    });
}

void HAL_VAMeter::_print_stack_high_water_mark()
{
    TaskHandle_t task_handle = xTaskGetCurrentTaskHandle();
    UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(task_handle);
    spdlog::info("Stack high water mark: {} bytes", stack_high_water_mark * sizeof(StackType_t));
}

/* -------------------------------------------------------------------------- */
/*                                 Normal apis                                */
/* -------------------------------------------------------------------------- */
void HAL_VAMeter::_web_server_api_loading()
{
    _http_server->on("/api/get_net_info",
                    [&](PsychicRequest* request)
                    {
                        std::string string_buffer;
                        {
                            JsonDocument doc;
                            doc["mac"] = _get_mac();
                            doc["ip"] = _get_ip();

                            serializeJson(doc, string_buffer);
                        }
                        return request->reply(string_buffer.c_str());
                    });

    _http_server->on("/api/set_syscfg",
                    HTTP_POST,
                    [&](PsychicRequest* request)
                    {
                        // spdlog::info("get json:\n{}", request->body().c_str());
                        spdlog::info("handle set config");

                        // _print_stack_high_water_mark();

                        // Parse
                        {
                            JsonDocument doc;
                            DeserializationError error = deserializeJson(doc, request->body().c_str());
                            if (error != DeserializationError::Ok)
                            {
                                spdlog::error("json parse failed");
                                spdlog::error("get:\n{}", request->body().c_str());
                                return request->reply(500, "application/json", "{\"msg\":\"json parse failed\"}");
                            }

                            // Copy
                            std::string string_buffer;

                            string_buffer = doc["wifiSsid"].as<std::string>();
                            if (string_buffer != "null")
                                _config.wifiSsid = string_buffer;

                            string_buffer = doc["wifiPassword"].as<std::string>();
                            if (string_buffer != "null")
                                _config.wifiPassword = string_buffer;

                            // ...
                        }

                        // _print_stack_high_water_mark();

                        saveSystemConfig();

                        return request->reply(200, "application/json", "{\"msg\":\"ok\"}");
                    });

    _http_server->on("/api/get_wifi_list",
                    [&](PsychicRequest* request)
                    {
                        std::string string_buffer;

                        auto wifi_list = _get_wifi_list();

                        // Encode
                        JsonDocument doc;
                        for (int i = 0; i < wifi_list.size(); i++)
                        {
                            doc["wifiList"][i] = wifi_list[i];
                        }
                        serializeJson(doc, string_buffer);

                        return request->reply(string_buffer.c_str());
                    });

    _http_server->on("/api/get_syscfg",
                    [&](PsychicRequest* request)
                    {
                        std::string string_buffer = _create_config_json();
                        return request->reply(string_buffer.c_str());
                    });
}

/* -------------------------------------------------------------------------- */
/*                                 Web server                                 */
/* -------------------------------------------------------------------------- */
WEB_SERVER_OWNER::StartResult HAL_VAMeter::startWebServer(OnLogPageRenderCallback_t onLogPageRender,
                                                          bool autoWifiMode,
                                                          WebServerReason reason)
{
    // Reconcile the synchronous AP mode before any owner, STA, AP, or HTTPD
    // side effect.  Retained or unknown AP state is a fail-closed recovery
    // result even when the internal owner state is inactive.
    if (!_ap_start_preflight())
    {
        spdlog::error("web server start rejected: AP stop retry required");
        return WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry;
    }

    const WEB_SERVER_OWNER::StartPreflightResult preflight =
        WEB_SERVER_OWNER::StartPreflight(_http_server_owner,
                                         WEB_SERVER_OWNER::Owner::System,
                                         _http_server != nullptr);
    if (preflight != WEB_SERVER_OWNER::StartPreflightResult::Proceed)
    {
        return preflight == WEB_SERVER_OWNER::StartPreflightResult::RetainedServerNeedsStopRetry
                   ? WEB_SERVER_OWNER::StartResult::RetainedServerNeedsStopRetry
                   : WEB_SERVER_OWNER::StartResult::BusyOtherOwner;
    }

    // Auto wifi mode
    bool go_sta_mode = autoWifiMode;
HELL:
    if (go_sta_mode)
    {
        // Check valid
        if (_config.wifiSsid.empty())
            go_sta_mode = false;
        if (_config.wifiPassword.empty())
            go_sta_mode = false;

        // Try connect
        if (go_sta_mode)
            go_sta_mode = connectWifi(onLogPageRender, false);

        // If not
        if (!go_sta_mode)
            goto HELL;
    }
    else
    {
        onLogPageRender("start ap mode", true, true);
        const WEB_SERVER_OWNER::StartResult apStart = MapApStartResult(_start_ap_mode());
        if (apStart != WEB_SERVER_OWNER::StartResult::Started)
            return apStart;
    }

    onLogPageRender("start web server", true, true);
    const RUNTIME_EVIDENCE::Reason evidenceReason =
        reason == WebServerReason::NetworkSettingsStart ? RUNTIME_EVIDENCE::Reason::NetworkSettingsStart
                                                        : RUNTIME_EVIDENCE::Reason::OwnerAcquire;
    const WEB_SERVER_OWNER::StartResult ownedStart =
        StartOwnedHttpServer(WEB_SERVER_OWNER::Owner::System, evidenceReason);
    if (ownedStart != WEB_SERVER_OWNER::StartResult::Started)
    {
        if (WEB_SERVER_OWNER::ShouldStopApAfterStartFailure(ownedStart))
        {
            const WEB_SERVER_OWNER::ApStopResult apStop = _stop_ap_mode();
            if (apStop == WEB_SERVER_OWNER::ApStopResult::StopFailed)
                return WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry;
        }
        return ownedStart;
    }

    _web_server_page_loading();
    _web_server_api_loading();
    D2B_ESP::SetServerGeneration(_http_server_owner.generation());
    if (!D2B_ESP::Register(_http_server->server, _http_server_owner.generation()))
    {
        const WEB_SERVER_OWNER::StopResult cleanup =
            StopOwnedHttpServer(WEB_SERVER_OWNER::Owner::System, RUNTIME_EVIDENCE::Reason::ServerStartFailed);
        if (!WEB_SERVER_OWNER::IsStopSuccessful(cleanup))
            return WEB_SERVER_OWNER::StartResult::RetainedServerNeedsStopRetry;
        const WEB_SERVER_OWNER::ApStopResult apStop = _stop_ap_mode();
        if (apStop == WEB_SERVER_OWNER::ApStopResult::StopFailed)
            return WEB_SERVER_OWNER::StartResult::RetainedApNeedsStopRetry;
        return WEB_SERVER_OWNER::StartResult::RouteOrRegistrationFailure;
    }
    spdlog::info("web server started");

    return WEB_SERVER_OWNER::StartResult::Started;
}

WEB_SERVER_OWNER::StopResult HAL_VAMeter::stopWebServer(WebServerReason reason)
{
    spdlog::info("stop web server");

    const RUNTIME_EVIDENCE::Reason evidenceReason =
        reason == WebServerReason::NetworkSettingsIntentionalStop
            ? RUNTIME_EVIDENCE::Reason::NetworkSettingsIntentionalStop
            : RUNTIME_EVIDENCE::Reason::OwnerRelease;
    const WEB_SERVER_OWNER::StopResult serverResult =
        StopOwnedHttpServer(WEB_SERVER_OWNER::Owner::System, evidenceReason);
    if (serverResult == WEB_SERVER_OWNER::StopResult::RetryRequired ||
        serverResult == WEB_SERVER_OWNER::StopResult::RejectedWrongOwner)
        return serverResult;

    const WEB_SERVER_OWNER::ApStopResult apStop = _stop_ap_mode();
    if (apStop == WEB_SERVER_OWNER::ApStopResult::StopFailed)
        return WEB_SERVER_OWNER::StopResult::ApStopFailed;

    return serverResult;
}

std::string HAL_VAMeter::getSystemConfigUrl()
{
    std::string ret = "http://";
    ret += _get_ip();
    ret += "/syscfg";
    return ret;
}

/* -------------------------------------------------------------------------- */
/*                            Local Download Server                           */
/* -------------------------------------------------------------------------- */
static LOCAL_CSV_DOWNLOAD::DownloadSelection _download_selection;
namespace
{
    class ScopedFile
    {
    public:
        explicit ScopedFile(FILE* file) : _file(file) {}
        ~ScopedFile() { close(); }

        FILE* get() const { return _file; }

        int close()
        {
            if (_file == nullptr)
                return 0;

            FILE* file = _file;
            _file = nullptr;
            return fclose(file);
        }

    private:
        FILE* _file;
        ScopedFile(const ScopedFile&);
        ScopedFile& operator=(const ScopedFile&);
    };

    struct ScopedBuffer
    {
        explicit ScopedBuffer(std::uint8_t* buffer) : value(buffer) {}
        ~ScopedBuffer() { free(value); }

        std::uint8_t* value;

    private:
        ScopedBuffer(const ScopedBuffer&);
        ScopedBuffer& operator=(const ScopedBuffer&);
    };

    std::size_t ReadFileChunk(void* context, std::uint8_t* buffer, std::size_t bufferSize, bool* readFailed)
    {
        FILE* file = static_cast<FILE*>(context);
        const std::size_t readSize = fread(buffer, 1, bufferSize, file);
        *readFailed = ferror(file) != 0;
        return readSize;
    }

    struct HttpChunkSender
    {
        PsychicResponse* response;
        esp_err_t result;
    };

    bool SendHttpChunk(void* context, const std::uint8_t* data, std::size_t dataSize)
    {
        HttpChunkSender* sender = static_cast<HttpChunkSender*>(context);
        sender->result = sender->response->sendChunk(const_cast<std::uint8_t*>(data), dataSize);
        if (sender->result == ESP_OK)
            taskYIELD();
        return sender->result == ESP_OK;
    }

    bool FinishHttpChunks(void* context)
    {
        HttpChunkSender* sender = static_cast<HttpChunkSender*>(context);
        sender->result = sender->response->finishChunking();
        return sender->result == ESP_OK;
    }
} // namespace

bool HAL_VAMeter::startDownloadServer(const std::string& recordName)
{
    // Reconcile the synchronous AP mode before validation, selection
    // publication, owner acquisition, AP start, or HTTPD allocation.
    if (!_ap_start_preflight())
    {
        spdlog::error("download server start rejected: AP stop retry required");
        return false;
    }

    if (!LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(recordName))
    {
        spdlog::warn("reject local download for invalid record name: {}", recordName);
        return false;
    }

    spdlog::info("start download server for: {}", recordName);

    const WEB_SERVER_OWNER::StartPreflightResult preflight =
        WEB_SERVER_OWNER::StartPreflight(_http_server_owner,
                                         WEB_SERVER_OWNER::Owner::Download,
                                         _http_server != nullptr);
    if (preflight != WEB_SERVER_OWNER::StartPreflightResult::Proceed)
        return false;

    const std::string recordPath = _fs_get_rec_file_path(recordName);
    _download_selection.set(recordName, recordPath);

    // Publish the complete selection before making the AP reachable.
    const WEB_SERVER_OWNER::ApStartResult apStart = _start_ap_mode();
    if (apStart != WEB_SERVER_OWNER::ApStartResult::Started)
    {
        _download_selection.clear();
        if (apStart == WEB_SERVER_OWNER::ApStartResult::StopRetryRequired)
            spdlog::error("download server AP start cleanup retained: stop retry required");
        return false;
    }

    const WEB_SERVER_OWNER::StartResult ownedStart =
        StartOwnedHttpServer(WEB_SERVER_OWNER::Owner::Download, RUNTIME_EVIDENCE::Reason::DownloadStart);
    if (ownedStart != WEB_SERVER_OWNER::StartResult::Started)
    {
        _download_selection.clear();
        if (WEB_SERVER_OWNER::ShouldStopApAfterStartFailure(ownedStart))
        {
            const WEB_SERVER_OWNER::ApStopResult apStop = _stop_ap_mode();
            if (apStop == WEB_SERVER_OWNER::ApStopResult::StopFailed)
                spdlog::error("download server rollback retained AP: stop retry required");
        }
        return false;
    }

    // Add download endpoint
    _http_server->on("/download/*",
                             [](PsychicRequest* request)
                             {
                                 spdlog::info("download request: {}", request->path().c_str());

                                 const LOCAL_CSV_DOWNLOAD::DownloadSelectionSnapshot selection =
                                     _download_selection.snapshot();
                                 const std::string& fileName = selection.name;
                                 const std::string& filePath = selection.path;
                                 const std::string requestPath = request->path().c_str();
                                 static const std::string downloadPrefix = "/download/";

                                 if (requestPath.compare(0, downloadPrefix.size(), downloadPrefix) != 0)
                                 {
                                     spdlog::warn("download rejected unexpected path: {}", requestPath);
                                     return request->reply(404, "text/plain", "File not found");
                                 }

                                 std::string requestedName;
                                 const std::string encodedName = requestPath.substr(downloadPrefix.size());
                                 if (!LOCAL_CSV_DOWNLOAD::DecodeUrlComponentOnce(encodedName, requestedName) ||
                                     !LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(requestedName) ||
                                     !LOCAL_CSV_DOWNLOAD::IsAllowedRecordName(fileName) || filePath.empty() ||
                                     requestedName != fileName)
                                 {
                                     spdlog::warn("download rejected path/name mismatch: {}", requestPath);
                                     return request->reply(404, "text/plain", "File not found");
                                 }

                                 ScopedFile file(fopen(filePath.c_str(), "rb"));
                                 if (file.get() == nullptr)
                                 {
                                     spdlog::error("download fopen failed: {}", filePath);
                                     return request->reply(404, "text/plain", "File not found");
                                 }

                                 const std::size_t bufferSize = LOCAL_CSV_DOWNLOAD::kStreamBufferSize;
                                 ScopedBuffer buffer(static_cast<std::uint8_t*>(malloc(bufferSize)));
                                 if (buffer.value == nullptr)
                                 {
                                     spdlog::error("download chunk buffer allocation failed");
                                     return request->reply(500, "text/plain", "Memory allocation failed");
                                 }

                                 // Set Content-Disposition header for download
                                 const std::string header = "attachment; filename=\"" + fileName + "\"";

                                 PsychicResponse response(request);
                                 response.setContentType("text/csv");
                                 response.addHeader("Content-Disposition", header.c_str());
                                 response.sendHeaders();

                                 HttpChunkSender sender = {&response, ESP_OK};
                                 LOCAL_CSV_DOWNLOAD::StreamStats stats = {0, 0, 0};
                                 const LOCAL_CSV_DOWNLOAD::StreamResult streamResult =
                                     LOCAL_CSV_DOWNLOAD::StreamChunks(ReadFileChunk,
                                                                      file.get(),
                                                                      SendHttpChunk,
                                                                      &sender,
                                                                      buffer.value,
                                                                      bufferSize,
                                                                      &stats);

                                 const int closeResult = file.close();
                                 if (streamResult != LOCAL_CSV_DOWNLOAD::StreamResult::Complete || closeResult != 0)
                                 {
                                     spdlog::error("download stream failed, stream result: {}, send result: {}, close result: {}, bytes read: {}, bytes sent: {}",
                                                   static_cast<int>(streamResult),
                                                   sender.result,
                                                   closeResult,
                                                   stats.bytesRead,
                                                   stats.bytesSent);
                                     return streamResult == LOCAL_CSV_DOWNLOAD::StreamResult::SendFailed ? sender.result : ESP_FAIL;
                                 }

                                 const LOCAL_CSV_DOWNLOAD::StreamResult finishResult =
                                     LOCAL_CSV_DOWNLOAD::FinishChunks(FinishHttpChunks, &sender);
                                 spdlog::info("download response finished, result: {}, bytes: {}, chunks: {}",
                                              sender.result,
                                              stats.bytesSent,
                                              stats.chunksSent);
                                 return finishResult == LOCAL_CSV_DOWNLOAD::StreamResult::Complete ? ESP_OK : sender.result;
                         });

    spdlog::info("download server started");
    return true;
}

void HAL_VAMeter::stopDownloadServer()
{
    spdlog::info("stop download server");

    const WEB_SERVER_OWNER::StopResult serverResult =
        StopOwnedHttpServer(WEB_SERVER_OWNER::Owner::Download, RUNTIME_EVIDENCE::Reason::DownloadIntentionalStop);
    if (serverResult != WEB_SERVER_OWNER::StopResult::Stopped &&
        serverResult != WEB_SERVER_OWNER::StopResult::AlreadyStopped)
        return;

    _download_selection.clear();

    const WEB_SERVER_OWNER::ApStopResult apStop = _stop_ap_mode();
    if (apStop == WEB_SERVER_OWNER::ApStopResult::StopFailed)
        spdlog::error("download server AP stop failed: stop retry required");

}

std::string HAL_VAMeter::getLocalIP() { return _get_ip(); }
