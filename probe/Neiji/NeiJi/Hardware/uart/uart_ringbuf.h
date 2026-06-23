#ifndef UART_RINGBUF_H
#define UART_RINGBUF_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t *buf;
    uint16_t size;
    volatile uint16_t head;
    volatile uint16_t tail;
} UartRingBuf;

void UartRingBuf_Init(UartRingBuf *rb, uint8_t *storage, uint16_t size);
bool UartRingBuf_Push(UartRingBuf *rb, uint8_t byte);
bool UartRingBuf_Pop(UartRingBuf *rb, uint8_t *byte);
uint16_t UartRingBuf_Read(UartRingBuf *rb, uint8_t *buf, uint16_t len);
uint16_t UartRingBuf_Count(const UartRingBuf *rb);
bool UartRingBuf_PeekAt(const UartRingBuf *rb, uint16_t index, uint8_t *byte);
void UartRingBuf_Discard(UartRingBuf *rb, uint16_t count);

#endif
