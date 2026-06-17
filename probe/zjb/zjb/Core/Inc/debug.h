/**
  ******************************************************************************
  * @file    debug.h
  * @brief   Simple debug print interface over USART1.
  ******************************************************************************
  */

#ifndef __DEBUG_H
#define __DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

extern int g_debug_enable;

void Debug_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_H */

