#include "measurement_pipeline.h"
#include <string.h>

static uint8_t s_pair = 0;

void mp_set_pair(uint8_t pair) { s_pair = pair; }

void mp_init_cache(void) {
    // Calibration disabled - cal_store not integrated
    // When cal_store is ready, load calibration data here
}

float mp_unit_scale_current(char range) {
    switch(range){ case 'L': return 0.01f; case 'M': return 0.001f; case 'S': return 0.00001f; }
    return 1.0f;
}
float mp_unit_scale_voltage(char range) {
    switch(range){ case 'L': return 1.0f; case 'M': return 0.01f; case 'S': return 0.01f; }
    return 1.0f;
}

// NOTE: Calibration is disabled until cal_store is integrated.
// These functions currently return the measured value unchanged.

float mp_apply_current(float measured_A, char range) {
    (void)range;
    return measured_A;
}

float mp_apply_voltage(float measured_V, char range) {
    (void)range;
    return measured_V;
}
