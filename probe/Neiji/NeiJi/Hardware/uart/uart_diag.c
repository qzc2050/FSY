#include "uart_diag.h"

#include "ota.h"
#include "usart.h"

#include <string.h>

void UartDiag_WriteRaw(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U)) {
        return;
    }

    /* OTA 期间禁止往 USART1 打日志，避免污染协议应答 */
    if (OTA_IsRealtimeMuted() != 0U) {
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
