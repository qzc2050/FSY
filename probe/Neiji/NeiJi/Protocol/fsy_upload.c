#include "fsy_upload.h"
#include "fsy_dispatch.h"
#include "fsy_regmap.h"
#include "fsy_crc.h"
#include "fsy_frame.h"
#include <stddef.h>

#define FSY_UPLOAD_FRAME_LEN  (5U + FSY_RT_REG_DATA_BYTES + 2U)

static void store_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)(v >> 8);
}

void Fsy_Upload_Init(void)
{
    Fsy_Regmap_Init();
}

int Fsy_Upload_BuildFrame(uint8_t *frame, uint16_t frame_cap)
{
    int payload_len;

    if (frame == NULL) {
        return -1;
    }

    if (frame_cap < FSY_UPLOAD_FRAME_LEN) {
        return -1;
    }

    frame[0] = Fsy_Dispatch_GetDeviceAddr();
    frame[1] = FSY_FC_ACTIVE_UPLOAD;
    frame[2] = (uint8_t)FSY_RT_REG_DATA_BYTES;
    store_u16_le(&frame[3], FSY_RT_REG_START);

    payload_len = Fsy_Regmap_BuildRtPayload(&frame[5], (uint16_t)(frame_cap - 7U));
    if (payload_len != (int)FSY_RT_REG_DATA_BYTES) {
        return -1;
    }

    Fsy_AppendCrc(frame, (uint16_t)(5U + (uint16_t)payload_len));
    return (int)FSY_UPLOAD_FRAME_LEN;
}

int Fsy_Upload_Send(int (*write_fn)(const uint8_t *data, uint16_t len))
{
    uint8_t frame[FSY_UPLOAD_FRAME_LEN];
    int frame_len;

    if (write_fn == NULL) {
        return -1;
    }

    frame_len = Fsy_Upload_BuildFrame(frame, sizeof(frame));
    if (frame_len <= 0) {
        return -1;
    }

    return write_fn(frame, (uint16_t)frame_len);
}
