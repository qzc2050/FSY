#include "alarm_output.h"
#include "geiger.h"
#include "beep.h"
#include "ws2812b.h"
#include "dose_rate.h"
#include "fsy_regmap.h"
#include "sys_cfg_defaults.h"
#include "device_config.h"

#include "main.h"
#include "stm32h7xx_hal.h"
#include <string.h>

#define GEIGER_ZERO_FAULT_SEC   120U
#define FSY_ALARM_ENV_MASK      ((1UL << 6) | (1UL << 10) | (1UL << 14) | \
                                 (1UL << 18) | (1UL << 22))

static Alarm_Visual_State_t s_vis_state = ALARM_VIS_NORMAL;
static uint32_t s_geiger_zero_sec;
static uint32_t s_last_cps;

static uint8_t dose_thresholds_enabled(void)
{
    return (sys_cfg.th_rh_rate > 0.0f) || (sys_cfg.th_rl_rate > 0.0f);
}

static uint8_t probe_count_fault(void)
{
    if (DoseRate_GetInputMode() != DR_INPUT_MODE_REAL) {
        s_geiger_zero_sec = 0U;
        return 0U;
    }
    if (s_last_cps > 0U) {
        s_geiger_zero_sec = 0U;
        return 0U;
    }
    if (s_geiger_zero_sec >= GEIGER_ZERO_FAULT_SEC) {
        return 1U;
    }
    return 0U;
}

static Alarm_Visual_State_t alarm_eval_visual(void)
{
    uint32_t st = Fsy_Regmap_GetAlarmStatus();

    if (probe_count_fault()) {
        return ALARM_VIS_FAULT;
    }
    /* 剂量率报警（reg82 使能 + 超限）优先于环境离线；离线仅走 WS2812，不驱动转轮/蜂鸣 */
    if ((st & (1UL << RATE_HIGH_ALARM_BIT)) != 0U) {
        return ALARM_VIS_HI;
    }
    if ((st & (1UL << RATE_LOW_ALARM_BIT)) != 0U) {
        return ALARM_VIS_LO;
    }
    /* RK100N 无空气成分：忽略环境传感器离线故障态 */
    {
        const char *model = DeviceConfig_GetProductModel();
        if ((model == NULL) || (strncmp(model, "RK100N", 6) != 0)) {
            if ((st & FSY_ALARM_ENV_MASK) != 0U) {
                return ALARM_VIS_FAULT;
            }
        }
    }
    return ALARM_VIS_NORMAL;
}

static uint8_t beep_is_user_test(uint8_t ev)
{
    return (ev == BEEP_EVENT_SETTING) || (ev == BEEP_EVENT_TEST);
}

static void alarm_apply_beep(Alarm_Visual_State_t vis)
{
    uint8_t sound_on;

    sound_on = sys_cfg.alarm_sound && (sys_cfg.alarm_volume > 0U) && dose_thresholds_enabled();
    if (!sound_on) {
        if ((beep_event != BEEP_EVENT_NULL) && !beep_is_user_test(beep_event)) {
            Beep_Ctr(BEEP_EVENT_CLR);
        }
        return;
    }

    if (vis == ALARM_VIS_HI) {
        if (data_var.real_rate >= (float)RATE_LIMIT) {
            Beep_Ctr(BEEP_EVENT_LIMIT);
        } else {
            Beep_Ctr(BEEP_EVENT_RTH);
        }
    } else if (vis == ALARM_VIS_LO) {
        Beep_Ctr(BEEP_EVENT_RTH);
    } else if ((beep_event != BEEP_EVENT_NULL) && !beep_is_user_test(beep_event)) {
        Beep_Ctr(BEEP_EVENT_CLR);
    }
}

static void alarm_apply_ws2812(Alarm_Visual_State_t vis)
{
#if !NEIJI_WS2812_ENABLE
    (void)vis;
    return;
#else
    ws2812_pattern_t pat;

    /* 关光报警：不显示红/故障灯效，仍保持正常白灯慢流水待机 */
    if (!sys_cfg.alarm_light) {
        pat = WS2812_PATTERN_WHITE_SLOW;
    } else {
        switch (vis) {
        case ALARM_VIS_FAULT:
            pat = WS2812_PATTERN_RED_SOLID;
            break;
        case ALARM_VIS_HI:
            pat = WS2812_PATTERN_RED_FAST;
            break;
        case ALARM_VIS_LO:
            pat = WS2812_PATTERN_RED_SLOW;
            break;
        default:
            pat = WS2812_PATTERN_WHITE_SLOW;
            break;
        }
    }

    if (ws2812_get_pattern() != pat) {
        ws2812_set_pattern(pat);
    }
#endif
}

void Alarm_Output_Init(void)
{
    s_vis_state = ALARM_VIS_NORMAL;
    s_geiger_zero_sec = 0U;
    s_last_cps = 0U;
#if NEIJI_WS2812_ENABLE
    ws2812_set_pattern(WS2812_PATTERN_WHITE_SLOW);
#endif
}

void Alarm_Output_Update(void)
{
    static uint32_t sec_tk;

    if ((HAL_GetTick() - sec_tk) >= 1000U) {
        sec_tk = HAL_GetTick();
        if (s_last_cps == 0U) {
            s_geiger_zero_sec++;
        } else {
            s_geiger_zero_sec = 0U;
        }
    }

    s_vis_state = alarm_eval_visual();
    alarm_apply_beep(s_vis_state);
    alarm_apply_ws2812(s_vis_state);
}

Alarm_Visual_State_t Alarm_Output_GetVisualState(void)
{
    return s_vis_state;
}

uint8_t Alarm_Output_IsDoseHiActive(void)
{
    return ((Fsy_Regmap_GetAlarmStatus() & (1UL << RATE_HIGH_ALARM_BIT)) != 0U) ? 1U : 0U;
}

/** 由 geiger 任务每秒告知 CPS，用于本底计数故障判定 */
void Alarm_Output_NotifyCps(uint32_t cps)
{
    s_last_cps = cps;
}
