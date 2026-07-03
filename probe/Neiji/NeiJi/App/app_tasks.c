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
#include "ws2812b.h"
#include "alarm_output.h"
#include "uart_diag.h"
#include "can_driver.h"
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



static int UploadWriteAll(const uint8_t *data, uint16_t len)
{
    return Fsy_Link_WriteUpload(data, len);
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

    (void)CanDriver_Init();

    /* 按键扫描先于 LVGL，供 lv_port_indev 读取 */
    Key_TaskInit();

    /* Flash 配置必须在 UI 之前加载，否则 UI bind 读到全零 */
    DeviceConfig_TaskInit();
    (void)DeviceConfig_Init();

    Ui_TaskInit();
    UartDiag_Write("[APP] after Ui_TaskInit\r\n");

    BootMessage();

    Fsy_Upload_Init();

    (void)W25Q_Port_SelfTest();

    uartTaskHandle = osThreadNew(UartTask, NULL, &uartTaskAttributes);

    uploadTaskHandle = osThreadNew(UploadTask, NULL, &uploadTaskAttributes);

		ws2812b_TaskInit();
    Sensor_TaskInit();
    Geiger_TaskInit();
    Net_TaskInit();
    
}



static void UartTask(void *argument)

{

    UartRingBuf *rb;



    (void)argument;

    rb = Uart1_Port_RxRing();



    for (;;) {

        Fsy_Link_OnUartBytes(rb, Fsy_Link_WriteUart);

        CanDriver_Poll();

        (void)osDelay(UART_RX_POLL_MS);

    }

}



static void UploadTask(void *argument)

{

    uint32_t last_sn_tick = 0U;

    (void)argument;



    for (;;) {

        uint32_t now = osKernelGetTickCount();

        (void)Fsy_Upload_Send(UploadWriteAll);

        if ((last_sn_tick == 0U) ||
            ((now - last_sn_tick) >= FSY_UPLOAD_SN_PERIOD_MS)) {
            last_sn_tick = now;
            (void)Fsy_Upload_SendSerial(UploadWriteAll);
        }

        (void)osDelay(FSY_UPLOAD_PERIOD_MS);

    }

}


