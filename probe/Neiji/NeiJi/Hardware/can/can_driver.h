#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Neiji FDCAN2 驱动（PB12/PB13 + TJA1051）。
 *
 * 与 zjb 经典 CAN 对齐：500 kbps、标准帧、最多 8 字节/帧；
 * RTU 整帧按 buf[0] 作为 StdId 分片发送（同 zjb protocol.c）。
 */

/** 0=停用 CAN 业务（不初始化、不镜像、不收发）；1=启用 */
#ifndef CAN_DRIVER_ENABLE
#define CAN_DRIVER_ENABLE  0
#endif

#define CAN_RX_QUEUE_SIZE  64U

typedef struct {
    uint16_t std_id;
    uint8_t  dlc;
    uint8_t  data[8];
    uint32_t tick_ms;
} CanRxItem;

bool CanDriver_Init(void);
bool CanDriver_IsReady(void);

/** 发送单帧标准数据（dlc 1~8） */
bool CanDriver_TransmitStd(uint16_t std_id, const uint8_t *data, uint8_t dlc);

/** 发送完整 Modbus RTU 帧（StdId = frame[0]，8 字节切片，片间 1ms） */
bool CanDriver_TransmitRtu(const uint8_t *frame, uint16_t len);

/** 从 ISR 入队的 RX 队列取一帧；无数据返回 false */
bool CanDriver_RxPop(CanRxItem *out);

/** 周期调用：CAN 8 字节切片 → RTU 组帧 → Fsy_Dispatch → 应答走 UART+CAN */
void CanDriver_Poll(void);

#endif /* CAN_DRIVER_H */
