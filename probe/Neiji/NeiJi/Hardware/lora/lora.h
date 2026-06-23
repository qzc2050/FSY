#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include <stdbool.h>

#define LORA_REC_LEN  256U

#define LORA_PARAM_ADDR  0

bool LORA_Init(void);
bool LORA_Param(int param, uint32_t *val, bool write);
bool LORA_Transmit(uint8_t *data, uint16_t len);
uint16_t LORA_Receive(uint8_t *data, uint16_t timeout, uint16_t maxlen);
bool LORA_IsCfgBusy(void);

#endif
