#include "fsy_upload.h"
#include "fsy_dispatch.h"
#include "fsy_regmap.h"
#include "fsy_crc.h"
#include "fsy_frame.h"
#include "device_config.h"
#include <stddef.h>
#include <string.h>

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

uint32_t Fsy_Upload_PhaseOffsetMs(void)
{
#if FSY_UPLOAD_PHASE_ENABLE
    uint8_t addr;
    uint32_t slots;
    uint32_t slot;

    if (FSY_UPLOAD_PHASE_SLOTS == 0U) {
        return 0U;
    }

    addr = Fsy_Dispatch_GetDeviceAddr();
    if (addr == 0U) {
        return 0U;
    }

    slots = FSY_UPLOAD_PHASE_SLOTS;
    slot = ((uint32_t)addr - 1U) % slots;
    return (slot * FSY_UPLOAD_PERIOD_MS) / slots;
#else
    return 0U;
#endif
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

int Fsy_Upload_BuildSerialFrame(uint8_t *frame, uint16_t frame_cap)
{
    const char *sn;
    size_t sn_len;

    if (frame == NULL) {
        return -1;
    }
    if (frame_cap < FSY_SN_UPLOAD_FRAME_LEN) {
        return -1;
    }

    sn = DeviceConfig_GetSn();
    if (sn == NULL) {
        sn = "";
    }

    frame[0] = Fsy_Dispatch_GetDeviceAddr();
    frame[1] = FSY_FC_ACTIVE_UPLOAD;
    frame[2] = (uint8_t)FSY_SN_UPLOAD_PAYLOAD_BYTES;
    store_u16_le(&frame[3], FSY_REG_SERIALNUM);
    memset(&frame[5], 0, FSY_SN_UPLOAD_PAYLOAD_BYTES);

    sn_len = strlen(sn);
    if (sn_len > FSY_SN_UPLOAD_PAYLOAD_BYTES) {
        sn_len = FSY_SN_UPLOAD_PAYLOAD_BYTES;
    }
    if (sn_len > 0U) {
        memcpy(&frame[5], sn, sn_len);
    }

    Fsy_AppendCrc(frame, (uint16_t)(5U + FSY_SN_UPLOAD_PAYLOAD_BYTES));
    return (int)FSY_SN_UPLOAD_FRAME_LEN;
}

int Fsy_Upload_SendSerial(int (*write_fn)(const uint8_t *data, uint16_t len))
{
    uint8_t frame[FSY_SN_UPLOAD_FRAME_LEN];
    int frame_len;

    if (write_fn == NULL) {
        return -1;
    }

    frame_len = Fsy_Upload_BuildSerialFrame(frame, sizeof(frame));
    if (frame_len <= 0) {
        return -1;
    }

    return write_fn(frame, (uint16_t)frame_len);
}
