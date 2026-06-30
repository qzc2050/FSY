#ifndef DOSE_RATE_H
#define DOSE_RATE_H

#include <stdbool.h>
#include <stdint.h>

#define GEIGER_DEFAULT_SENSITIVITY_CPM  120.0f

typedef enum {
    DR_INPUT_MODE_REAL = 0,
    DR_INPUT_MODE_SIM = 1,
    DR_INPUT_MODE_MANUAL = 2,
} DoseRateInputMode;

typedef struct {
    int threshold_cps;
    int threshold_delta;
    float alpha_low;
    float alpha_high;
    int boost_duration;
} EwmaGlobalConfig;

typedef struct {
    float current_avg_cps;
    float current_dose_rate;
    int last_raw_cps;
    int boost_timer;
    int threshold_cps;
    int threshold_delta;
    float alpha_high;
    float alpha_low;
    bool is_initialized;
} GeigerEWMA;

void DoseRate_Init(void);
void DoseRate_ResetFilter(void);
float DoseRate_UpdateFromCps(uint32_t cps);
float DoseRate_GetCurrent(void);

bool DoseRate_SetSensitivity(float sensitivity_cpm_per_usvh);

bool DoseRate_SetEwmaConfig(const EwmaGlobalConfig *cfg);
void DoseRate_GetEwmaConfig(EwmaGlobalConfig *cfg);

bool DoseRate_SetRateLimitUsvh(float limit_usvh);
float DoseRate_GetRateLimitUsvh(void);

uint8_t DoseRate_GetInputMode(void);
bool DoseRate_SetInputMode(uint8_t mode);
uint32_t DoseRate_GetManualCps(void);
bool DoseRate_SetManualCps(uint32_t cps);

#endif
