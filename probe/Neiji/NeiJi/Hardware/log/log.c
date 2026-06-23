#include "log.h"
#include "uart1_port.h"
#include <stddef.h>
#include <string.h>

void Log_Init(void)
{
}

void Log_Write(const char *message)
{
    if (message == NULL)
    {
        return;
    }

    (void)Uart1_Port_Write((const uint8_t *)message, (uint16_t)strlen(message));
}

void Log_Info(const char *message)
{
    Log_Write("[NeiJi] ");
    Log_Write(message);
    Log_Write("\r\n");
}
