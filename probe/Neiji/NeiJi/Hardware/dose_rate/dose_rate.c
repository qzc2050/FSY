#include "dose_rate.h"
#include <stddef.h>

static EwmaGlobalConfig s_ewma_config;
static float s_sensitivity_cpm_per_usvh = GEIGER_DEFAULT_SENSITIVITY_CPM;
static GeigerEWMA s_sensor;
static uint8_t s_dr_input_mode = DR_INPUT_MODE_REAL;
static uint32_t s_dr_manual_cps = 0U;
static float s_rate_limit_usvh = 10000.0f;
static uint32_t s_background_cpm = GEIGER_DEFAULT_BACKGROUND_CPM;
static uint32_t s_dead_time_us_x100 = GEIGER_DEFAULT_DEAD_TIME_US_X100;

static float dose_rate_correct_dead_time(float raw_cps)
{
    float tau_s;
    float denom;
    float corrected;

    if ((s_dead_time_us_x100 == 0U) || (raw_cps <= 0.0f)) {
        return raw_cps;
    }

    tau_s = ((float)s_dead_time_us_x100 / 100.0f) / 1000000.0f;
    denom = 1.0f - (raw_cps * tau_s);
    if (denom <= 0.0f) {
        return raw_cps;
    }

    corrected = raw_cps / denom;
    if (corrected < 0.0f) {
        return 0.0f;
    }
    return corrected;
}

/* 本底扣除后保留小数 CPS；勿截成 uint，否则本底附近会整段变 0 */
static float dose_rate_effective_cps(uint32_t raw_cps)
{
    float bg_cps;
    float eff;

    eff = dose_rate_correct_dead_time((float)raw_cps);
    bg_cps = (float)s_background_cpm / 60.0f;
    eff -= bg_cps;

    if (eff <= 0.0f) {
        return 0.0f;
    }
    return eff;
}

static void init_ewma_config(void)
{
    s_ewma_config.threshold_cps = 200;
    s_ewma_config.threshold_delta = 100;
    s_ewma_config.alpha_low = 0.01f;
    s_ewma_config.alpha_high = 0.35f;
    s_ewma_config.boost_duration = 20;
}

static void ewma_apply_config(GeigerEWMA *ctx)
{
    ctx->threshold_cps = s_ewma_config.threshold_cps;
    ctx->threshold_delta = s_ewma_config.threshold_delta;
    ctx->alpha_low = s_ewma_config.alpha_low;
    ctx->alpha_high = s_ewma_config.alpha_high;
}

static void ewma_init(GeigerEWMA *ctx)
{
    ctx->current_avg_cps = 0.0f;
    ctx->current_dose_rate = 0.0f;
    ctx->last_raw_cps = 0.0f;
    ctx->boost_timer = 0;
    ctx->is_initialized = false;
    ewma_apply_config(ctx);
}

static void ewma_update(GeigerEWMA *ctx, float new_cps)
{
    float diff;
    float alpha;

    if (!ctx->is_initialized) {
        ctx->current_avg_cps = new_cps;
        ctx->last_raw_cps = new_cps;
        ctx->is_initialized = true;

        if (s_sensitivity_cpm_per_usvh > 0.0f) {
            ctx->current_dose_rate =
                (ctx->current_avg_cps * 60.0f) / s_sensitivity_cpm_per_usvh;
        } else {
            ctx->current_dose_rate = 0.0f;
        }
        return;
    }

    diff = (new_cps > ctx->last_raw_cps) ?
           (new_cps - ctx->last_raw_cps) : (ctx->last_raw_cps - new_cps);
    if (diff > (float)ctx->threshold_delta) {
        ctx->boost_timer = s_ewma_config.boost_duration;
    }

    if (ctx->boost_timer > 0) {
        alpha = ctx->alpha_high;
        ctx->boost_timer--;
    } else if (new_cps > (float)ctx->threshold_cps) {
        alpha = ctx->alpha_high;
    } else {
        alpha = ctx->alpha_low;
    }

    ctx->current_avg_cps =
        alpha * new_cps + (1.0f - alpha) * ctx->current_avg_cps;
    ctx->last_raw_cps = new_cps;

    if (s_sensitivity_cpm_per_usvh > 0.0f) {
        ctx->current_dose_rate =
            (ctx->current_avg_cps * 60.0f) / s_sensitivity_cpm_per_usvh;
    } else {
        ctx->current_dose_rate = 0.0f;
    }
}

void DoseRate_Init(void)
{
    init_ewma_config();
    ewma_init(&s_sensor);
}

void DoseRate_ResetFilter(void)
{
    ewma_init(&s_sensor);
}

float DoseRate_UpdateFromCps(uint32_t cps)
{
    float eff = dose_rate_effective_cps(cps);

    ewma_update(&s_sensor, eff);
    return s_sensor.current_dose_rate;
}

float DoseRate_GetCurrent(void)
{
    return s_sensor.current_dose_rate;
}

uint32_t DoseRate_GetLastRawCps(void)
{
    if (s_sensor.last_raw_cps <= 0.0f) {
        return 0U;
    }
    return (uint32_t)(s_sensor.last_raw_cps + 0.5f);
}

float DoseRate_GetAvgCps(void)
{
    return s_sensor.current_avg_cps;
}

bool DoseRate_SetSensitivity(float sensitivity_cpm_per_usvh)
{
    if (sensitivity_cpm_per_usvh <= 0.0f) {
        return false;
    }

    s_sensitivity_cpm_per_usvh = sensitivity_cpm_per_usvh;
    return true;
}

bool DoseRate_SetEwmaConfig(const EwmaGlobalConfig *cfg)
{
    if (cfg == NULL) {
        return false;
    }
    if ((cfg->threshold_cps < 0) || (cfg->threshold_delta < 0) ||
        (cfg->boost_duration < 0) ||
        (cfg->alpha_low <= 0.0f) || (cfg->alpha_low > 1.0f) ||
        (cfg->alpha_high <= 0.0f) || (cfg->alpha_high > 1.0f)) {
        return false;
    }

    s_ewma_config = *cfg;
    ewma_apply_config(&s_sensor);
    return true;
}

void DoseRate_GetEwmaConfig(EwmaGlobalConfig *cfg)
{
    if (cfg != NULL) {
        *cfg = s_ewma_config;
    }
}

bool DoseRate_SetRateLimitUsvh(float limit_usvh)
{
    if (limit_usvh <= 0.0f) {
        return false;
    }
    s_rate_limit_usvh = limit_usvh;
    return true;
}

float DoseRate_GetRateLimitUsvh(void)
{
    return s_rate_limit_usvh;
}

bool DoseRate_SetBackgroundCpm(uint32_t background_cpm)
{
    s_background_cpm = background_cpm;
    return true;
}

uint32_t DoseRate_GetBackgroundCpm(void)
{
    return s_background_cpm;
}

bool DoseRate_SetDeadTimeUsX100(uint32_t dead_time_us_x100)
{
    s_dead_time_us_x100 = dead_time_us_x100;
    return true;
}

uint32_t DoseRate_GetDeadTimeUsX100(void)
{
    return s_dead_time_us_x100;
}

uint8_t DoseRate_GetInputMode(void)
{
    return s_dr_input_mode;
}

bool DoseRate_SetInputMode(uint8_t mode)
{
    if (mode > DR_INPUT_MODE_MANUAL) {
        return false;
    }
    s_dr_input_mode = mode;
    return true;
}

uint32_t DoseRate_GetManualCps(void)
{
    return s_dr_manual_cps;
}

bool DoseRate_SetManualCps(uint32_t cps)
{
    s_dr_manual_cps = cps;
    return true;
}
