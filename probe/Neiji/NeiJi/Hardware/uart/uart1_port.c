#include "uart1_port.h"



#include "pm25.h"

#include "cmsis_os.h"

#include "usart.h"



#include <stddef.h>



#include <string.h>







#define UART1_RX_RING_SIZE 512U







static uint8_t s_rx_ring_storage[UART1_RX_RING_SIZE];



static UartRingBuf s_rx_ring;



static uint8_t s_rx_byte;



static osMutexId_t s_tx_mutex;



static const osMutexAttr_t s_tx_mutex_attr = {



    .name = "uart1TxMu",



};







static int Uart1_Port_WriteLocked(const uint8_t *data, uint16_t len, uint32_t timeout_ms)



{



    if ((data == NULL) || (len == 0U)) {



        return 0;



    }







    if (HAL_UART_Transmit(&huart1, (uint8_t *)data, len, timeout_ms) != HAL_OK) {



        return -1;



    }







    return (int)len;



}







void Uart1_Port_Init(void)



{



    UartRingBuf_Init(&s_rx_ring, s_rx_ring_storage, UART1_RX_RING_SIZE);



    s_tx_mutex = osMutexNew(&s_tx_mutex_attr);



}







void Uart1_Port_StartRx(void)



{



    (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);



}







UartRingBuf *Uart1_Port_RxRing(void)



{



    return &s_rx_ring;



}







int Uart1_Port_WriteByte(uint8_t byte)



{



    if (s_tx_mutex != NULL) {



        (void)osMutexAcquire(s_tx_mutex, osWaitForever);



    }







    if (Uart1_Port_WriteLocked(&byte, 1U, 100U) != 1) {



        if (s_tx_mutex != NULL) {



            (void)osMutexRelease(s_tx_mutex);



        }



        return -1;



    }







    if (s_tx_mutex != NULL) {



        (void)osMutexRelease(s_tx_mutex);



    }







    return 1;



}







int Uart1_Port_Write(const uint8_t *data, uint16_t len)



{



    int ret;







    if ((data == NULL) || (len == 0U)) {



        return 0;



    }







    if (s_tx_mutex != NULL) {



        (void)osMutexAcquire(s_tx_mutex, osWaitForever);



    }







    ret = Uart1_Port_WriteLocked(data, len, 200U);







    if (s_tx_mutex != NULL) {



        (void)osMutexRelease(s_tx_mutex);



    }







    return ret;



}







void Uart1_Port_OnRxByte(uint8_t byte)



{



    if (!UartRingBuf_Push(&s_rx_ring, byte)) {



        (void)UartRingBuf_Pop(&s_rx_ring, &byte);



        (void)UartRingBuf_Push(&s_rx_ring, byte);



    }



}







void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)



{



    if (huart->Instance == USART1) {



        Uart1_Port_OnRxByte(s_rx_byte);



        (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);



        return;



    }







    if (huart->Instance == USART3) {



        PM25_OnRxCplt();



    }



}







void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)



{



    if (huart->Instance == USART1) {



        __HAL_UART_CLEAR_OREFLAG(huart);



        __HAL_UART_CLEAR_NEFLAG(huart);



        __HAL_UART_CLEAR_FEFLAG(huart);



        (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);



        return;



    }







    if (huart->Instance == USART3) {



        __HAL_UART_CLEAR_OREFLAG(huart);



        __HAL_UART_CLEAR_NEFLAG(huart);



        __HAL_UART_CLEAR_FEFLAG(huart);



        PM25_Rx_Start();



    }



}



