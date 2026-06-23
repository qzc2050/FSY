#ifndef UART_DIAG_H
#define UART_DIAG_H

#include <stdint.h>

void UartDiag_Write(const char *text);
void UartDiag_WriteRaw(const uint8_t *data, uint16_t len);

#endif
