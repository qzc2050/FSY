#include "freertos_app.h"

#include "device_config.h"
#include "hist_record_app.h"
#include "i2c.h"

#include "env_monitor.h"
#include "lora.h"


/* START_TASK 任务配置 */
// 任务优先级
#define START_TASK_PRIO             1
// 任务堆栈大小
#define START_STK_SIZE              1024
// 任务句柄
TaskHandle_t StartTask_Handler;
// 任务函数
void start_task(void *pvParameters);


/* 串口指令处理任务配置 */
// 任务优先级
#define UART_RECV_TASK_PRIO         5
// 任务堆栈大小
#define UART_RECV_STK_SIZE          4096
// 任务句柄
TaskHandle_t Uart_Recv_Task_Handler;
// 任务函数
void Uart_Recv_Task(void *pvParameters);


/* LVGL 任务配置 */
// 任务优先级
#define LVGL_TASK_PRIO              10
// 任务堆栈大小
#define LVGL_STK_SIZE               1024
// 任务句柄
TaskHandle_t LVGL_Task_Handler;
//任务函数
void LVGL_Task(void *pvParameters);


/* 环境监测任务配置 */
// 任务优先级
#define ENV_MONITOR_TASK_PRIO       5
// 任务堆栈大小
#define ENV_MONITOR_STK_SIZE        1024
// 任务句柄
TaskHandle_t Env_Monitor_Task_Handler;
// 任务函数
void Env_Monitor_Task(void *pvParameters);


/* 盖革管任务配置 */
// 任务优先级
#define GEIGER_TASK_PRIO              10
// 任务堆栈大小
#define GEIGER_STK_SIZE               2048
// 任务句柄
TaskHandle_t Geiger_Task_Handler;
// 任务函数
void Geiger_Task(void *pvParameters);


/* USB读卡器任务配置 */
// 任务优先级
#define USB_DISK_TASK_PRIO            6
// 任务堆栈大小
#define USB_DISK_STK_SIZE             2048
// 任务句柄
TaskHandle_t USB_Disk_Task_Handler;
//任务函数
void USB_Disk_Task(void *pvParameters);


/* 按键任务配置 */
// 任务优先级
#define KEY_TASK_PRIO            11
// 任务堆栈大小
#define KEY_STK_SIZE             256
// 任务句柄
TaskHandle_t Key_Task_Handler;
//任务函数
void Key_Task(void *pvParameters);


/* WS2812 RGB 任务配置 */
// 任务优先级
#define WS2812_TASK_PRIO             4
// 任务堆栈大小
#define WS2812_STK_SIZE              256
// 任务句柄
TaskHandle_t WS2812_Task_Handler;


/* W5500 网络任务配置 */
// 任务优先级
#define W5500_TASK_PRIO               5
// 任务堆栈大小
#define W5500_STK_SIZE                8192
// 任务句柄
TaskHandle_t W5500_Task_Handler;
// 任务函数
void W5500_Task(void *pvParameters);

// W5500 接收缓冲区
// static uint8_t w5500_rx_buffer[2048];


extern Net_Device_t *net_w5500_dh;
extern Net_Device_t *net_can_dh;
extern Net_Device_t *net_lora_dh;
extern PCD_HandleTypeDef hpcd;
extern volatile uint8_t USB_STATUS_REG;			//USB状态
extern volatile uint8_t bDeviceState;			//USB连接 情况

/* USB MSC 运行时开关：默认关闭，由串口命令手动开启 */
static volatile uint8_t g_usb_msc_enable_req = 0U;
static volatile uint8_t g_usb_msc_started = 0U;

void USB_MSC_SetEnable(uint8_t enable)
{
    g_usb_msc_enable_req = (enable != 0U) ? 1U : 0U;
}

uint8_t USB_MSC_GetEnable(void)
{
    return g_usb_msc_enable_req;
}

