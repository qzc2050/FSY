#ifndef __DOSE_RATE_H
#define __DOSE_RATE_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define DEFAULT_SENSITIVITY_CPM_PER_USVH 40.0f

typedef enum {
    DR_INPUT_MODE_REAL = 0,   // 使用真实盖革计数
    DR_INPUT_MODE_SIM = 1,    // 使用内置模拟序列
    DR_INPUT_MODE_MANUAL = 2, // 使用串口设置的固定CPS
} DoseRateInputMode;

typedef struct {
    int threshold_cps;      // 绝对值阈值 (CPS)，高于此值进入快速响应
    int threshold_delta;    // 突变阈值 (CPS)，变化超过此值触发Boost
    float alpha_low;        // 低通滤波系数 (慢响应)
    float alpha_high;       // 高通滤波系数 (快响应)
    int boost_duration;     // Boost模式持续时间 (秒)
} EwmaGlobalConfig;

typedef struct {
    float current_avg_cps;     // 当前平均CPS
    float current_dose_rate;   // 当前剂量率
    int last_raw_cps;          // 上一次原始CPS
    int boost_timer;           // Boost模式计时器
    int threshold_cps;
    int threshold_delta;
    float alpha_high;
    float alpha_low;
    bool is_initialized;       // 初始化标志
} GeigerEWMA;

extern EwmaGlobalConfig G_EWMA_CONFIG;
extern float G_SENSITIVITY_CPM_PER_USVH;
extern GeigerEWMA gm_sensor;
extern uint8_t G_DR_INPUT_MODE;
extern uint32_t G_DR_MANUAL_CPS;

void init_ewma_config(void);
void ewma_apply_config(GeigerEWMA* ctx);
void ewma_init(GeigerEWMA* ctx);
void ewma_update(GeigerEWMA* ctx, int new_cps);
void print_status(int time_sec, int raw_cps, GeigerEWMA* ctx);

void DoseRate_Init(void);
void DoseRate_ResetFilter(void);
float DoseRate_UpdateFromCps(uint32_t cps);
float DoseRate_GetCurrent(void);
void DoseRate_PrintConfig(void);

bool DoseRate_SetSensitivity(float sensitivity_cpm_per_usvh);
bool DoseRate_SetThresholdCps(int threshold_cps);
bool DoseRate_SetThresholdDelta(int threshold_delta);
bool DoseRate_SetAlphaLow(float alpha_low);
bool DoseRate_SetAlphaHigh(float alpha_high);
bool DoseRate_SetBoostDuration(int boost_duration_sec);

bool DoseRate_SetInputMode(uint8_t mode);
uint8_t DoseRate_GetInputMode(void);
bool DoseRate_SetManualCps(uint32_t cps);
uint32_t DoseRate_GetManualCps(void);

#endif
