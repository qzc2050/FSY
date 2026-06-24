#include "app_tasks.h"
#include "fmc.h"

#include "cmsis_os.h"

#include "uart1_port.h"

#include "fsy_link.h"

#include "fsy_upload.h"

#include "device_config.h"
#include "flash_fs_mutex.h"
#include "sensor_task.h"
#include "geiger_task.h"
#include "key_task.h"
#include "net_task.h"
#include "ui_task.h"
#include "net_tcp.h"
#include "w25q_port.h"
#include "uart_diag.h"
#include <stdio.h>
#include <string.h>



static void UartTask(void *argument);

static void UploadTask(void *argument);



#ifndef UART_RX_POLL_MS
#define UART_RX_POLL_MS 20U
#endif



static osThreadId_t uartTaskHandle;

static const osThreadAttr_t uartTaskAttributes = {

    .name = "uartTask",

    .stack_size = 512 * 4,

    .priority = (osPriority_t)osPriorityAboveNormal,

};



static osThreadId_t uploadTaskHandle;

static const osThreadAttr_t uploadTaskAttributes = {

    .name = "uploadTask",

    .stack_size = 512 * 4,

    .priority = (osPriority_t)osPriorityNormal,

};



static int UploadWriteBoth(const uint8_t *data, uint16_t len)
{
    (void)Uart1_Port_Write(data, len);
    (void)Net_Tcp_Write(data, len);
    return (int)len;
}

static void BootMessage(void)

{

    static const char msg[] = "NeiJi uart1 proto ready\r\n";

    UartDiag_Write(msg);

}



void App_TasksInit(void)

{

    UartDiag_Write("[APP] tasks init\r\n");

    flash_fs_mutex_init();

    Uart1_Port_Init();

    Uart1_Port_StartRx();

    /* LVGL 尽早启动，避免长时间停留在 main 里的纯色底 */
    Ui_TaskInit();
    UartDiag_Write("[APP] after Ui_TaskInit\r\n");

    BootMessage();

    Fsy_Upload_Init();

    DeviceConfig_TaskInit();

    (void)DeviceConfig_Init();

    (void)W25Q_Port_SelfTest();

    uartTaskHandle = osThreadNew(UartTask, NULL, &uartTaskAttributes);

    uploadTaskHandle = osThreadNew(UploadTask, NULL, &uploadTaskAttributes);

    Sensor_TaskInit();
    Geiger_TaskInit();
    Key_TaskInit();
    Net_TaskInit();

}



static void UartTask(void *argument)

{

    UartRingBuf *rb;



    (void)argument;

    rb = Uart1_Port_RxRing();



    for (;;) {

        Fsy_Link_OnUartBytes(rb, Uart1_Port_Write);

        (void)osDelay(UART_RX_POLL_MS);

    }

}



static void UploadTask(void *argument)

{

    (void)argument;



    for (;;) {

        (void)Fsy_Upload_Send(UploadWriteBoth);

        (void)osDelay(FSY_UPLOAD_PERIOD_MS);

    }

}


