#ifndef OTA_H
#define OTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * NeiJi App 侧 OTA 收包（与 ZJB / 上位机 / NeijiBoot 一致）
 *
 *  0x10 写: 200=size → 208=data(≤128B) → 202=crc32
 *  0x03 读: 204 → state(u32) + written_bytes(u32)
 *
 * 成功后写 Set 区 OtaFlag(PENDING) 并复位，由 Boot 搬运 Download→App。
 */

#define OTA_CHUNK_MAX_BYTES  128U

typedef enum
{
    OTA_STATE_IDLE    = 0,
    OTA_STATE_STARTED = 1,
    OTA_STATE_VERIFY  = 2,
    OTA_STATE_ERROR   = 3,
    OTA_STATE_DONE    = 4
} OtaState_e;

void       OTA_Init(void);
OtaState_e OTA_GetState(void);
uint32_t   OTA_GetWrittenBytes(void);
uint32_t   OTA_GetTotalSize(void);
uint8_t    OTA_IsRealtimeMuted(void);
void       OTA_Service(void);

int  OTA_StartSession(uint32_t total_size);
int  OTA_WriteChunk(const uint8_t *data, uint16_t len);
/** 将 WriteChunk 入队数据写入 Download；链路层应在发出 0x20 后再调用 */
int  OTA_CommitPending(void);
int  OTA_Finish(uint32_t expected_crc32);
void OTA_Abort(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_H */
