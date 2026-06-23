#ifndef UART1_PORT_H

#define UART1_PORT_H



#include "uart_ringbuf.h"

#include <stdint.h>



void Uart1_Port_Init(void);

void Uart1_Port_StartRx(void);

UartRingBuf *Uart1_Port_RxRing(void);

int Uart1_Port_Write(const uint8_t *data, uint16_t len);

int Uart1_Port_WriteByte(uint8_t byte);



#endif

