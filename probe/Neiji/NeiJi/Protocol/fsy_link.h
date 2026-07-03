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

/** 0x23 主动上传：UART1 + TCP + CAN */
int Fsy_Link_WriteUpload(const uint8_t *data, uint16_t len);

/** 串口应答：UART1 + CAN */
int Fsy_Link_WriteUart(const uint8_t *data, uint16_t len);

/** TCP 应答：TCP + CAN */
int Fsy_Link_WriteTcp(const uint8_t *data, uint16_t len);

#endif
