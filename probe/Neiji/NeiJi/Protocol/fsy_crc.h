#ifndef FSY_CRC_H
#define FSY_CRC_H

#include <stdint.h>

uint16_t Fsy_Crc16Modbus(const uint8_t *data, uint16_t len);
void Fsy_AppendCrc(uint8_t *frame, uint16_t len_no_crc);

#endif
