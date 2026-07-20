#ifndef FSY_LINK_H
#define FSY_LINK_H

#include "uart_ringbuf.h"

#include <stdbool.h>
#include <stdint.h>

/** 20ms 轮询：ring -> 线性组包缓冲 -> 解析/应答（参考 zjb Protocol_OnUart1Bytes）。 */
void Fsy_Link_OnUartBytes(UartRingBuf *rx_ring,
                            int (*write_fn)(const uint8_t *data, uint16_t len));

/** 从 ring 解析并处理至多一帧（TCP 等路径）。 */
bool Fsy_Link_ProcessOneFrame(UartRingBuf *rx_ring,
                              int (*write_fn)(const uint8_t *data, uint16_t len));

/** 连续处理直到无完整帧可解析。 */
void Fsy_Link_ProcessRx(UartRingBuf *rx_ring,
                        int (*write_fn)(const uint8_t *data, uint16_t len));

/** 0x23 主动上报：调试串口镜像 + 按优先级 TCP>CAN>LoRa 选一路 */
int Fsy_Link_WriteUpload(const uint8_t *data, uint16_t len);

/** 周期检测上报主通道变化并打调试日志（拔插网线时可观察） */
void Fsy_Link_PollUploadRoute(void);

/** 串口应答：仅 UART1 */
int Fsy_Link_WriteUart(const uint8_t *data, uint16_t len);

/** TCP 应答：仅 TCP */
int Fsy_Link_WriteTcp(const uint8_t *data, uint16_t len);

/** CAN 应答：仅 CAN */
int Fsy_Link_WriteCan(const uint8_t *data, uint16_t len);

#endif
