#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 対象メータ番号の設定（起動時または切替時に呼ぶ）
void mp_set_pair(uint8_t pair);

// キャッシュを読む（起動時に1回呼ぶ）
void mp_init_cache(void);

// レンジ別の保存単位（物理量へのスケール）
float mp_unit_scale_current(char range); // 'L','M','S'
float mp_unit_scale_voltage(char range); // 'L','M','S'

// 補正適用（測定値→補正後値）
// NOTE: Calibration is disabled until cal_store is integrated.
float mp_apply_current(float measured_A, char range);
float mp_apply_voltage(float measured_V, char range);

#ifdef __cplusplus
}
#endif
