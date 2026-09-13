/*
* SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
*
* SPDX-License-Identifier: MIT
*/
#include <app.h>
#include <mooncake.h>
#include "hal_vameter/hal_vameter.h"
#include "hal_vameter/components/d2b_runtime_evidence.h"
#include <esp_partition.h>
#include "libs/asset_pool_layout/asset_pool_layout.h"
#include <cstring>
#include <nvs_flash.h>
#include "esp_log.h"

extern "C" void app_main(void)
{
    D2B_RUNTIME_EVIDENCE::LogBoot();
    ESP_LOGI("boot", "HELLO from app_main");
    spdlog::set_pattern("[%H:%M:%S] [%L] %v");
    // spdlog::set_level(spdlog::level::warn);

    APP::SetupCallback_t callback;

    callback.AssetPoolInjection = []() -> bool {
        using namespace ASSET_POOL_LAYOUT;
        const esp_partition_t* part =
            esp_partition_find_first(static_cast<esp_partition_type_t>(233),
                                     static_cast<esp_partition_subtype_t>(0x23), nullptr);
        if (part == nullptr)
        {
            spdlog::error("ASSETPOOL_LAYOUT_REJECTED reason=PartitionMissing");
            return false;
        }
        if (part->size != kAssetPoolPartitionBytes)
        {
            spdlog::error("ASSETPOOL_LAYOUT_REJECTED reason=PartitionSizeMismatch");
            return false;
        }
        const void* mapped = nullptr;
        esp_partition_mmap_handle_t handle;
        const esp_err_t err = esp_partition_mmap(part, 0, kAssetPoolPartitionBytes,
                                                ESP_PARTITION_MMAP_DATA, &mapped, &handle);
        if (err != ESP_OK)
        {
            spdlog::error("ASSETPOOL_LAYOUT_REJECTED reason=MapFailed error={}", esp_err_to_name(err));
            return false;
        }
        std::uint8_t trailer[kTrailerReserveBytes];
        std::memcpy(trailer, static_cast<const std::uint8_t*>(mapped) + kTrailerOffset, sizeof(trailer));
        const Result result = ValidateStaticAsset(mapped, sizeof(StaticAsset_t), trailer, sizeof(trailer));
        if (result != Result::Ok)
        {
            spdlog::error("ASSETPOOL_LAYOUT_REJECTED reason={}", ResultName(result));
            esp_partition_munmap(handle);
            return false;
        }
        nvs_flash_init();
        // No StaticAsset_t member is accessed before the complete layout/CRC gate.
        if (!AssetPool::InjectStaticAsset(static_cast<StaticAsset_t*>(const_cast<void*>(mapped))))
        {
            spdlog::error("ASSETPOOL_LAYOUT_REJECTED reason=InjectionFailed");
            esp_partition_munmap(handle);
            return false;
        }
        return true; // The injected pool owns the mapping for the application lifetime.
    };

    callback.HalInjection = []() { HAL::Inject(new HAL_VAMeter); };

    if (!APP::Setup(callback))
    {
        // IDF v5.1.6 deletes its main task when app_main returns. HAL/watchdog
        // initialization has not run; no asset-backed UI or reboot loop is entered.
        ESP_LOGE("boot", "ASSETPOOL_TIER1_FAIL_STOP");
        return;
    }

    while (1)
    {
        APP::Loop();
    }
}
