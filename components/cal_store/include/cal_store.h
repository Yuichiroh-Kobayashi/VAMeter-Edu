/*
 * SPDX-FileCopyrightText: 2024 Yuichiro Kobayashi
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// VAMeter Education Custom Firmware
// Calibration Store Component Header (cal_store.h)
//
// Defines the data structure and public API for the calibration data store.
// Based on "VAMeter_補正機能_統合仕様書.md".

#define CAL_POINTS 10

// 仕様書 7.2. 構造体
typedef struct {
    uint16_t version;        // =1
    uint16_t flags;          // 予約（bit0: valid）
    uint16_t fs_scaled;      // FS（保存単位）
    int16_t  error[CAL_POINTS]; // 1/10..10/10の誤差（保存単位）
    uint32_t timestamp;      // UNIX秒（最終更新）
    uint8_t  gen;            // 世代番号（ダブルバッファ）
    uint8_t  operator_id;    // 担当者ID（任意）
    uint16_t crc16;          // version〜operator_id のCRC-16
} cal_block_t;

// 種別・レンジ識別
typedef enum { CAL_TYPE_AMP = 'A', CAL_TYPE_VOLT = 'V' } cal_type_t;
typedef enum { CAL_RANGE_L = 'L', CAL_RANGE_M = 'M', CAL_RANGE_S = 'S' } cal_range_t;

#define CAL_NS "cal"   // NVS namespace

// ---- 初期化・キー作成
esp_err_t cal_store_init(void); // nvs_flash_init の後に呼ぶ
// 例: A,12,L -> "A12_L"
void cal_store_make_key(cal_type_t type, uint8_t pair, cal_range_t range, char out[8]);

// ---- 読み書き（ダブルバッファ）
esp_err_t cal_store_load(cal_type_t type, uint8_t pair, cal_range_t range, cal_block_t *out, bool *out_valid);
// RAM上のブロックを保存（_0/_1 を自動で切替）
esp_err_t cal_store_save(cal_type_t type, uint8_t pair, cal_range_t range, const cal_block_t *in);

// ---- ユーティリティ
uint16_t cal_crc16_ccitt(const uint8_t *data, uint32_t len);
bool cal_block_calc_and_set_crc(cal_block_t *blk);
bool cal_block_verify_crc(const cal_block_t *blk);

// 量子化ヘルパ（保存単位 <-> 物理単位）
static inline int16_t cal_quantize(float physical, float unit_scale) {
    // 四捨五入 + 飽和
    float q = physical / unit_scale;
    if (q >  32767.0f) return  32767;
    if (q < -32768.0f) return -32768;
    return (int16_t)((q >= 0.f) ? (q + 0.5f) : (q - 0.5f));
}
static inline float cal_dequantize(int16_t scaled, float unit_scale) {
    return (float)scaled * unit_scale;
}

#ifdef __cplusplus
}
#endif