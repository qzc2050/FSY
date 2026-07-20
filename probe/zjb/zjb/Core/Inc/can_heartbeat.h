#ifndef CAN_HEARTBEAT_H
#define CAN_HEARTBEAT_H

#include <stdint.h>

/** 与 NeiJi Hardware/can/can_heartbeat.h 保持一致 */
#define CAN_HB_STD_ID       0x7F0U
#define CAN_HB_DLC          4U
#define CAN_HB_MAGIC0       0x5AU
#define CAN_HB_MAGIC1       0x4AU
#define CAN_HB_MAGIC2       0x42U
#define CAN_HB_PERIOD_MS    2000U

#endif /* CAN_HEARTBEAT_H */
