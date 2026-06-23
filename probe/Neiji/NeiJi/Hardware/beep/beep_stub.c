#include "beep.h"

uint8_t beep_event = BEEP_EVENT_NULL;

void Beep_On(void)
{
}

void Beep_Off(void)
{
}

void Beep_Ctr(uint8_t req_event)
{
    switch (req_event) {
    case BEEP_EVENT_RTH:
        beep_event = BEEP_EVENT_RTH;
        break;
    case BEEP_EVENT_LIMIT:
        beep_event = BEEP_EVENT_LIMIT;
        break;
    case BEEP_EVENT_CLR:
    case BEEP_EVENT_STOP_TEST:
        beep_event = BEEP_EVENT_NULL;
        break;
    default:
        break;
    }
}