uint8_t USB_MSC_IsStarted(void)
{
    return g_usb_msc_started;
}


/**
 * @brief   FreeRTOS入口函数
 * @param   无
 * @retval  无
 */
void freertos_task(void)
{
    RGB_Color_TypeDef Color = {0,0,0};
    
    ws2812b_init();
    State_Led_Show(10, Color, 10);
    HAL_Delay(100);

//    __HAL_TIM_SET_COMPARE(&htim12,TIM_CHANNEL_2,800);
//    HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_2);
    

    W25Qx_QSPI_Init();
    // QSPI_EnableMemoryMappedMode(&hqspi);

    w5500_rst_io_configuration();
    Reset_W5500();
    set_w5500_default();
    set_w5500_network();
    close(0);
    close(1);

    Dev_Protocol_init();

    xTaskCreate((TaskFunction_t )start_task,            /* 任务函数 */
                (const char*    )"start_task",          /* 任务名称 */
                (uint16_t       )START_STK_SIZE,        /* 任务堆栈大小 */
                (void*          )NULL,                  /* 传入给任务函数的参数 */
                (UBaseType_t    )START_TASK_PRIO,       /* 任务优先级 */
                (TaskHandle_t*  )&StartTask_Handler);   /* 任务句柄 */
    vTaskStartScheduler();
}

/**
 * @brief   start_task
 * @param   pvParameters : 传入参数(未用到)
 * @retval  无
 */
