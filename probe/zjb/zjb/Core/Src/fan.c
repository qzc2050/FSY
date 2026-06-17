#include "fan.h"

static volatile uint8_t s_fan_on = 0U;

void Fan_Set(uint8_t on)
{
  if (on != 0U)
  {
    s_fan_on = 1U;
  }
  else
  {
    s_fan_on = 0U;
  }
}

uint8_t Fan_Get(void)
{
  return s_fan_on;
}

