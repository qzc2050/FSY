#ifndef __CMD_H
#define __CMD_H

#include "stm32l0xx_hal.h"
#include "pcf8563.h"
#include "sys_ctr.h"

#define WKUP_KEY_STA  HAL_GPIO_ReadPin(KEY_INT_GPIO_Port,KEY_INT_Pin)


extern bool one_second_cnt_func;
extern bool dose_rate_print_func;
extern bool test_cmd;

//void STR_Cpy(char *dest, char *src);
void Usart_Cmd_Tip(void);
void Uasrt_Cmd_Rx(void);

void UART_Date_Printf(uint32_t date);
void UART_Dose_Printf(bool data_type);
void UART_PRINTF_HISTORY(void);
void UART_AVER_PRINTF(void);
void UART_CLEAR_HISTORY(void);

void JumpToIAP(void);
#endif

