#ifndef FSY_FRAME_H
#define FSY_FRAME_H

#include <stdint.h>
#include <stdbool.h>

#define FSY_FC_READ_HOLDING_REQ   0x03U
#define FSY_FC_WRITE_SINGLE_REQ   0x06U
#define FSY_FC_WRITE_MULTI_REQ    0x10U
#define FSY_FC_READ_SINGLE_REQ    0x05U
#define FSY_FC_READ_HOLDING_RESP  0x13U
#define FSY_FC_WRITE_SINGLE_RESP  0x16U
#define FSY_FC_WRITE_MULTI_RESP   0x20U
#define FSY_FC_READ_SINGLE_RESP   0x15U
#define FSY_FC_ACTIVE_UPLOAD      0x23U

#define FSY_FRAME_MAX_LEN         256U

bool Fsy_Frame_CrcOk(const uint8_t *frame, uint16_t len);
uint16_t Fsy_Frame_PredictLen(const uint8_t *frame, uint16_t avail);
bool Fsy_Frame_FormatOk(const uint8_t *frame, uint16_t len);

#endif
