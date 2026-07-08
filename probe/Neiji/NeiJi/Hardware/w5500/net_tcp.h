#ifndef NET_TCP_H
#define NET_TCP_H

#include <stdbool.h>
#include <stdint.h>

#include "uart_ringbuf.h"

#define NET_TCP_SYN_HOLD_MS       5000U
#define NET_TCP_RX_CAP            512U
#define NET_TCP_SOCK_INIT_WAIT_MS 50U
#define NET_TCP_LISTEN_RETRY_MS   3000U

bool Net_Tcp_Init(void);
void Net_Tcp_DeInit(void);
void Net_Tcp_RebindOnIpChange(void);
void Net_Tcp_PeriodicMaintain(void);
bool Net_Tcp_IsConnected(void);
int Net_Tcp_Write(const uint8_t *data, uint16_t len);
void Net_Tcp_PollRx(UartRingBuf *rx_ring);

#endif
