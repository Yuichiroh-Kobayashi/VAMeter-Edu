#include "measurement_pipeline.h"
#include <string.h>

static uint8_t s_pair = 0;

static cal_block_t s_A_L, s_A_M, s_A_S;
static cal_block_t s_V_L, s_V_M, s_V_S;
static bool v_A_L=false, v_A_M=false, v_A_S=false;
static bool v_V_L=false, v_V_M=false, v_V_S=false;

void mp_set_pair(uint8_t pair) { s_pair = pair; }

void mp_init_cache(void) {
    // 電流計
    cal_store_load(CAL_TYPE_AMP,  s_pair, CAL_RANGE_L, &s_A_L, &v_A_L);
    cal_store_load(CAL_TYPE_AMP,  s_pair, CAL_RANGE_M, &s_A_M, &v_A_M);
    cal_store_load(CAL_TYPE_AMP,  s_pair, CAL_RANGE_S, &s_A_S, &v_A_S);
    // 電圧計
    cal_store_load(CAL_TYPE_VOLT, s_pair, CAL_RANGE_L, &s_V_L, &v_V_L);
    cal_store_load(CAL_TYPE_VOLT, s_pair, CAL_RANGE_M, &s_V_M, &v_V_M);
    cal_store_load(CAL_TYPE_VOLT, s_pair, CAL_RANGE_S, &s_V_S, &v_V_S);
}

float mp_unit_scale_current(char range) {
    switch(range){ case 'L': return 0.01f; case 'M': return 0.001f; case 'S': return 0.00001f; }
    return 1.0f;
}
float mp_unit_scale_voltage(char range) {
    switch(range){ case 'L': return 1.0f; case 'M': return 0.01f; case 'S': return 0.01f; }
    return 1.0f;
}

// 仕様書の apply_calibration と同じロジック（Cで実装）
static float apply_calibration(float measured, const cal_block_t* c, float unit_scale) {
    if (!c) return measured;
    // version 未設定などは上位で v_* が false になるが、念のため
    float FS = (float)c->fs_scaled * unit_scale;
    if (FS <= 0.f) return measured;

    float f = measured / FS;
    if (f <= 0.f) return measured; // 原点は補正0
    if (f >= 1.f) return measured - (c->error[9] * unit_scale);

    float pos = f * 10.f;           // 0..10
    int   k   = (int)pos;           // 0..9
    float t   = pos - k;            // 0..1
    float e0  = (k==0) ? 0.f : (c->error[k-1] * unit_scale);
    float e1  = c->error[k] * unit_scale;
    float err = e0 + (e1 - e0) * t;
    return measured - err;
}

float mp_apply_current(float measured_A, char range) {
    switch(range){
        case 'L': return v_A_L ? apply_calibration(measured_A, &s_A_L, mp_unit_scale_current('L')) : measured_A;
        case 'M': return v_A_M ? apply_calibration(measured_A, &s_A_M, mp_unit_scale_current('M')) : measured_A;
        case 'S': return v_A_S ? apply_calibration(measured_A, &s_A_S, mp_unit_scale_current('S')) : measured_A;
    }
    return measured_A;
}

float mp_apply_voltage(float measured_V, char range) {
    switch(range){
        case 'L': return v_V_L ? apply_calibration(measured_V, &s_V_L, mp_unit_scale_voltage('L')) : measured_V;
        case 'M': return v_V_M ? apply_calibration(measured_V, &s_V_M, mp_unit_scale_voltage('M')) : measured_V;
        case 'S': return v_V_S ? apply_calibration(measured_V, &s_V_S, mp_unit_scale_voltage('S')) : measured_V;
    }
    return measured_V;
}
