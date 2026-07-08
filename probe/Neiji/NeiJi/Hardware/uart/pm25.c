#include "pm25.h"
#include "usart.h"
#include "main.h"
#include "sensor_common.h"

#define PM25_HEADER1      0x42U
#define PM25_HEADER2      0x4DU
#define PM25_FRAME_LEN    32U

typedef enum {
    PM25_STATE_WAIT_HEADER1 = 0,
    PM25_STATE_WAIT_HEADER2,
    PM25_STATE_COLLECT_FRAME
} PM25_ParseState_t;

static PM25_ParseState_t s_state;
static uint8_t s_buf[PM25_FRAME_LEN];
static uint8_t s_index;
static PM25_Data_t s_data;
static uint8_t s_rx_byte;

static void PM25_ResetParser(void)
{
    s_state = PM25_STATE_WAIT_HEADER1;
    s_index = 0U;
}

static void PM25_OnFrame(const uint8_t *frame)
{
    uint16_t sum = 0U;
    uint16_t recv_sum;
    uint8_t i;

    for (i = 0U; i < PM25_FRAME_LEN - 2U; i++) {
        sum = (uint16_t)(sum + frame[i]);
    }

    recv_sum = (uint16_t)(((uint16_t)frame[PM25_FRAME_LEN - 2U] << 8) |
                          (uint16_t)frame[PM25_FRAME_LEN - 1U]);
    if (sum != recv_sum) {
        return;
    }

    s_data.pm1_0 = (uint16_t)(((uint16_t)frame[10] << 8) | (uint16_t)frame[11]);
    s_data.pm2_5 = (uint16_t)(((uint16_t)frame[12] << 8) | (uint16_t)frame[13]);
    s_data.pm10 = (uint16_t)(((uint16_t)frame[14] << 8) | (uint16_t)frame[15]);
    s_data.last_update_tick = HAL_GetTick();
    s_data.online = 1U;
}

void PM25_Init(void)
{
    s_data.pm1_0 = 0U;
    s_data.pm2_5 = 0U;
    s_data.pm10 = 0U;
    s_data.online = 0U;
    s_data.last_update_tick = 0U;
    PM25_ResetParser();
}

void PM25_ProcessByte(uint8_t byte)
{
    switch (s_state) {
    case PM25_STATE_WAIT_HEADER1:
        if (byte == PM25_HEADER1) {
            s_buf[0] = byte;
            s_index = 1U;
            s_state = PM25_STATE_WAIT_HEADER2;
        }
        break;

    case PM25_STATE_WAIT_HEADER2:
        if (byte == PM25_HEADER2) {
            s_buf[1] = byte;
            s_index = 2U;
            s_state = PM25_STATE_COLLECT_FRAME;
        } else {
            PM25_ResetParser();
        }
        break;

    case PM25_STATE_COLLECT_FRAME:
        s_buf[s_index++] = byte;
        if (s_index >= PM25_FRAME_LEN) {
            PM25_OnFrame(s_buf);
            PM25_ResetParser();
        }
        break;

    default:
        PM25_ResetParser();
        break;
    }
}

void PM25_GetData(PM25_Data_t *out)
{
    if (out == NULL) {
        return;
    }

    *out = s_data;
    if (sensor_tick_is_stale(s_data.last_update_tick, SENSOR_OFFLINE_MS)) {
        out->online = 0U;
    }
}

void PM25_Rx_Start(void)
{
    PM25_Init();
    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    (void)HAL_UART_Receive_IT(&huart3, &s_rx_byte, 1U);
}

void PM25_OnRxCplt(void)
{
    PM25_ProcessByte(s_rx_byte);
    (void)HAL_UART_Receive_IT(&huart3, &s_rx_byte, 1U);
}
