#include "dose_rate.h"
#include "geiger.h"
#include <stdlib.h>

EwmaGlobalConfig g_ewma_config = {0};
float g_sensitivity_cpm_per_usvh = DEFAULT_SENSITIVITY_CPM_PER_USVH;
GeigerEWMA gm_sensor = {0};
uint8_t g_dr_input_mode = DR_INPUT_MODE_REAL;
uint32_t g_dr_manual_cps = 0;

static void dose_rate_sync_sensitivity_from_syscfg(void)
{
    if (sys_cfg.sensitivity > 0.0f) {
        g_sensitivity_cpm_per_usvh = sys_cfg.sensitivity;
    }
}

void init_ewma_config(void)
{
    g_ewma_config.threshold_cps = 100;
    g_ewma_config.threshold_delta = 10;
    g_ewma_config.alpha_low = 0.03f;
    g_ewma_config.alpha_high = 0.35f;
    g_ewma_config.boost_duration = 20;
}

void ewma_apply_config(GeigerEWMA* ctx)
{
    ctx->threshold_cps = g_ewma_config.threshold_cps;
    ctx->threshold_delta = g_ewma_config.threshold_delta;
    ctx->alpha_low = g_ewma_config.alpha_low;
    ctx->alpha_high = g_ewma_config.alpha_high;
}

void ewma_init(GeigerEWMA* ctx)
{
    ctx->current_avg_cps = 0.0f;
    ctx->current_dose_rate = 0.0f;
    ctx->last_raw_cps = 0;
    ctx->boost_timer = 0;
    ctx->is_initialized = false;
    ewma_apply_config(ctx);
}

void ewma_update(GeigerEWMA* ctx, int new_cps)
{
    int diff = 0;

    if (!ctx->is_initialized) {
        ctx->current_avg_cps = (float)new_cps;
        ctx->last_raw_cps = new_cps;
        ctx->is_initialized = true;

        if (g_sensitivity_cpm_per_usvh > 0.0f) {
            ctx->current_dose_rate = (ctx->current_avg_cps * 60.0f) / g_sensitivity_cpm_per_usvh;
        } else {
            ctx->current_dose_rate = 0.0f;
        }
        return;
    }

    diff = abs(new_cps - ctx->last_raw_cps);
    if (diff > ctx->threshold_delta) {
        ctx->boost_timer = g_ewma_config.boost_duration;
    }

    float alpha;
    if (ctx->boost_timer > 0) {
        alpha = ctx->alpha_high;
        ctx->boost_timer--;
    } else {
        if (new_cps > ctx->threshold_cps) {
            alpha = ctx->alpha_high;
        } else {
            alpha = ctx->alpha_low;
        }
    }

    ctx->current_avg_cps = alpha * (float)new_cps + (1.0f - alpha) * ctx->current_avg_cps;
    ctx->last_raw_cps = new_cps;

    if (g_sensitivity_cpm_per_usvh > 0.0f) {
        ctx->current_dose_rate = (ctx->current_avg_cps * 60.0f) / g_sensitivity_cpm_per_usvh;
    } else {
        ctx->current_dose_rate = 0.0f;
    }
}

void print_status(int time_sec, int raw_cps, GeigerEWMA* ctx)
{
    const char* mode_str = (ctx->boost_timer > 0 || raw_cps > ctx->threshold_cps) ? "FAST" : "SLOW";
    printf("T=%03ds | Raw:%4d | Avg:%6.2f | Dose:%7.3f uSv/h | [%s] Boost:%d\r\n",
           time_sec, raw_cps, ctx->current_avg_cps, ctx->current_dose_rate, mode_str, ctx->boost_timer);
}

void DoseRate_Init(void)
{
    init_ewma_config();
    dose_rate_sync_sensitivity_from_syscfg();
    ewma_init(&gm_sensor);
}

void DoseRate_ResetFilter(void)
{
    ewma_init(&gm_sensor);
}

float DoseRate_UpdateFromCps(uint32_t cps)
{
    dose_rate_sync_sensitivity_from_syscfg();
    ewma_update(&gm_sensor, (int)cps);
    return gm_sensor.current_dose_rate;
}

float DoseRate_GetCurrent(void)
{
    return gm_sensor.current_dose_rate;
}

void DoseRate_PrintConfig(void)
{
    printf("EWMA CFG: sens=%.2f cpm/uSv/h, th=%d, delta=%d, a_low=%.3f, a_high=%.3f, boost=%ds, mode=%d, manual_cps=%lu\r\n",
           g_sensitivity_cpm_per_usvh,
           g_ewma_config.threshold_cps,
           g_ewma_config.threshold_delta,
           g_ewma_config.alpha_low,
           g_ewma_config.alpha_high,
           g_ewma_config.boost_duration,
           (int)g_dr_input_mode,
           (unsigned long)g_dr_manual_cps);
}

bool DoseRate_SetSensitivity(float sensitivity_cpm_per_usvh)
{
    if (sensitivity_cpm_per_usvh <= 0.0f) {
        return false;
    }
    g_sensitivity_cpm_per_usvh = sensitivity_cpm_per_usvh;
    return true;
}

bool DoseRate_SetThresholdCps(int threshold_cps)
{
    if (threshold_cps < 0) {
        return false;
    }
    g_ewma_config.threshold_cps = threshold_cps;
    ewma_apply_config(&gm_sensor);
    return true;
}

bool DoseRate_SetThresholdDelta(int threshold_delta)
{
    if (threshold_delta < 0) {
        return false;
    }
    g_ewma_config.threshold_delta = threshold_delta;
    ewma_apply_config(&gm_sensor);
    return true;
}

bool DoseRate_SetAlphaLow(float alpha_low)
{
    if (alpha_low <= 0.0f || alpha_low >= 1.0f) {
        return false;
    }
    g_ewma_config.alpha_low = alpha_low;
    return true;
}

bool DoseRate_SetAlphaHigh(float alpha_high)
{
    if (alpha_high <= 0.0f || alpha_high >= 1.0f) {
        return false;
    }
    g_ewma_config.alpha_high = alpha_high;
    return true;
}

bool DoseRate_SetBoostDuration(int boost_duration_sec)
{
    if (boost_duration_sec < 0 || boost_duration_sec > 600) {
        return false;
    }
    g_ewma_config.boost_duration = boost_duration_sec;
    return true;
}

bool DoseRate_SetInputMode(uint8_t mode)
{
    if (mode > DR_INPUT_MODE_MANUAL) {
        return false;
    }
    g_dr_input_mode = mode;
    return true;
}

uint8_t DoseRate_GetInputMode(void)
{
    return g_dr_input_mode;
}

bool DoseRate_SetManualCps(uint32_t cps)
{
    g_dr_manual_cps = cps;
    return true;
}

uint32_t DoseRate_GetManualCps(void)
{
    return g_dr_manual_cps;
}

