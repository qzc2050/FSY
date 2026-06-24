#include "uart_diag.h"

#include "usart.h"

#include <string.h>

void UartDiag_WriteRaw(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U)) {
        return;
    }

    (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 100U);
}

void UartDiag_Write(const char *text)
{
    if (text == NULL) {
        return;
    }

    UartDiag_WriteRaw((const uint8_t *)text, (uint16_t)strlen(text));
}
