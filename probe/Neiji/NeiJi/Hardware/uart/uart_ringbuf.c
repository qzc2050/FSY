#include "uart_ringbuf.h"

void UartRingBuf_Init(UartRingBuf *rb, uint8_t *storage, uint16_t size)
{
    rb->buf = storage;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
}

bool UartRingBuf_Push(UartRingBuf *rb, uint8_t byte)
{
    uint16_t next = (uint16_t)((rb->head + 1U) % rb->size);

    if (next == rb->tail) {
        return false;
    }

    rb->buf[rb->head] = byte;
    rb->head = next;
    return true;
}

bool UartRingBuf_Pop(UartRingBuf *rb, uint8_t *byte)
{
    if (rb->head == rb->tail) {
        return false;
    }

    *byte = rb->buf[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1U) % rb->size);
    return true;
}

uint16_t UartRingBuf_Read(UartRingBuf *rb, uint8_t *buf, uint16_t len)
{
    uint16_t n = 0U;

    while ((n < len) && (rb->head != rb->tail)) {
        buf[n++] = rb->buf[rb->tail];
        rb->tail = (uint16_t)((rb->tail + 1U) % rb->size);
    }

    return n;
}

uint16_t UartRingBuf_Count(const UartRingBuf *rb)
{
    if (rb->head >= rb->tail) {
        return (uint16_t)(rb->head - rb->tail);
    }
    return (uint16_t)(rb->size - rb->tail + rb->head);
}

bool UartRingBuf_PeekAt(const UartRingBuf *rb, uint16_t index, uint8_t *byte)
{
    uint16_t count = UartRingBuf_Count(rb);

    if (index >= count) {
        return false;
    }

    *byte = rb->buf[(uint16_t)((rb->tail + index) % rb->size)];
    return true;
}

void UartRingBuf_Discard(UartRingBuf *rb, uint16_t count)
{
    while (count > 0U) {
        uint8_t dummy;

        if (!UartRingBuf_Pop(rb, &dummy)) {
            break;
        }
        count--;
    }
}
