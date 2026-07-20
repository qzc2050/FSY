#ifndef CAN_HEARTBEAT_H
#define CAN_HEARTBEAT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * ZJB → NeiJi CAN 链路心跳（与业务 StdId=RTU 地址 分开）
 *
 * StdId = 0x7F0
 * Data  = 5A 4A 42 seq   ('ZJB' 魔数 + 序号)
 * 周期  = 2s；NeiJi 超过 6s 未收到则认为未接到 ZJB
 */
#define CAN_HB_STD_ID       0x7F0U
#define CAN_HB_DLC          4U
#define CAN_HB_MAGIC0       0x5AU
#define CAN_HB_MAGIC1       0x4AU /* 'J' */
#define CAN_HB_MAGIC2       0x42U /* 'B' */
#define CAN_HB_PERIOD_MS    2000U
#define CAN_HB_TIMEOUT_MS   6000U

/** NeiJi：收到一帧合法心跳时调用 */
void CanHb_OnFrame(uint16_t std_id, const uint8_t *data, uint8_t dlc);

/** NeiJi：近期收到 ZJB 心跳则为 true */
bool CanHb_IsZjbLinked(void);

#endif /* CAN_HEARTBEAT_H */
