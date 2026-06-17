#ifndef __LORA_H
#define __LORA_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* E32-433T20S 出厂默认：C0 00 00 1A 17 44 */
#define LORA_DEFAULT_ADDR_H     (0x00U)
#define LORA_DEFAULT_ADDR_L     (0x01U)
#define LORA_DEFAULT_SPED       (0x1AU)
#define LORA_DEFAULT_CHAN       (0x17U)
#define LORA_DEFAULT_OPTION     (0x44U)   /* V8.0 出厂默认 OPTION */

#define LORA_CHAN_MIN           (0x00U)
#define LORA_CHAN_MAX           (0x1FU)
#define LORA_CFG_RSP_LEN        (6U)    /* 读配置应答：C0 + 5 字节参数 */
#define LORA_UART_FRAME_GAP_MS  (5U)    /* 字节间隔超过此值视为一帧结束 */
#define LORA_AUX_TIMEOUT_MS     (500U)
#define LORA_CFG_RX_TIMEOUT_MS  (500U)
#define LORA_SYNC_MAX_RETRY     (3U)    /* 地址/信道与 Flash 不一致时最多写入重试次数 */
#define LORA_ChanToMHz(chan)    (410U + (uint32_t)(chan))

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
    LORA_CFG_DEFAULT,   /* 信道/速率/功率=E32 缺省；地址=sys_cfg.dev_addr */
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

/* 从模块读回配置并打印；读失败则不打印默认值 */
bool LORA_ConfigPrint(void);

/* 上电后：读 LoRa 配置，与 Flash dev_addr / LORA_DEFAULT_CHAN 对齐（各最多重试 3 次） */
bool LORA_SyncFromFlash(void);

/* UART5 中断入口（由 usart.c UART5_IRQHandler 调用） */
void LORA_UART_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __LORA_H */
