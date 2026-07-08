#include "beep.h"
#include "geiger.h"
#include "uart_diag.h"
#include "main.h"
#include "tim.h"

#include <stdio.h>

#ifndef BEEP_PWM_Pin
#define BEEP_PWM_Pin        GPIO_PIN_9
#endif
#ifndef BEEP_PWM_GPIO_Port
#define BEEP_PWM_GPIO_Port  GPIOH
#endif

#define BEEP_PWM_PERIOD     1000U

uint8_t beep_event = BEEP_EVENT_NULL;

static void beep_pwm_ensure(void)
{
    (void)HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_2);
}

void Beep_SetVolumePercent(float volume)
{
    uint32_t compare = 0U;

    beep_pwm_ensure();
    if (volume > 0.0f) {
        if (volume > 100.0f) {
            volume = 100.0f;
        }
        /* 无源蜂鸣器：占空比>50% 接近直流反而变小，UI 0~100% 映射到 0~50% */
        compare = (uint32_t)(volume * (float)BEEP_PWM_PERIOD / 200.0f);
    }
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, compare);
}

static void beep_log_pwm(const char *tag)
{
    char msg[96];

    (void)snprintf(msg, sizeof(msg),
                   "[BEEP] %s PH9 TIM12 CH2 CCR=%lu\r\n",
                   tag,
                   (unsigned long)__HAL_TIM_GET_COMPARE(&htim12, TIM_CHANNEL_2));
    UartDiag_Write(msg);
}

void Beep_PinEnsure(void)
{
    beep_pwm_ensure();
    Beep_SetVolumePercent(0.0f);
}

void Beep_On(void)
{
    beep_pwm_ensure();
    Beep_SetVolumePercent((float)sys_cfg.alarm_volume);
}

void Beep_Off(void)
{
#if (NEIJI_BEEP_GPIO_HIGH_TEST != 0U)
    return;
#endif
    beep_pwm_ensure();
    Beep_SetVolumePercent(0.0f);
}

static bool Beep_Alternate(uint16_t time, uint8_t cnt, bool ref)
{
    static uint32_t beep_tk;
    static bool beep_sta = BEEP_STA_ON;
    static uint8_t beep_times;

    if (ref) {
        beep_times = cnt;
        beep_tk = HAL_GetTick() - time;
        beep_sta = BEEP_STA_ON;
    }

    if ((HAL_GetTick() - beep_tk) >= time) {
        beep_tk = HAL_GetTick();

        if (beep_times == 0U) {
            beep_event = BEEP_EVENT_NULL;
            return beep_sta;
        }

        if (beep_sta) {
            Beep_On();
        } else {
            Beep_Off();
            if (beep_times != 0xFFU) {
                beep_times--;
            }
        }
        beep_sta = !beep_sta;
    }

    return beep_sta;
}

void Beep_Ctr(uint8_t req_event)
{
#if (NEIJI_BEEP_GPIO_HIGH_TEST != 0U)
    (void)req_event;
    return;
#endif
    bool ref = false;

    if (req_event > beep_event) {
        ref = true;
        beep_event = req_event;
    }

    switch (beep_event) {
    case BEEP_EVENT_NULL:
        Beep_Off();
        return;
    case BEEP_EVENT_RTH:
        (void)Beep_Alternate(300U, 0xFFU, ref);
        break;
    case BEEP_EVENT_LIMIT:
        Beep_On();
        break;
    case BEEP_EVENT_CLR:
        Beep_Off();
        beep_event = BEEP_EVENT_NULL;
        return;
    case BEEP_EVENT_SETTING:
        (void)Beep_Alternate(120U, 2U, ref);
        break;
    case BEEP_EVENT_TEST:
        (void)Beep_Alternate(200U, 0xFFU, ref);
        break;
    case BEEP_EVENT_STOP_TEST:
        Beep_Off();
        beep_event = BEEP_EVENT_NULL;
        return;
    default:
        return;
    }
}

void Beep_GpioHighTestHold(void)
{
    Beep_PinEnsure();
    Beep_SetVolumePercent(100.0f);
    beep_log_pwm("pwm-hi-hold");
    UartDiag_Write("[BEEP] PH9 PWM 100% hold (Q3 ON, need VCC_5V)\r\n");
}

void Beep_DebugProbe(void)
{
    char msg[96];

    Beep_PinEnsure();
    beep_log_pwm("init");

    (void)snprintf(msg, sizeof(msg),
                   "[BEEP] cfg sound=%u vol=%u (TIM12 CH2 PWM ~4kHz passive)\r\n",
                   (unsigned)sys_cfg.alarm_sound,
                   (unsigned)sys_cfg.alarm_volume);
    UartDiag_Write(msg);

    UartDiag_Write("[BEEP] PWM vol 2s (Q3 ON, need VCC_5V) ...\r\n");
    Beep_On();
    HAL_Delay(2000U);
    Beep_Off();
    beep_log_pwm("done");
}
