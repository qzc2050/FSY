#include "fsy_crc.h"

uint16_t Fsy_Crc16Modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;

    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8U; j++) {
            if ((crc & 0x0001U) != 0U) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001U);
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

void Fsy_AppendCrc(uint8_t *frame, uint16_t len_no_crc)
{
    uint16_t crc = Fsy_Crc16Modbus(frame, len_no_crc);

    frame[len_no_crc] = (uint8_t)(crc & 0xFFU);
    frame[len_no_crc + 1U] = (uint8_t)(crc >> 8);
}
