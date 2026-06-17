#include "debug.h"
#include "usart.h"
#include <stdio.h>
#include <stdint.h>

int g_debug_enable = 1;

void Debug_Init(void)
{
}

int fputc(int ch, FILE *f)
{
  if (!g_debug_enable)
  {
    return ch;
  }

  uint8_t c = (uint8_t)ch;

  if (ch == '\n')
  {
    uint8_t cr = '\r';
    (void)USART1_Tx(&cr, 1U, 10U);
  }

  (void)USART1_Tx(&c, 1U, 10U);
  return ch;
}

