#ifndef __LORA_BOOT_H
#define __LORA_BOOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** 上电读 E32 配置(C1)并 printf 到 USART1，格式与 Neiji 一致 */
uint8_t Lora_BootPrintConfig(void);

#ifdef __cplusplus
}
#endif

#endif /* __LORA_BOOT_H */
