#include "beep.h"
#include "geiger.h"
#include "uart_diag.h"
#include "main.h"

#include <stdio.h>

#ifndef BEEP_PWM_Pin
#define BEEP_PWM_Pin        GPIO_PIN_9
#endif
#ifndef BEEP_PWM_GPIO_Port
#define BEEP_PWM_GPIO_Port  GPIOH
#endif

uint8_t beep_event = BEEP_EVENT_NULL;

static void beep_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOH_CLK_ENABLE();
    gpio.Pin = BEEP_PWM_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BEEP_PWM_GPIO_Port, &gpio);
}

static void beep_log_pin_hw(const char *tag)
{
    char msg[96];
    uint32_t moder;
    uint32_t odr;

    moder = (BEEP_PWM_GPIO_Port->MODER >> (9U * 2U)) & 0x3U;
    odr = (BEEP_PWM_GPIO_Port->ODR >> 9U) & 0x1U;
    (void)snprintf(msg, sizeof(msg),
                   "[BEEP] %s PH9 moder=%lu odr=%lu (1=out 0=%s)\r\n",
                   tag,
                   (unsigned long)moder,
                   (unsigned long)odr,
                   (odr != 0U) ? "HI" : "LO");
    UartDiag_Write(msg);
}

void Beep_PinEnsure(void)
{
    beep_gpio_init();
}

void Beep_On(void)
{
    beep_gpio_init();
    HAL_GPIO_WritePin(BEEP_PWM_GPIO_Port, BEEP_PWM_Pin, GPIO_PIN_SET);
}

void Beep_Off(void)
{
    beep_gpio_init();
    HAL_GPIO_WritePin(BEEP_PWM_GPIO_Port, BEEP_PWM_Pin, GPIO_PIN_RESET);
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

void Beep_DebugProbe(void)
{
    char msg[96];

    Beep_PinEnsure();
    beep_log_pin_hw("init");

    (void)snprintf(msg, sizeof(msg),
                   "[BEEP] cfg sound=%u vol=%u (GPIO drive, HYG-1203A)\r\n",
                   (unsigned)sys_cfg.alarm_sound,
                   (unsigned)sys_cfg.alarm_volume);
    UartDiag_Write(msg);

    UartDiag_Write("[BEEP] GPIO HIGH 2s (Q3 ON, need VCC_5V) ...\r\n");
    Beep_On();
    HAL_Delay(2000U);
    Beep_Off();
    beep_log_pin_hw("done");
}
