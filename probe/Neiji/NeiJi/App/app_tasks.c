#include "app_tasks.h"
#include "fmc.h"

#include "cmsis_os.h"

#include "uart1_port.h"

#include "fsy_link.h"

#include "fsy_upload.h"

#include "device_config.h"
#include "flash_fs_mutex.h"
#include "ota.h"
#include "hist_5min.h"
#include "hist_5min_query.h"
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
#include "lora.h"
#include "i2c.h"
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

    /* OTA START 擦 Download 扇区在本任务上下文执行，需更大栈 */
    .stack_size = 1024 * 4,

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
    I2C_BusMutex_Init();

    Uart1_Port_Init();

    Uart1_Port_StartRx();

    (void)CanDriver_Init();

    /* 按键扫描先于 LVGL，供 lv_port_indev 读取 */
    Key_TaskInit();

    /* Flash 配置必须在 UI / LoRa 之前加载（reg123 bit9 控制 LoRa 使能） */
    DeviceConfig_TaskInit();
    (void)DeviceConfig_Init();
    (void)Hist5Min_Init();
    OTA_Init();

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

        /*
         * CAN 可能就是当前 OTA 数据通道，mute 只能抑制主动上报/非必要业务，
         * 不能停止 CAN 收包，否则 START 后的 DATA/DONE 永远无法处理。
         */
#if CAN_DRIVER_ENABLE
        CanDriver_Poll();
#endif

        /*
         * LoRa 也可能是当前 OTA 数据通道；与 CAN 一样，mute 期间仍须收包，
         * 否则 START 进入 mute 后 DATA/DONE 将永远无法处理。
         */
        if (LORA_IsEnabled()) {
            LORA_Poll();
        }

        OTA_Service();

        (void)osDelay((OTA_IsRealtimeMuted() != 0U) ? 2U : UART_RX_POLL_MS);

    }

}



static void UploadTask(void *argument)

{

    uint32_t last_sn_tick = 0U;
    uint32_t last_maintain_tick = 0U;
    uint16_t upload_fail_cnt = 0U;
    uint32_t phase_ms = Fsy_Upload_PhaseOffsetMs();

    (void)argument;

    if (phase_ms > 0U) {
        (void)osDelay(phase_ms);
    }

    for (;;) {

        uint32_t now = osKernelGetTickCount();
        int upload_rc;

        OTA_Service();
        if (OTA_IsRealtimeMuted() != 0U) {
            (void)osDelay(UART_RX_POLL_MS);
            continue;
        }

        Fsy_Link_PollUploadRoute();

        if ((last_maintain_tick == 0U) ||
            ((now - last_maintain_tick) >= 30000U)) {
            last_maintain_tick = now;
            Net_Tcp_PeriodicMaintain();
        }

        upload_rc = Fsy_Upload_Send(UploadWriteAll);
        if (upload_rc < 0) {
            if (upload_fail_cnt < 0xFFFFU) {
                upload_fail_cnt++;
            }
            if (upload_fail_cnt >= 10U) {
                upload_fail_cnt = 0U;
                Net_Tcp_PeriodicMaintain();
            }
        } else {
            upload_fail_cnt = 0U;
        }

        Hist5Min_Query_Pump();

        if ((last_sn_tick == 0U) ||
            ((now - last_sn_tick) >= FSY_UPLOAD_SN_PERIOD_MS)) {
            last_sn_tick = now;
            (void)Fsy_Upload_SendSerial(UploadWriteAll);
        }

        (void)osDelay(FSY_UPLOAD_PERIOD_MS);

    }

}


