#include "fsy_link.h"

#include "fsy_dispatch.h"
#include "fsy_frame.h"
#include "can_driver.h"
#include "lora.h"
#include "net_tcp.h"
#include "uart1_port.h"

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#define FSY_ASM_BUF_SIZE     FSY_FRAME_MAX_LEN
#define FSY_ASM_DRAIN_CHUNK  32U

static uint8_t s_uart_asm_buf[FSY_ASM_BUF_SIZE];
static uint16_t s_uart_asm_len;

static void fsy_drain_ring_to_asm(UartRingBuf *rx_ring)
{
    uint8_t tmp[FSY_ASM_DRAIN_CHUNK];
    uint16_t n;

    for (;;) {
        n = UartRingBuf_Count(rx_ring);
        if (n == 0U) {
            break;
        }
        if (n > (uint16_t)sizeof(tmp)) {
            n = (uint16_t)sizeof(tmp);
        }
        n = UartRingBuf_Read(rx_ring, tmp, n);
        if (n == 0U) {
            break;
        }
        if ((s_uart_asm_len + n) > (uint16_t)sizeof(s_uart_asm_buf)) {
            s_uart_asm_len = 0U;
        }
        memcpy(&s_uart_asm_buf[s_uart_asm_len], tmp, n);
        s_uart_asm_len = (uint16_t)(s_uart_asm_len + n);
    }
}

void Fsy_Link_OnUartBytes(UartRingBuf *rx_ring,
                          int (*write_fn)(const uint8_t *data, uint16_t len))
{
    uint8_t resp[FSY_FRAME_MAX_LEN];
    int resp_len;

    if ((rx_ring == NULL) || (write_fn == NULL)) {
        return;
    }

    fsy_drain_ring_to_asm(rx_ring);

    for (;;) {
        uint16_t frame_len = Fsy_Frame_RtuAssembleLen(s_uart_asm_buf, s_uart_asm_len);

        if (frame_len == FSY_FRAME_LEN_NEED_MORE) {
            return;
        }
        if (frame_len == 0U) {
            if (s_uart_asm_len < 1U) {
                return;
            }
            memmove(s_uart_asm_buf, &s_uart_asm_buf[1], s_uart_asm_len - 1U);
            s_uart_asm_len--;
            continue;
        }
        if (frame_len > (uint16_t)sizeof(s_uart_asm_buf)) {
            memmove(s_uart_asm_buf, &s_uart_asm_buf[1], s_uart_asm_len - 1U);
            s_uart_asm_len--;
            continue;
        }
        if (s_uart_asm_len < frame_len) {
            return;
        }

        if (!Fsy_Frame_CrcOk(s_uart_asm_buf, frame_len) ||
            !Fsy_Frame_FormatOk(s_uart_asm_buf, frame_len)) {
            memmove(s_uart_asm_buf, &s_uart_asm_buf[1], s_uart_asm_len - 1U);
            s_uart_asm_len--;
            continue;
        }

        resp_len = Fsy_Dispatch_Request(s_uart_asm_buf, frame_len, resp, sizeof(resp));

        if (s_uart_asm_len > frame_len) {
            memmove(s_uart_asm_buf, &s_uart_asm_buf[frame_len], s_uart_asm_len - frame_len);
            s_uart_asm_len = (uint16_t)(s_uart_asm_len - frame_len);
        } else {
            s_uart_asm_len = 0U;
            if (resp_len > 0) {
                (void)write_fn(resp, (uint16_t)resp_len);
            }
            return;
        }

        if (resp_len > 0) {
            (void)write_fn(resp, (uint16_t)resp_len);
        }
    }
}

static bool copy_frame_from_ring(UartRingBuf *rx_ring, uint16_t len, uint8_t *out)
{
    for (uint16_t i = 0; i < len; i++) {
        if (!UartRingBuf_PeekAt(rx_ring, i, &out[i])) {
            return false;
        }
    }
    return true;
}

bool Fsy_Link_ProcessOneFrame(UartRingBuf *rx_ring,
                              int (*write_fn)(const uint8_t *data, uint16_t len))
{
    uint8_t scratch[FSY_ASM_BUF_SIZE];
    uint8_t resp[FSY_FRAME_MAX_LEN];
    uint16_t avail;
    uint16_t frame_len;
    int resp_len;

    if ((rx_ring == NULL) || (write_fn == NULL)) {
        return false;
    }

    if (UartRingBuf_Count(rx_ring) < 4U) {
        return false;
    }

    avail = UartRingBuf_Count(rx_ring);
    if (avail > FSY_ASM_BUF_SIZE) {
        avail = FSY_ASM_BUF_SIZE;
    }

    if (!copy_frame_from_ring(rx_ring, avail, scratch)) {
        return false;
    }

    frame_len = Fsy_Frame_PredictLen(scratch, avail);
    if (frame_len == 0U) {
        UartRingBuf_Discard(rx_ring, 1U);
        return true;
    }

    if (frame_len > UartRingBuf_Count(rx_ring)) {
        return false;
    }

    if (!copy_frame_from_ring(rx_ring, frame_len, scratch)) {
        return false;
    }

    if (!Fsy_Frame_CrcOk(scratch, frame_len) || !Fsy_Frame_FormatOk(scratch, frame_len)) {
        UartRingBuf_Discard(rx_ring, 1U);
        return true;
    }

    resp_len = Fsy_Dispatch_Request(scratch, frame_len, resp, sizeof(resp));
    UartRingBuf_Discard(rx_ring, frame_len);

    if (resp_len > 0) {
        (void)write_fn(resp, (uint16_t)resp_len);
    }

    return true;
}

void Fsy_Link_ProcessRx(UartRingBuf *rx_ring,
                        int (*write_fn)(const uint8_t *data, uint16_t len))
{
    while (Fsy_Link_ProcessOneFrame(rx_ring, write_fn)) {
    }
}

static void fsy_link_mirror_can(const uint8_t *data, uint16_t len)
{
    if (CanDriver_IsReady()) {
        (void)CanDriver_TransmitRtu(data, len);
    }
}

static void fsy_link_mirror_lora(const uint8_t *data, uint16_t len)
{
    if (LORA_IsEnabled() && LORA_IsReady()) {
        (void)LORA_Transmit((uint8_t *)data, len);
    }
}

int Fsy_Link_WriteUpload(const uint8_t *data, uint16_t len)
{
    int uart_rc;

    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    uart_rc = Uart1_Port_Write(data, len);
    (void)Net_Tcp_Write(data, len);
    fsy_link_mirror_can(data, len);
    fsy_link_mirror_lora(data, len);

    return (uart_rc == (int)len) ? (int)len : -1;
}

int Fsy_Link_WriteUart(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    (void)Uart1_Port_Write(data, len);
    fsy_link_mirror_can(data, len);
    return (int)len;
}

int Fsy_Link_WriteTcp(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    (void)Net_Tcp_Write(data, len);
    fsy_link_mirror_can(data, len);
    return (int)len;
}
