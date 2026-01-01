/*
 * VAMeter Education Custom Firmware
 * Calibration Store Component Implementation (cal_store.c)
 *
 * SPDX-FileCopyrightText: 2025 Yuichiro Kobayashi
 * SPDX-License-Identifier: MIT
 */

#include "cal_store.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>          // offsetof
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_crc.h"         // esp_crc16_be()

// ---- ログタグ & NVS名前空間
static const char* TAG = "cal_store";
static const char* NVS_NAMESPACE = CAL_NS; // "cal"

// ---- キー生成: "A12_L"
void cal_store_make_key(cal_type_t type, uint8_t pair, cal_range_t range, char out[8]) {
    // 例: 'A',12,'M' -> "A12_M"
    // バッファは最低 8 バイト（"A12_M\0"）想定
    (void)snprintf(out, 8, "%c%02u_%c", (char)type, (unsigned)pair, (char)range);
}

// 内部ユーティリティ: サフィックス付きキー "A12_L_0" / "A12_L_1"
static void make_key_with_suffix(const char* base, uint8_t suffix, char out[16]) {
    // out は最低 16 バイト程度を想定
    (void)snprintf(out, 16, "%s_%u", base, (unsigned)(suffix ? 1 : 0));
}

// ---- CRC-16/CCITT-FALSE (poly=0x1021, init=0xFFFF, 非反射) を esp_crc16_be で計算
uint16_t cal_crc16_ccitt(const uint8_t *data, uint32_t len) {
    return esp_crc16_be(0xFFFF, data, (size_t)len);
}

// 仕様: version 〜 operator_id まで（末尾の crc16 は含めない）
static uint16_t calc_crc_without_tail(const cal_block_t *blk) {
    const uint8_t* start = (const uint8_t*)&blk->version;
    size_t len = offsetof(cal_block_t, crc16) - offsetof(cal_block_t, version);
    return cal_crc16_ccitt(start, (uint32_t)len);
}

bool cal_block_calc_and_set_crc(cal_block_t *blk) {
    blk->crc16 = calc_crc_without_tail(blk);
    return true;
}

bool cal_block_verify_crc(const cal_block_t *blk) {
    return blk->crc16 == calc_crc_without_tail(blk);
}

// ---- 初期化（ここでは何もしない：nvs_flash_init はアプリ側責務）
esp_err_t cal_store_init(void) {
    return ESP_OK;
}

// ---- 低レベル: NVS blob 読み
static esp_err_t nvs_read_blob(const char *key, void *buf, size_t buf_size, bool *out_found) {
    *out_found = false;

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // 名前空間自体が無い
        return ESP_OK;
    }
    if (err != ESP_OK) return err;

    size_t required = buf_size;  // get_blob は required を更新する
    err = nvs_get_blob(h, key, buf, &required);
    nvs_close(h);

    if (err == ESP_OK && required == buf_size) {
        *out_found = true;
        return ESP_OK;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK; // 見つからないのは正常系
    }
    return err;
}

// ---- 低レベル: NVS blob 書き
static esp_err_t nvs_write_blob(const char *key, const void *buf, size_t len) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(h, key, buf, len);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

// ---- 読み出し（ダブルバッファ: _0 / _1 から新しい方を選ぶ）
// out_valid=true: 有効ブロック、false: 未設定 or 破損
esp_err_t cal_store_load(cal_type_t type, uint8_t pair, cal_range_t range, cal_block_t *out, bool *out_valid) {
    if (!out || !out_valid) return ESP_ERR_INVALID_ARG;

    char base[8];  cal_store_make_key(type, pair, range, base);
    char k0[16];   make_key_with_suffix(base, 0, k0);
    char k1[16];   make_key_with_suffix(base, 1, k1);

    cal_block_t b0 = {0}, b1 = {0};
    bool f0=false, f1=false;
    esp_err_t err;

    err = nvs_read_blob(k0, &b0, sizeof(b0), &f0); if (err != ESP_OK) return err;
    err = nvs_read_blob(k1, &b1, sizeof(b1), &f1); if (err != ESP_OK) return err;

    *out_valid = false;

    bool c0 = f0 && cal_block_verify_crc(&b0);
    bool c1 = f1 && cal_block_verify_crc(&b1);

    if (!c0 && !c1) {
        // どちらも無い or どちらもCRC不一致 → 未設定
        memset(out, 0, sizeof(*out));
        out->version = 0xFFFF;  // 未設定の見分け
        return ESP_OK;
    }

    // 両方有効 → genで新しさを決める。同点なら timestamp 大きい方。
    if (c0 && c1) {
        const cal_block_t* pick =
            (b1.gen != b0.gen) ? ((b1.gen > b0.gen) ? &b1 : &b0)
                               : ((b1.timestamp > b0.timestamp) ? &b1 : &b0);
        *out = *pick;
        *out_valid = true;
        ESP_LOGD(TAG, "Loaded %s (both valid, gen=%u)", (pick == &b1) ? k1 : k0, pick->gen);
        return ESP_OK;
    }

    // 片方だけ有効
    *out = c1 ? b1 : b0;
    *out_valid = true;
    ESP_LOGD(TAG, "Loaded %s (single valid)", c1 ? k1 : k0);
    return ESP_OK;
}

// ---- 書き込み（ダブルバッファ: 現行の逆側にトグル）
// in は呼び出し側のRAM、ここで gen と CRC を上書きしたコピーを保存
esp_err_t cal_store_save(cal_type_t type, uint8_t pair, cal_range_t range, const cal_block_t *in) {
    if (!in) return ESP_ERR_INVALID_ARG;

    char base[8];  cal_store_make_key(type, pair, range, base);
    char kdst[16];

    // 現行の世代を確認
    cal_block_t cur = {0};
    bool cur_valid = false;
    esp_err_t err = cal_store_load(type, pair, range, &cur, &cur_valid);
    if (err != ESP_OK) return err;

    // 次は 0/1 トグル
    uint8_t next_gen = cur_valid ? ((cur.gen == 0) ? 1 : 0) : 0;
    make_key_with_suffix(base, next_gen, kdst);

    // 書き込み用コピーを作成
    cal_block_t wr = *in;
    wr.gen = next_gen;
    cal_block_calc_and_set_crc(&wr);

    ESP_LOGI(TAG, "Save %s (gen=%u, ver=%u, ts=%u)", kdst, wr.gen, wr.version, (unsigned)wr.timestamp);
    return nvs_write_blob(kdst, &wr, sizeof(wr));
}
