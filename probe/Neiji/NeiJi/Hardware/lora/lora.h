#ifndef __LORA_H
#define __LORA_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* E32 与 ZJB 对齐：C0 00 00 3D 17 44（115200 + 19.2k + 433MHz） */
#define LORA_DEFAULT_ADDR_H     (0x00U)
#define LORA_DEFAULT_ADDR_L     (0x00U)
#define LORA_DEFAULT_SPED       (0x3DU)
#define LORA_DEFAULT_CHAN       (0x17U)
#define LORA_DEFAULT_OPTION     (0x44U)   /* V8.0 出厂默认 OPTION */
#define LORA_USART_BAUD_CFG     (9600U)   /* 配置模式固定 9600 */
#define LORA_USART_BAUD_RUN     (115200U)

/* 0=上电写目标参数；1=发 AT+DEFAULT 恢复出厂 2.4k（勿与 ZJB 联调同开） */
#ifndef LORA_BOOT_AT_DEFAULT
#define LORA_BOOT_AT_DEFAULT    (0)
#endif

#define LORA_CHAN_MIN           (0x00U)
#define LORA_CHAN_MAX           (0x1FU)
#define LORA_CFG_RSP_LEN        (6U)    /* 读配置应答：C0 + 5 字节参数 */
#define LORA_UART_FRAME_GAP_MS  (5U)    /* 字节间隔超过此值视为一帧结束 */
#define LORA_AUX_TIMEOUT_MS     (500U)
/* 先听后发：信道刚忙过则随机退避再试，降低多机同频道碰撞 */
#ifndef LORA_CSMA_QUIET_MS
#define LORA_CSMA_QUIET_MS      (50U)
#endif
#ifndef LORA_CSMA_BACKOFF_MIN_MS
#define LORA_CSMA_BACKOFF_MIN_MS (15U)
#endif
#ifndef LORA_CSMA_BACKOFF_SPAN_MS
#define LORA_CSMA_BACKOFF_SPAN_MS (65U) /* 实际退避 = MIN + [0..SPAN] */
#endif
#ifndef LORA_CSMA_RETRY_MAX
#define LORA_CSMA_RETRY_MAX     (4U)
#endif
#define LORA_CFG_RX_TIMEOUT_MS  (500U)
#define LORA_SYNC_MAX_RETRY     (3U)    /* 地址/信道与 Flash 不一致时最多写入重试次数 */
#define LORA_ChanToMHz(chan)    (410U + (uint32_t)(chan))
#define LORA_REC_LEN            (256U)

/* UART5 + E32 控制脚（对齐 FSY-I / E32-433T20S 接法） */
#define LORA_TX_Pin             GPIO_PIN_12
#define LORA_TX_GPIO_Port       GPIOC
#define LORA_RX_Pin             GPIO_PIN_2
#define LORA_RX_GPIO_Port       GPIOD
#define LORA_AUX_Pin            GPIO_PIN_3
#define LORA_AUX_GPIO_Port      GPIOD
#define LORA_M1_Pin             GPIO_PIN_4
#define LORA_M1_GPIO_Port       GPIOD
#define LORA_M0_Pin             GPIO_PIN_5
#define LORA_M0_GPIO_Port       GPIOD

/* UART5 中断 + 环形缓冲收数（1=启用，配置会话期间自动切回 RDR 紧轮询） */
#ifndef LORA_UART_RX_IRQ
#define LORA_UART_RX_IRQ        (1)
#endif
#ifndef LORA_UART_RX_RING_SIZE
#define LORA_UART_RX_RING_SIZE  (512U)
#endif
/* usart.c 旧版 UART5 调试收帧（\r\n 转发 USART1），默认关闭 */
#ifndef LORA_UART5_IRQ_LEGACY
#define LORA_UART5_IRQ_LEGACY   (0)
#endif

/* 1=输出 [LORA_DBG] 调试信息，联调稳定后改为 0 */
#ifndef LORA_DEBUG
#define LORA_DEBUG              (0)
#endif

typedef enum {
    LORA_AIR_RATE_300BPS  = 0,
    LORA_AIR_RATE_1200BPS = 1,
    LORA_AIR_RATE_2400BPS = 2,
    LORA_AIR_RATE_4800BPS = 3,
    LORA_AIR_RATE_9600BPS = 4,
    LORA_AIR_RATE_19K2BPS = 5,
} LORA_AirRate_t;

typedef enum {
    LORA_TX_POWER_20DBM = 0,
    LORA_TX_POWER_17DBM = 1,
    LORA_TX_POWER_14DBM = 2,
    LORA_TX_POWER_10DBM = 3,
} LORA_TxPower_t;

typedef struct {
    uint8_t addr_h;
    uint8_t addr_l;
    uint8_t sped;
    uint8_t chan;
    uint8_t option;
} LORA_Config_t;

typedef enum {
    LORA_CFG_READ,
    LORA_CFG_WRITE,
    LORA_CFG_WRITE_VOLATILE,
    LORA_CFG_DEFAULT,   /* 恢复 E32 出厂缺省参数 */
} LORA_CfgOp_t;

typedef enum {
    LORA_PARAM_ADDR,
    LORA_PARAM_CHANNEL,
    LORA_PARAM_AIR_RATE,
    LORA_PARAM_TX_POWER,
} LORA_Param_t;

/* 底层驱动（UART5） */
bool LORA_Init(void);
bool LORA_Transmit(uint8_t *pdata, uint16_t len);
/* expect_len>0 固定长度轮询；expect_len==0 非阻塞读 FIFO 已有数据 */
uint16_t LORA_Receive(uint8_t *pdata, uint16_t expect_len, uint32_t timeout_ms);

/* 模块处于配置模式会话中（M0/M1=1 且读写配置寄存器） */
bool LORA_IsCfgBusy(void);

/* 配置帧读写调度 */
bool LORA_Config(LORA_CfgOp_t op, LORA_Config_t *cfg);

/*
 * 单项参数读写调度：is_set=true 写入 *value，false 读出到 *value
 * LORA_PARAM_ADDR 的 value 为 16 位地址（高 8 位在 value>>8）
 */
bool LORA_Param(LORA_Param_t id, uint32_t *value, bool is_set);

/* 模块就绪（软件使能 + UART5 已初始化且处于模式 0） */
bool LORA_IsReady(void);

/** reg123 bit9：0=关停 LoRa 收发/上报，1=允许（无电源脚时仅软件开关） */
bool LORA_IsEnabled(void);
bool LORA_SetEnabled(bool enable);

/* 轮询 LoRa 透传 RTU 并应答（在 UartTask 中调用） */
void LORA_Poll(void);

/* 发送 AT+DEFAULT 恢复 E32 出厂参数（需已进入 Init 且 UART5 可用） */
bool LORA_AtDefault(void);

/* UART5 中断入口（由 stm32h7xx_it.c 调用） */
void LORA_UART_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __LORA_H */
