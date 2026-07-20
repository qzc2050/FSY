#include "can_heartbeat.h"

#include "main.h"

static volatile uint32_t s_last_hb_ms;
static volatile uint8_t s_have_hb;

void CanHb_OnFrame(uint16_t std_id, const uint8_t *data, uint8_t dlc)
{
    if (std_id != CAN_HB_STD_ID) {
        return;
    }
    if ((data == NULL) || (dlc < CAN_HB_DLC)) {
        return;
    }
    if ((data[0] != CAN_HB_MAGIC0) ||
        (data[1] != CAN_HB_MAGIC1) ||
        (data[2] != CAN_HB_MAGIC2)) {
        return;
    }

    s_last_hb_ms = HAL_GetTick();
    s_have_hb = 1U;
}

bool CanHb_IsZjbLinked(void)
{
    uint32_t now;

    if (s_have_hb == 0U) {
        return false;
    }
    now = HAL_GetTick();
    return ((now - s_last_hb_ms) <= CAN_HB_TIMEOUT_MS);
}