void start_task(void *pvParameters)
{
    int8_t err = 0;

    flash_fs_mutex_init();
    I2C_BusMutex_Init();
    // printf("\r\n[数据] 正在初始化数据管理（QSPI 直读直写）...\r\n");
    if (HistRecord_Init() != 0)
        printf("[数据] 历史记录初始化失败\r\n");
    else
        printf("[数据] 数据管理初始化完成。\r\n");
    if (DeviceConfig_Init() != 0)
        printf("[配置] DeviceConfig_Init 失败\r\n");

    if(LORA_Init())
        (void)LORA_SyncFromFlash();
    else
        printf("[LORA] 初始化失败\r\n");
    

    err = xTaskCreate((TaskFunction_t )Env_Monitor_Task,
                (const char*    )"Env_Monitor_Task",
                (uint16_t       )ENV_MONITOR_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )ENV_MONITOR_TASK_PRIO,
                (TaskHandle_t*  )&Env_Monitor_Task_Handler);
    if(err == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
        printf("Env_Monitor_Task create failed!\r\n");

    err = xTaskCreate((TaskFunction_t )Uart_Recv_Task,
                (const char*    )"Uart_Recv_Task",
                (uint16_t       )UART_RECV_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )UART_RECV_TASK_PRIO,
                (TaskHandle_t*  )&Uart_Recv_Task_Handler);
    if(err == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
        printf("Uart_Recv_Task create failed!\r\n");

    err = xTaskCreate((TaskFunction_t )LVGL_Task,
                (const char*    )"LVGL_Task",
                (uint16_t       )LVGL_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )LVGL_TASK_PRIO,
                (TaskHandle_t*  )&LVGL_Task_Handler);
    if(err == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
        printf("LVGL_Task create failed!\r\n");

    err = xTaskCreate((TaskFunction_t )Geiger_Task,
                (const char*    )"Geiger_Task",
                (uint16_t       )GEIGER_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )GEIGER_TASK_PRIO,
                (TaskHandle_t*  )&Geiger_Task_Handler);
    if(err == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
        printf("Geiger_Task create failed!\r\n");

    err = xTaskCreate((TaskFunction_t )Key_Task,
                (const char*    )"Key_Task",
                (uint16_t       )KEY_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )KEY_TASK_PRIO,
                (TaskHandle_t*  )&Key_Task_Handler);
    if(err == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
        printf("Key_Task create failed!\r\n");

    err = xTaskCreate((TaskFunction_t )ws2812_task,
                (const char*    )"ws2812_task",
                (uint16_t       )WS2812_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )WS2812_TASK_PRIO,
                (TaskHandle_t*  )&WS2812_Task_Handler);
    if(err == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
        printf("ws2812_task create failed!\r\n");

    err = xTaskCreate((TaskFunction_t )W5500_Task,
                (const char*    )"W5500_Task",
                (uint16_t       )W5500_STK_SIZE,
                (void*          )NULL,
                (UBaseType_t    )W5500_TASK_PRIO,
                (TaskHandle_t*  )&W5500_Task_Handler);
    if(err == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
        printf("W5500_Task create failed!\r\n");

    /* 用户寄存器初始化 */
    Net_User_Registers_Init();
    
//    err = xTaskCreate((TaskFunction_t )USB_Disk_Task,
//                (const char*    )"USB_Disk_Task",
//                (uint16_t       )USB_DISK_STK_SIZE,
//                (void*          )NULL,
//                (UBaseType_t    )USB_DISK_TASK_PRIO,
//                (TaskHandle_t*  )&USB_Disk_Task_Handler);
//    if(err == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
//        printf("USB_Disk_Task create failed!\r\n");

    vTaskDelete(NULL);
}

/**
 * @brief   环境监测任务
 * @param   pvParameters : 传入参数(未用到)
 * @retval  无
 */
void Env_Monitor_Task(void *pvParameters)
{
    /* 辐射数据定时上传时间管理 */
    static uint32_t upload_tk = 0;
    
    ENS160_Setting();
    bme280_app_init();
    AHT20_Init();
    
    /* 初始化 PM2.5 传感器 */
    PM2_5_Init();

    vTaskDelay(1000);
    
    while(1)
    {
        // float light = 0.2f;
        // RGB_Color_TypeDef Color = {(uint8_t)(255 * light),(uint8_t)(0 * light),(uint8_t)(0 * light)};
        // State_Led_Show(10, Color, 10);
        // vTaskDelay(1000);
        
        // RGB_Color_TypeDef Color1 = {(uint8_t)(0 * light),(uint8_t)(255 * light),(uint8_t)(0 * light)};
        // State_Led_Show(10, Color1, 10);
        // vTaskDelay(1000);
        
        // RGB_Color_TypeDef Color2 = {(uint8_t)(0 * light),(uint8_t)(0 * light),(uint8_t)(255 * light)};
        // State_Led_Show(10, Color2, 10);
        // vTaskDelay(1000);
        
//        RGB_Color_TypeDef Color3 = {(uint8_t)(255 * light),(uint8_t)(0 * light),(uint8_t)(255 * light)};
//        State_Led_Show(10, Color3, 10);
//        vTaskDelay(1000);

        /* 处理 PM2.5 数据 */
        /* 调用应用层处理函数，会自动调用 PM2_5_ProcessData() 并更新 last_valid_pm2_5 */
        PM2_5_App_Process();
        float pm2_5_val = PM2_5_App_GetPM2_5();
        if (pm2_5_val >= 0.0f) {
            env_data.PM2_5 = (uint16_t)pm2_5_val;
            
            /* 打印 PM2.5 数据到串口 */
//            printf("[PM2.5] PM2.5: %.1f ug/m3 (raw=%d)\r\n", 
//                    pm2_5_val, pm2_5_status.data.pm2_5_atmosphere);
        } else {
            /* 数据无效时，显示 0 */
            env_data.PM2_5 = 0;
//            printf("[PM2.5] Data invalid (raw=%d) -> display 0\r\n", pm2_5_status.data.pm2_5_atmosphere);
        }
        
        
        pcf8563_get_cur_time(&env_data.dt);
        // printf("20%02d/%02d/%02d %02d:%02d:%02d\r\n",env_data.dt.year,env_data.dt.month,env_data.dt.day,\
        //                                             env_data.dt.hour,env_data.dt.minute,env_data.dt.second);
        // AHT20_ReadOnce(&env_data.h6umidity, &env_data.temperature);
        bme280_get_real_data(&env_data.temperature, &env_data.humidity, &env_data.baro);
        ENS160_SetEnvData(&ens160, env_data.temperature, env_data.humidity);
        ENS160_Measure_Task();
        env_data.CO2 = ENS160_GetECO2(&ens160);
        
        /* 将传感器数据同步到寄存器表 */
        Net_Sync_SensorData_To_Registers();

        /* 辐射数据定时主动上传（使用 Dev_Tk_Wait 实现定时） */
        if(Dev_Tk_Wait(NET_ACTIVE_UPLOAD_PERIOD_MS, upload_tk))
        {
            /* 重置时间节点 */
            Dev_Tk_Init(&upload_tk);
            Net_Active_Upload_Scheduled();
        }

        vTaskDelay(1000);
    }
}

/**
 * @brief   串口接收处理任务
 * @param   pvParameters : 传入参数(未用到)
 * @retval  无
 */
void Uart_Recv_Task(void *pvParameters)
{
    while(1)
    {
        Uart_Data_Recv();
        vTaskDelay(50);
    }
}

/**
 * @brief   LVGL任务
 * @param   pvParameters : 传入参数(未用到)
 * @retval  无
 */
void LVGL_Task(void *pvParameters)
{
    // static uint32_t refresh_tk = 0;

//    button_adc_val_init();
    vTaskDelay(200);
    LCD_GC9503V_init();
    vTaskDelay(100);

    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    // LCD_Clear(GREEN);
    // vTaskDelay(400);
    // LCD_Clear(BLUE);
    // vTaskDelay(400);
    // LCD_Clear(WHITE);
    // vTaskDelay(400);

    ui_init();
    lv_timer_handler();       //LVGL界面刷新
    vTaskDelay(5);
    
//    __HAL_TIM_SET_COMPARE(&htim12,TIM_CHANNEL_1,500);
//    HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_1);
    // lv_demo_stress();

    while(1)
    {
        // vTaskDelay(lv_timer_handler());       //LVGL界面刷新
        lv_timer_handler();       //LVGL界面刷新
        vTaskDelay(5);
    }
}

/**
 * @brief   盖革管任务
 * @param   pvParameters : 传入参数(未用到)
 * @retval  无
 */
void Geiger_Task(void *pvParameters)
{
    Geiger_Init();
    
    while(1)
    {
        Geiger_Doserate_Calculate();    // 盖革管数据处理
        Dose_Rate_TH_Alarm();           // 报警检测
        Beep_Ctr(beep_event);           // 蜂鸣器任务
        vTaskDelay(10);
    }
}

void ws2812_task(void *pvParameters)
{
    uint8_t idx_val = 0;

    for(uint8_t i = 0; i < TOTAL_LED_COUNT; i++)
        rgb_color[i] = rgb_color_array[i];
    color_idx = TOTAL_LED_COUNT;

    ws2812_clr_mode(RGBLED_SHUTDOWN_MODE);
//    ws2812_set_mode(RGBLED_KEEP_1_MODE);

    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1)
    {
        if(rgb_ctrl.rgb_sta)
        {
            idx_val = rgb_ctrl.rgb_sta;
            for(uint8_t idx = 0; idx < 8; idx++)
            {
                if(idx_val & 0x01)
                {
                    switch(idx)
                    {
                        case RGBLED_SHUTDOWN_MODE:
                            idx = 8;
                            ws2812_shutdown();
                            ws2812_clr_mode(RGBLED_SHUTDOWN_MODE);
                            break;
                        case RGBLED_LPR_MODE:
                            ws2812_shutdown();
                            ws2812_clr_mode(RGBLED_LPR_MODE);
                            break;
                        case RGBLED_KEEP_1_MODE:
                            mode_keep1(0.05f);
                            break;
                        case RGBLED_KEEP_2_MODE:
                            rgb_ctrl.bits.keep_2 = 0;
                            break;
                        default: break;
                    }
                }
                idx_val >>= 1;
            }
            rgb_led_flush();
        }
        
        vTaskDelay(COMMON_DELAY);
    }
}

