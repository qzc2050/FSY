#include "beep.h"
#include "tim.h"

uint8_t beep_event = BEEP_EVENT_NULL;

void Beep_On(void)
{
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, BEEP_DUTY);
}

void Beep_Off(void)
{
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_2, 0U);
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
    switch (req_event) {
    case BEEP_EVENT_RTH:
        beep_event = BEEP_EVENT_RTH;
        (void)Beep_Alternate(300U, 0xFFU, true);
        break;
    case BEEP_EVENT_LIMIT:
        beep_event = BEEP_EVENT_LIMIT;
        (void)Beep_Alternate(200U, 0xFFU, true);
        break;
    case BEEP_EVENT_CLR:
        Beep_Off();
        beep_event = BEEP_EVENT_NULL;
        break;
    case BEEP_EVENT_SETTING:
        (void)Beep_Alternate(120U, 2U, true);
        break;
    case BEEP_EVENT_TEST:
        (void)Beep_Alternate(200U, 0xFFU, true);
        break;
    case BEEP_EVENT_STOP_TEST:
        Beep_Off();
        beep_event = BEEP_EVENT_NULL;
        break;
    default:
        break;
    }
}
