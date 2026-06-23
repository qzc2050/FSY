#include "uart1_port.h"



#include <stdio.h>



/*

 * printf / fprintf -> USART1 115200

 * 走 Uart1_Port_*，与协议发送共用 TX 互斥锁。

 */



#if defined(__ARMCC_VERSION) || defined(__CC_ARM)

#pragma import(__use_no_semihosting)

#endif



#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)

__asm(".global __use_no_semihosting\n\t");

#endif



struct __FILE {

    int handle;

};



FILE __stdout;



void _sys_exit(int x)

{

    (void)x;

    while (1) {

    }

}



int _ttywrch(int ch)

{

    (void)Uart1_Port_WriteByte((uint8_t)ch);

    return ch;

}



int fputc(int ch, FILE *f)

{

    (void)f;

    (void)Uart1_Port_WriteByte((uint8_t)ch);

    return ch;

}



int __io_putchar(int ch)

{

    (void)Uart1_Port_WriteByte((uint8_t)ch);

    return ch;

}



int _write(int file, char *ptr, int len)

{

    (void)file;



    if ((ptr == NULL) || (len <= 0)) {

        return 0;

    }



    if (Uart1_Port_Write((const uint8_t *)ptr, (uint16_t)len) != len) {

        return -1;

    }



    return len;

}