/**
 * @brief   按键任务
 * @param   pvParameters : 传入参数(未用到)
 * @retval  无
 */
void Key_Task(void *pvParameters)
{
    KEY_Init();
    
    while(1)
    {
        KEY_Scan();
        
        KEY_ID_t pressed_key = KEY_GetPressedKey();
        if(pressed_key != KEY_ID_MAX) {
            switch(pressed_key) {
                case KEY_ID_RETURN:
                    printf("[KEY] RETURN pressed\r\n");
                    break;
                case KEY_ID_UP:
                    printf("[KEY] UP pressed\r\n");
                    break;
                case KEY_ID_DOWN:
                    printf("[KEY] DOWN pressed\r\n");
                    break;
                case KEY_ID_OK:
                    printf("[KEY] OK pressed\r\n");
                    break;
                default:
                    break;
            }
        }
        
        vTaskDelay(20);
    }
}

/**
 * @brief   USB 磁盘任务
 * @param   pvParameters : 传入参数(未用到)
 * @retval  无
 */
void USB_Disk_Task(void *pvParameters)
{
    uint8_t USB_STA = 0U, Divece_STA = 0U;
    uint8_t tct = 0, offline_cnt = 0;
    USBD_HandleTypeDef USBD_Device = {0};

    USBD_Init(&USBD_Device, &MSC_Desc,0);                        // 初始化USB
	USBD_RegisterClass(&USBD_Device, USBD_MSC_CLASS);            // 添加类
	USBD_MSC_RegisterStorage(&USBD_Device, &USBD_DISK_fops);     // 为MSC类添加回调函数
	/* 先执行 MCU 本地 FatFs 测试，再启动 USB MSC。
	   否则当 USB 连接到 PC 时，diskio 会返回 NOT_READY 以避免双主机冲突。 */

    /* DataManager / DeviceConfig 已在 start_task 中初始化（见上文注释） */

    printf("[USB MSC] default OFF, use uart cmd: usb,on,end / usb,off,end\r\n");
    USB_STATUS_REG = 0;
    bDeviceState = 0;
    // USB_MSC_SetEnable(1);

    while(1)
    {
        if ((g_usb_msc_enable_req != 0U) && (g_usb_msc_started == 0U))
        {
            flash_fs_lock();
            USBD_Start(&USBD_Device);
            HAL_PWREx_EnableUSBVoltageDetector();
            g_usb_msc_started = 1U;
            USB_STATUS_REG = 0;
            bDeviceState = 0;
            printf("[USB MSC] started (MCU 本地 QSPI 写已暂停)\r\n");
            flash_fs_unlock();
            vTaskDelay(50);
        }
        else if ((g_usb_msc_enable_req == 0U) && (g_usb_msc_started != 0U))
        {
            flash_fs_lock();
            USBD_Stop(&USBD_Device);
            g_usb_msc_started = 0U;
            USB_STATUS_REG = 0;
            bDeviceState = 0;
            offline_cnt = 0;
            tct = 0;
            printf("[USB MSC] stopped\r\n");
            flash_fs_unlock();
            vTaskDelay(50);
        }

        if (g_usb_msc_started == 0U)
        {
            vTaskDelay(20);
            continue;
        }

        if(USB_STA != USB_STATUS_REG)     // 状态改变了 
        {
            if(USB_STATUS_REG & 0x01)     // 正在写
                printf("USB Writing...\r\n");  // 提示 USB正在写入数据
            if(USB_STATUS_REG & 0x02)     // 正在读
                printf("USB Reading...\r\n");  // 提示 USB正在读出数据
            if(USB_STATUS_REG & 0x04)
                printf("USB Write Err\r\n");   // 提示写入错误
            // else
            //     printf("清除显示！\r\n");       // 清除显示
            if(USB_STATUS_REG & 0x08)
                printf("USB Read  Err\r\n");   // 提示读出错误
            // else
            //     printf("清除显示！\r\n");       // 清除显示
            USB_STA = USB_STATUS_REG;          // 记录最后的状态
        }
        if(Divece_STA != bDeviceState)
        {
            if(bDeviceState == 1)
                printf("USB Connected\r\n");   // 提示 USB连接已经建立
            else 
                printf("USB DisConnected\r\n");   // 提示 USB被拔出了
            Divece_STA = bDeviceState;
        }
        tct++;
        if(tct == 200)
        {
            tct = 0;
            if(USB_STATUS_REG & 0x10)
            {
                offline_cnt = 0;    // USB连接了，则清除offline计数器
                bDeviceState = 1;
            }
            else    // 没有得到轮询 
            {
                offline_cnt++;
                if(offline_cnt > 10)
                    bDeviceState = 0;   // 2s内没收到在线标记，代表 USB被拔出了
            }
            USB_STATUS_REG = 0;
        }
        vTaskDelay(10);
    }
}


