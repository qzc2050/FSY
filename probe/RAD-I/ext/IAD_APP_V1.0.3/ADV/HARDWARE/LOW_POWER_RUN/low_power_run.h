#ifndef __LOW_POWER_RUN_H
#define __LOW_POWER_RUN_H

#include "main.h"

#define __LPR_EXTERN extern 

__LPR_EXTERN uint32_t LPR_Time_Cnt;

void SystemClock_MSI_131(void);
void Sys_LPR_Setting(void);
void ENTER_LOW_POWER_RUN_MODE(void);
void EXIT_LOW_POWER_RUN_MODE(void);
void LPR_LED_Twinkle(void);
void LPR_USART_INIT(uint32_t baud_rate);
void LPR_Critical_Execute(void (*execute_fun)(),void *p);
#endif

