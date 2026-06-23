#include "fsy_frame.h"
#include "fsy_crc.h"
#include <stddef.h>

bool Fsy_Frame_CrcOk(const uint8_t *frame, uint16_t len)
{
    uint16_t crc;

    if ((frame == NULL) || (len < 4U)) {
        return false;
    }

    crc = Fsy_Crc16Modbus(frame, (uint16_t)(len - 2U));
    return (frame[len - 2U] == (uint8_t)(crc & 0xFFU)) &&
           (frame[len - 1U] == (uint8_t)(crc >> 8));
}

uint16_t Fsy_Frame_PredictLen(const uint8_t *frame, uint16_t avail)
{
    uint16_t len = 0U;
    uint8_t fc;

    if ((frame == NULL) || (avail < 4U)) {
        return 0U;
    }

    fc = frame[1];
    if ((fc & 0x80U) != 0U) {
        return 8U;
    }

    switch (fc) {
    case FSY_FC_READ_HOLDING_REQ:
    case FSY_FC_WRITE_SINGLE_REQ:
    case FSY_FC_WRITE_SINGLE_RESP:
    case FSY_FC_WRITE_MULTI_RESP:
    case FSY_FC_READ_SINGLE_REQ:
    case FSY_FC_READ_SINGLE_RESP:
        len = 8U;
        break;

    case FSY_FC_WRITE_MULTI_REQ:
        if (avail >= 7U) {
            len = (uint16_t)(9U + frame[6]);
        }
        break;

    case FSY_FC_READ_HOLDING_RESP:
    case FSY_FC_ACTIVE_UPLOAD:
        if (avail >= 3U) {
            len = (uint16_t)(7U + frame[2]);
        }
        break;

    default:
        break;
    }

    if ((len >= 4U) && (len <= avail)) {
        return len;
    }

    return 0U;
}

bool Fsy_Frame_FormatOk(const uint8_t *frame, uint16_t len)
{
    if ((frame == NULL) || (len < 4U)) {
        return false;
    }

    if ((frame[1] & 0x80U) != 0U) {
        return (len == 8U);
    }

    switch (frame[1]) {
    case FSY_FC_READ_HOLDING_REQ:
    case FSY_FC_WRITE_SINGLE_REQ:
    case FSY_FC_WRITE_SINGLE_RESP:
    case FSY_FC_WRITE_MULTI_RESP:
    case FSY_FC_READ_SINGLE_REQ:
    case FSY_FC_READ_SINGLE_RESP:
        return (len == 8U);

    case FSY_FC_WRITE_MULTI_REQ:
        if (len < 9U) {
            return false;
        }
        return (len == (uint16_t)(9U + frame[6]));

    case FSY_FC_READ_HOLDING_RESP:
    case FSY_FC_ACTIVE_UPLOAD:
        if (len < 7U) {
            return false;
        }
        return (len == (uint16_t)(7U + frame[2]));

    default:
        return false;
    }
}
