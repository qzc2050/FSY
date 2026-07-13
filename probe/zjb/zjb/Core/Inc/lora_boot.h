#ifndef __LORA_BOOT_H
#define __LORA_BOOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f1xx_hal.h"

/* E32 目标参数：115200 8N1 + 空中 19.2k，SPED=(7<<3)|5=0x3D */
#define LORA_TARGET_ADDR_H    (0x00U)
#define LORA_TARGET_ADDR_L    (0x00U)
#define LORA_TARGET_SPED      (0x3DU)
#define LORA_TARGET_CHAN      (0x17U)   /* 433MHz */
#define LORA_TARGET_OPTION    (0x44U)
#define LORA_USART_BAUD_CFG   (9600U)   /* 配置模式固定 9600 */
#define LORA_USART_BAUD_RUN   (115200U)

/** 上电写/校验 E32 参数并切换 USART2 到 115200，返回 1=成功 */
uint8_t Lora_BootApplyConfig(void);

/**
 * 等待 E32 AUX 空闲（高电平）。
 * @return 1=就绪，0=超时（仍继续发，避免卡死）
 */
uint8_t Lora_WaitAuxReady(uint32_t timeout_ms);

/** 经 USART2 发往 LoRa：发前/发后均等 AUX，减轻本地模块忙时盲写 */
HAL_StatusTypeDef Lora_Usart2Tx(const uint8_t *buf, uint16_t len, uint32_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* __LORA_BOOT_H */