/**
 * @brief   W5500 网络任务
 * @param   pvParameters : 传入参数 (未用到)
 * @retval  无
 */
void W5500_Task(void *pvParameters)
{
//    int32_t recv_len = 0;
//    uint8_t socket_status[3] = {0};
    uint8_t phy_status = 0;
    uint8_t ip[4] = {0};
    uint8_t mac[6] = {0};
//    static uint32_t debug_cnt = 0;
//    static uint8_t last_link_status = 0;

    vTaskDelay(500);
    
    printf("\r\n========== W5500 Network Diagnostics ==========\r\n");
    
    printf("[W5500] Chip Version: 0x%02X\r\n", IINCHIP_READ(VERSIONR));
    
    getSHAR(mac);
    printf("[W5500] MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    getSIPR(ip);
    printf("[W5500] IP Address: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
    getSUBR(ip);
    printf("[W5500] Subnet Mask: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
    getGAR(ip);
    printf("[W5500] Gateway: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
    
    phy_status = getPHYCFGR();
    printf("[W5500] PHYCFGR Register: 0x%02X\r\n", phy_status);
    printf("[W5500]   - Link Status: %s\r\n", (phy_status & LINK) ? "UP" : "DOWN");
    printf("[W5500]   - Speed: %s\r\n", (phy_status & SPD) ? "100Mbps" : "10Mbps");
    printf("[W5500]   - Duplex: %s\r\n", (phy_status & DPX) ? "Full" : "Half");
    
    if(phy_status & LINK)
    {
        printf("[W5500] PHY Link: UP\r\n");
//        last_link_status = 1;
    }
    else
    {
        printf("[W5500] PHY Link: DOWN (Check network cable!)\r\n");
//        last_link_status = 0;
    }
    printf("==========================================\r\n\r\n");
    
    while(1)
    {
        /* DHCP 客户端任务（处理 DHCP 获取和重连） */
        DHCP_Client_Task();

        /* DHCP 看门狗：长时间无 IP 时硬复位 W5500 */
        DHCP_Watchdog_Task();

        if(W5500_Is_Network_Recovering())
        {
            vTaskDelay(2);
            continue;
        }

        /* UDP 广播任务（如果启用 DHCP） */
        UDP_Broadcast_Task();

        /* TCP 5001 listen 健康维护（与是否有客户端连接无关） */
        Net_Tcp_PeriodicMaintain();

        Net_Thread_Task();
        Net_5MinHistory_Upload_Task();
        Ota_Thread_Task();  /* OTA 会话超时 / DONE 后自动重启 */
        vTaskDelay(2);
    }
}



