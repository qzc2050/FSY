#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cmd.h"
#include "beep.h"
#include "geiger.h"
#include "device.h"
#include "config.h"
#include "hist_record_app.h"
#include "device_config.h"
#include "./core/dev_config.h"
#include "w5500_dhcp.h"
#include "w25qxx.h"
#include "ext_flash_layout.h"
#include "pcf8563.h"
#include "ws2812b.h"
#include "network_cmd.h"
#include "../dev_protocol/net_raw/net_raw_app.h"
#include "lora.h"
#include "dose_rate.h"


static int Cmd_CfgEnsureWritable(void);static int Cmd_LocalFlashBusy(void);


/* Net_Raw 协议寄存器同步接口 */
/* 无需 extern，net_raw_app.h 已声明 */
/* USB MSC 运行时开关接口（由 FreeRTOS APP 提供） */
extern void USB_MSC_SetEnable(uint8_t enable);
extern uint8_t USB_MSC_GetEnable(void);
extern uint8_t USB_MSC_IsStarted(void);
/* 与盖革任务一致：USB 连接或 MSC 已启动时禁止本地 QSPI 写 Flash */
extern volatile uint8_t bDeviceState;


/********************************************************************************************
* 函数名：Uart_Cmd_Tip
* 描述  ：从串口接收指令
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Uart_Part_Cmd_Tip(void)
{
    Uart_Cmd_Tip();
}

/********************************************************************************************
* 函数名：Uart_Cmd_Tip
* 描述  ：从串口接收指令
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Uart_Cmd_Tip(void)
{
    printf("\r\n★请发送以下指令进行操作: \r\n\r\n");

    printf("a、BootLoader:   \"BO\"\r\n");
    printf("b、盖革管计数:   \"CT\"\r\n");
    printf("c、设备信息:     \"IF\"\r\n");
    printf("d、参数配置:     \"PM\"\r\n");
    printf("e、硬件测试:     \"HT\"\r\n");
    printf("f、恢复出厂设置:  \"FD\"\r\n");

    printf("\r\n/-------------------- 测试指令 --------------------/\r\n\r\n");
    printf("1、DHCP 并立即重配：\"netmode,dhcp,end\"\r\n");
    printf("2、静态 IP 并立即重配：\"netmode,static,end\"\r\n");

    printf("\r\n/-------------------- 数据管理指令 --------------------/\r\n\r\n");
    printf("9、读全部 5 分钟：\"data5min,all,end\"\r\n");
    printf("10、模拟写 n 条 5 分钟：\"data5min,sim,n,end\"\r\n");
    printf("\r\n/-------------------- 设备配置 (cfg + end) --------------------/\r\n");
    printf("12、读取配置：\"cfg,read,end\" (打印 Flash 中全部参数)\r\n");
    printf("13、写序列号：\"cfg,sn,xxx,end\" (≤%d 字)\r\n", DEVICE_CFG_SN_LEN);
    printf("14、写硬件版本：\"cfg,hw,xxx,end\" (≤%d 字)\r\n", DEVICE_CFG_HW_VER_LEN);
    printf("15、写盖革灵敏度：\"cfg,sens,xxx,end\" (cpm/uSv/h)\r\n");
    printf("16、显示屏开关：\"cfg,display,xxx,end\" (0=关 1=开)\r\n");
    printf("17、设备地址：\"cfg,addr,xxx,end\" (0 ~ 255)\r\n");
    printf("18、系统语言：\"cfg,lang,xxx,end\" (0=中文 1=English)\r\n");
    printf("19、剂量率上限阈值: \"cfg,rate,hi,xxx,end\" (uSv/h)\r\n");
    printf("20、剂量率下限阈值: \"cfg,rate,lo,xxx,end\" (uSv/h)\r\n");
    printf("21、温度上限阈值: \"cfg,temp,hi,xxx,end\" (C)\r\n");
    printf("22、温度下限阈值: \"cfg,temp,lo,xxx,end\" (C)\r\n");
    printf("23、气压上限阈值: \"cfg,press,hi,xxx,end\" (hPa)\r\n");
    printf("24、气压下限阈值: \"cfg,press,lo,xxx,end\" (hPa)\r\n");
    printf("25、湿度上限阈值: \"cfg,hum,hi,xxx,end\" (%%RH)\r\n");
    printf("26、湿度下限阈值: \"cfg,hum,lo,xxx,end\" (%%RH)\r\n");
    printf("27、CO2 上限阈值: \"cfg,co2,hi,xxx,end\" (ppm)\r\n");
    printf("28、CO2 下限阈值: \"cfg,co2,lo,xxx,end\" (ppm)\r\n");
    printf("29、PM2.5 上限阈值: \"cfg,pm25,hi,xxx,end\" (0 ~ 65535)\r\n");
    printf("30、PM2.5 下限阈值: \"cfg,pm25,lo,xxx,end\" (0 ~ 65535)\r\n");
    printf("31、声报警开关: \"cfg,alarm,sound,xxx,end\" (0=关 1=开)\r\n");
    printf("32、光报警开关: \"cfg,alarm,light,xxx,end\" (0=关 1=开)\r\n");
    printf("33、报警音量：\"cfg,alarm,volume,xxx,end\" (0 ~ 100 %%)\r\n");
    printf("34、屏幕亮度：\"cfg,bright,xxx,end\" (0 ~ 100 %%)\r\n");
    printf("35、清空数据：\"CLR,5min|all,end\" (清空历史数据记录)\r\n");
    printf("36、设 LoRa 地址：\"lora,addr,xxx,end\" (范围：0 ~ 255)\r\n");
    printf("37、设 LoRa 信道：\"lora,chan,xxx,end\" (范围：0 ~ 31，频率=410+chan MHz)\r\n");
    printf("38、设 LoRa 空中速率：\"lora,rate,xxx,end\"\r\n");
    printf("    (0=300bps  1=1.2kbps  2=2.4kbps  3=4.8kbps  4=9.6kbps  5=19.2kbps)\r\n");
    printf("39、设 LoRa 发射功率：\"lora,power,xxx,end\"\r\n");
    printf("    (0=20dBm  1=17dBm  2=14dBm  3=10dBm)\r\n");
    printf("40、设置时间：\"settime,YYMMDD,HHMMSS,end\" (如：settime,260409,172733,end)\r\n");
    // printf("9、开启 U 盘 (USB MSC): \"usb,on,end\"\r\n");
    // printf("10、关闭 U 盘 (USB MSC): \"usb,off,end\"\r\n");
    // printf("11、查询 U 盘状态：\"usb,status,end\"\r\n");

    printf("\r\n/-------------------- 剂量率 EWMA 算法配置 --------------------/\r\n\r\n");
    printf("41、打印配置：\"drcfg,end\"\r\n");
    printf("42、设置输入模式：\"setdrmode,x,end\" (0=真实，1=模拟，2=手动)\r\n");
    printf("43、读取输入模式：\"getdrmode,end\"\r\n");
    printf("44、设置手动 CPS: \"setdrcps,xxx,end\"\r\n");
    printf("44、设置阈值 CPS: \"setdrth,xxx,end\"\r\n");
    printf("45、设置突变阈值：\"setdrdelta,xxx,end\"\r\n");
    printf("46、设置慢速 alpha: \"setdralow,x.xxx,end\" (0~1)\r\n");
    printf("47、设置快速 alpha: \"setdrahigh,x.xxx,end\" (0~1)\r\n");
    printf("48、设置 Boost 时长：\"setdrboost,xxx,end\" (秒)\r\n");
    printf("49、设置全部参数：\"setdrall,t,d,al,ah,b,end\"\r\n");
    printf("50、重置 EWMA 状态：\"setdrreset,end\"\r\n");

    // printf("\r\n/-------------------- 生产指令 --------------------/\r\n\r\n");
    
}

/** 恢复出厂设置（保留 SN/硬件版本/灵敏度/设备地址/语言） */
static void Cmd_Factory_Reset(void)
{
    int ret;
    int lora_ok;
    LORA_Config_t lora_cfg;

    printf("[CMD] 正在恢复出厂设置...\r\n");

    if (Cmd_CfgEnsureWritable() != 0) {
        printf("[CMD] 无法执行恢复出厂设置\r\n");
        return;
    }

    sys_cfg.th_rl_rate = DEVICE_CFG_DEFAULT_RATE_TH_RL;
    sys_cfg.th_rh_rate = DEVICE_CFG_DEFAULT_RATE_TH_RH;
    sys_cfg.th_rh_rate_saved = 0.0f;
    sys_cfg.th_rl_rate_saved = 0.0f;
    sys_cfg.dose_th_shadow_flags = 0U;
    sys_cfg.alarm_volume_saved = 0U;
    sys_cfg.temp_th_hi = DEVICE_CFG_DEFAULT_TEMP_TH_HI;
    sys_cfg.temp_th_lo = DEVICE_CFG_DEFAULT_TEMP_TH_LO;
    sys_cfg.press_th_hi = DEVICE_CFG_DEFAULT_PRESS_TH_HI;
    sys_cfg.press_th_lo = DEVICE_CFG_DEFAULT_PRESS_TH_LO;
    sys_cfg.hum_th_hi = DEVICE_CFG_DEFAULT_HUM_TH_HI;
    sys_cfg.hum_th_lo = DEVICE_CFG_DEFAULT_HUM_TH_LO;
    sys_cfg.co2_th_hi = DEVICE_CFG_DEFAULT_CO2_TH_HI;
    sys_cfg.co2_th_lo = DEVICE_CFG_DEFAULT_CO2_TH_LO;
    sys_cfg.pm25_th_hi = DEVICE_CFG_DEFAULT_PM25_TH_HI;
    sys_cfg.pm25_th_lo = DEVICE_CFG_DEFAULT_PM25_TH_LO;
    sys_cfg.alarm_sound = DEVICE_CFG_DEFAULT_ALARM_SOUND;
    sys_cfg.alarm_light = DEVICE_CFG_DEFAULT_ALARM_LIGHT;
    sys_cfg.alarm_volume = DEVICE_CFG_DEFAULT_ALARM_VOLUME;
    sys_cfg.display_enable = DEVICE_CFG_DEFAULT_DISPLAY;
    sys_cfg.bright_sz = (float)DEVICE_CFG_DEFAULT_BRIGHT;

    Alarm_Status_Clear();

    ret = DeviceConfig_WriteFromSysCfg();
    if (ret != 0) {
        printf("[CMD] 恢复出厂设置失败（写入 Flash 错误 %d）\r\n", ret);
        return;
    }

    Net_Config_Sync_All();
    Net_Device_Update_Addr();

    /* HistRecord_Clear 内部已加 flash_fs_lock，此处不可再包一层（非递归锁会死锁） */
    ret = HistRecord_Clear();
    if (ret != 0) {
        printf("[CMD] 配置已恢复，但擦除 5 分钟记录区失败 (err=%d)\r\n", ret);
        return;
    }
    printf("[CMD] 5 分钟历史数据已清空\r\n");

    lora_ok = LORA_Config(LORA_CFG_DEFAULT, &lora_cfg) ? 1 : 0;
    if (lora_ok) {
        printf("[CMD] LoRa 模块已恢复出厂参数（addr=%u, chan=%u/%luMHz）\r\n",
                (unsigned)sys_cfg.dev_addr,
                (unsigned)LORA_DEFAULT_CHAN,
                (unsigned long)LORA_ChanToMHz(LORA_DEFAULT_CHAN));
    } else {
        printf("[CMD] LoRa 模块恢复出厂参数失败\r\n");
    }

    if (lora_ok) {
        printf("[CMD] 恢复出厂设置成功！\r\n");
    } else {
        printf("[CMD] 恢复出厂设置完成（LoRa 未恢复，请检查模块）\r\n");
    }
    printf("[CMD] 剂量率阈值：%.2f-%.2f uSv/h\r\n", 
            (double)sys_cfg.th_rl_rate, (double)sys_cfg.th_rh_rate);
    printf("[CMD] 温度阈值：%.2f-%.2f ℃\r\n", 
            (double)sys_cfg.temp_th_lo, (double)sys_cfg.temp_th_hi);
    printf("[CMD] 气压阈值：%.1f-%.1f hPa\r\n", 
            (double)sys_cfg.press_th_lo, (double)sys_cfg.press_th_hi);
    printf("[CMD] 湿度阈值：%.1f-%.1f %%RH\r\n", 
            (double)sys_cfg.hum_th_lo, (double)sys_cfg.hum_th_hi);
    printf("[CMD] CO2 阈值：%u-%u ppm\r\n", 
            (unsigned)sys_cfg.co2_th_lo, (unsigned)sys_cfg.co2_th_hi);
    printf("[CMD] PM2.5 阈值：%u-%u ug/m³\r\n", 
            (unsigned)sys_cfg.pm25_th_lo, (unsigned)sys_cfg.pm25_th_hi);
    printf("[CMD] 声报警：%s\r\n", sys_cfg.alarm_sound ? "开" : "关");
    printf("[CMD] 光报警：%s\r\n", sys_cfg.alarm_light ? "开" : "关");
    printf("[CMD] 报警音量：%u%%\r\n", (unsigned)sys_cfg.alarm_volume);
    printf("[CMD] 显示屏：%s\r\n", sys_cfg.display_enable ? "开" : "关");
    printf("[CMD] 屏幕亮度：%u%%\r\n", (unsigned)sys_cfg.bright_sz);
}

/** 设置 PCF8563 时间 */
static void Cmd_Set_Time(const char *date_str, const char *time_str)
{
    DateTime_t dt;
    int year, hour;
    
    /* 检查参数 */
    if (date_str == NULL || time_str == NULL) {
        printf("[CMD] 参数错误\r\n");
        return;
    }
    
    /* 解析日期：YYMMDD */
    if (strlen(date_str) != 6) {
        printf("[CMD] 日期格式错误（应为 YYMMDD，如 260409）\r\n");
        return;
    }
    year = atoi(date_str);
    if (year < 0 || year > 999999) {
        printf("[CMD] 日期数值错误\r\n");
        return;
    }
    dt.year = year / 10000;        /* 年份后两位（如 26） */
    dt.month = (year % 10000) / 100;
    dt.day = year % 100;
    
    /* 解析时间：HHMMSS */
    if (strlen(time_str) != 6) {
        printf("[CMD] 时间格式错误（应为 HHMMSS，如 172733）\r\n");
        return;
    }
    hour = atoi(time_str);
    if (hour < 0 || hour > 235959) {
        printf("[CMD] 时间数值错误\r\n");
        return;
    }
    dt.hour = hour / 10000;
    dt.minute = (hour % 10000) / 100;
    dt.second = hour % 100;
    
    /* 验证日期时间范围 */
    if (dt.month < 1 || dt.month > 12 || dt.day < 1 || dt.day > 31 ||
        dt.hour > 23 || dt.minute > 59 || dt.second > 59) {
        printf("[CMD] 日期或时间超出范围\r\n");
        return;
    }
    
    dt.week = 0;  /* 星期暂不设置 */
    
    printf("[CMD] 设置时间：%02d-%02d-%02d %02d:%02d:%02d\r\n",
           dt.year % 100, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    
    /* 写入 PCF8563 */
    pcf8563_set_cur_time(&dt);
    
    printf("[CMD] 时间设置成功\r\n");
}

/** 清空数据记录 */
static void Cmd_Clear_Data(const char *type)
{
    int ret;
    const char *label = NULL;
    uint8_t clear_5min = 0;
    
    /* 检查参数 */
    if (type == NULL) {
        printf("[CMD] 参数错误\r\n");
        return;
    }
    
    /* 确定要清空的数据类型 */
    if (!strcasecmp(type, "all")) {
        clear_5min = 1;
        label = "全部";
    } else if (!strcasecmp(type, "5min") || !strcasecmp(type, "5minall")) {
        clear_5min = 1;
        label = "5 分钟";
    } else {
        printf("[CMD] 未知类型：%s (使用 5min/all)\r\n", type);
        return;
    }
    
    /* 检查是否可写 */
    if (Cmd_LocalFlashBusy()) {
        printf("[CMD] SPI Flash 被 USB 占用，无法清空数据\r\n");
        return;
    }
    
    printf("[CMD] 正在清空%s数据...\r\n", label);

    if (clear_5min) {
        /* HistRecord_Clear 内部已加 flash_fs_lock */
        ret = HistRecord_Clear();
        if (ret != 0) {
            printf("[CMD] 清空 5 分钟数据失败 (err=%d)\r\n", ret);
            return;
        }
    }
    
    printf("[CMD] 已清空%s数据\r\n", label);
    if (clear_5min) {
        DM_Manager_t *mgr = HistRecord_GetManager();
        if (mgr != NULL) {
            printf("[CMD] 5 分钟：条数=%u，下一写槽=%u，阶段=%s\r\n",
                    (unsigned)mgr->valid_count,
                    (unsigned)mgr->write_next,
                    (mgr->phase == DM_PHASE_CACHE) ? "CACHE" : "PRIMARY");
        }
    }
}



static int Cmd_LocalFlashBusy(void)
{
    if (USB_MSC_IsStarted()) {
        return 1;
    }
    return (bDeviceState == 1U) ? 1 : 0;
}

/** cfg 写 Flash 前统一检查（与 cfg,sn 等一致） */
static int Cmd_CfgEnsureWritable(void)
{
    if (!DeviceConfig_IsReady()) {
        printf("[CMD] 设备配置尚未初始化（等待 DeviceConfig_Init）\r\n");
        return -1;
    }
    if (Cmd_LocalFlashBusy()) {
        printf("[CMD] SPI Flash 被 USB 占用，无法写入设备配置\r\n");
        return -1;
    }
    return 0;
}

/**
 * 五段 cfg：cfg,类,hi|lo|sound|light,xxx,end（xxx 见帮助括号说明）
 * 成功时仅修改 sys_cfg，由调用方执行 DeviceConfig_WriteFromSysCfg。
 */
static int Cmd_CfgApply5Seg(char gstr[16][16])
{
    float vf;
    unsigned long temp;
    unsigned ttt;

    if (!strcasecmp(gstr[1], "rate"))
    {
        vf = (float)atof(gstr[3]);
        if (!strcasecmp(gstr[2], "hi"))
            sys_cfg.th_rh_rate = (float)((uint32_t)(vf * 100.0f)) / 100.0f;
        else if (!strcasecmp(gstr[2], "lo"))
            sys_cfg.th_rl_rate = (float)((uint32_t)(vf * 100.0f)) / 100.0f;
        else
            return -1;
    }
    else if (!strcasecmp(gstr[1], "temp"))
    {
        vf = (float)atof(gstr[3]);
        if (!strcasecmp(gstr[2], "hi"))
            sys_cfg.temp_th_hi = (float)((uint32_t)(vf * 10)) / 10.0f;
        else if (!strcasecmp(gstr[2], "lo"))
            sys_cfg.temp_th_lo = (float)((uint32_t)(vf * 10)) / 10.0f;
        else
            return -1;
    }
    else if (!strcasecmp(gstr[1], "press"))
    {
        vf = (float)atof(gstr[3]);
        if (!strcasecmp(gstr[2], "hi"))
            sys_cfg.press_th_hi = (float)((uint32_t)(vf * 10)) / 10.0f;
        else if (!strcasecmp(gstr[2], "lo"))
            sys_cfg.press_th_lo = (float)((uint32_t)(vf * 10)) / 10.0f;
        else
            return -1;
    }
    else if (!strcasecmp(gstr[1], "hum"))
    {
        vf = (float)atof(gstr[3]);
        if (!strcasecmp(gstr[2], "hi"))
            sys_cfg.hum_th_hi = (float)((uint32_t)(vf * 10)) / 10.0f;
        else if (!strcasecmp(gstr[2], "lo"))
            sys_cfg.hum_th_lo = (float)((uint32_t)(vf * 10)) / 10.0f;
        else
            return -1;
    }
    else if (!strcasecmp(gstr[1], "co2"))
    {
        temp = strtoul(gstr[3], NULL, 10);
        if (!strcasecmp(gstr[2], "hi"))
            sys_cfg.co2_th_hi = (uint32_t)temp;
        else if (!strcasecmp(gstr[2], "lo"))
            sys_cfg.co2_th_lo = (uint32_t)temp;
        else
            return -1;
    }
    else if (!strcasecmp(gstr[1], "pm25"))
    {
        ttt = (unsigned)strtoul(gstr[3], NULL, 10);
        if (ttt > 0xFFFFu)
            return -1;
        if (!strcasecmp(gstr[2], "hi"))
            sys_cfg.pm25_th_hi = (uint32_t)ttt;
        else if (!strcasecmp(gstr[2], "lo"))
            sys_cfg.pm25_th_lo = (uint32_t)ttt;
        else
            return -1;
    }
    else if (!strcasecmp(gstr[1], "alarm"))
    {
        ttt = (unsigned)strtoul(gstr[3], NULL, 10);
        if (!strcasecmp(gstr[2], "sound"))
        {
            if (ttt > 1U)
                return -1;
            sys_cfg.alarm_sound = (uint8_t)ttt;
        }
        else if (!strcasecmp(gstr[2], "light"))
        {
            if (ttt > 1U) 
                return -1;
            sys_cfg.alarm_light = (uint8_t)ttt;
        }
        else if (!strcasecmp(gstr[2], "volume"))
        {
            if (ttt > 100U)
                return -1;
            sys_cfg.alarm_volume = (uint8_t)ttt;
        }
        else
            return -1;
    }
    else
        return -1;

    return 0;
}

/* cmd_sim_fill_5min 已删除，改用直接计算剂量值 */

static void Cmd_Lora_Handle(const char *param, const char *value_str)
{
    uint32_t value;
    LORA_Param_t pid;

    if(param == NULL || value_str == NULL)
    {
        printf("[CMD] LoRa 用法: lora,参数,数值,end\r\n");
        return;
    }

    value = (uint32_t)strtoul(value_str, NULL, 0);

    if(!strcasecmp(param, "addr") || !strcasecmp(param, "address"))
        pid = LORA_PARAM_ADDR;
    else if(!strcasecmp(param, "chan") || !strcasecmp(param, "channel"))
        pid = LORA_PARAM_CHANNEL;
    else if(!strcasecmp(param, "rate") || !strcasecmp(param, "air") || !strcasecmp(param, "airrate"))
        pid = LORA_PARAM_AIR_RATE;
    else if(!strcasecmp(param, "power") || !strcasecmp(param, "txpower"))
        pid = LORA_PARAM_TX_POWER;
    else
    {
        printf("[CMD] LoRa 未知参数: %s\r\n", param);
        printf("[CMD] 支持: addr/chan/rate/power (读配置请用 IF)\r\n");
        return;
    }

    if(pid == LORA_PARAM_CHANNEL && value > LORA_CHAN_MAX)
    {
        printf("[CMD] LoRa 信道范围 0~%u\r\n", (unsigned)LORA_CHAN_MAX);
        return;
    }
    if(pid == LORA_PARAM_AIR_RATE && value > LORA_AIR_RATE_19K2BPS)
    {
        printf("[CMD] LoRa 空中速率范围 0~%u\r\n", (unsigned)LORA_AIR_RATE_19K2BPS);
        return;
    }
    if(pid == LORA_PARAM_TX_POWER && value > LORA_TX_POWER_10DBM)
    {
        printf("[CMD] LoRa 发射功率范围 0~%u\r\n", (unsigned)LORA_TX_POWER_10DBM);
        return;
    }
    if(pid == LORA_PARAM_ADDR && value > 0xFFFFU)
    {
        printf("[CMD] LoRa 地址范围 0~65535\r\n");
        return;
    }

    if(!LORA_Param(pid, &value, true))
    {
        printf("[CMD] LoRa 写入失败: %s=%lu\r\n", param, (unsigned long)value);
        return;
    }

    printf("[CMD] LoRa 已写入: %s=%lu\r\n", param, (unsigned long)value);
    LORA_ConfigPrint();
}

/********************************************************************************************
* 函数名：Uart_Data_Recv
* 描述  ：串口数据接收
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Uart_Data_Recv(void)
{
    char *p;
    char gstr[16][16];
    static bool ht_b = false;

    uint8_t len = 0,i = 0;
	
    if (UART_RX_STA & 0x8000) {     //接受完成
        len = UART_RX_STA & 0x3fff;   //得到此次接收到的数据长度
        UART_RX_STA = 0;
		
        if ((len < 4) && len) {
            if (UART_RX_BUF[0] == 'B') {
                if (UART_RX_BUF[1] == 'O') {
                    printf("跳转到系统BootLoader！\r\n\r\n");
                    // JumpToBootloader();
                    return;
                }
                // else if (USART1_RX_BUF[0] == 'B') {
                //     if (USART1_RX_BUF[1] == 'T') {
                //         if(beep_test)
                //         {
                //             beep_test = false;
                //             Beep_Ctr(BEEP_EVENT_STOP_TEST);
                //             printf("beep test off!\r\n");
                //         }
                //         else
                //         {
                //             beep_test = true;
                //             Beep_Ctr(BEEP_EVENT_TEST);
                //             printf("beep test on!\r\n");
                //         }
                //     }
                //     return;
                // }
            }
            else if (UART_RX_BUF[0] == 'C') {
                if (UART_RX_BUF[1] == 'M' && UART_RX_BUF[2] == 'D') {
                    Uart_Cmd_Tip();
                    return;
                }
				else if(UART_RX_BUF[1] == 'T') {
                    one_second_cnt_func = !one_second_cnt_func;
                    printf("Cnt test %s!\r\n",one_second_cnt_func ? "on":"off");
                    return;
				}
            }
            else if (UART_RX_BUF[0] == 'F') {
                if (UART_RX_BUF[1] == 'D') {
                    Cmd_Factory_Reset();
                    return;
                }
            }
            else if (UART_RX_BUF[0] == 'H') {
                if (UART_RX_BUF[1] == 'T') {
                    if(ht_b)
                    {
                        ht_b = false;
                        Beep_Ctr(BEEP_EVENT_STOP_TEST);
                        
                        // RGB_Color_TypeDef Color = {0,0,0};
                        // State_Led_Show(TOTAL_LED_COUNT, Color, TOTAL_LED_COUNT);
                        printf("关闭声光测试！\r\n");
                    }
                    else
                    {
                        ht_b = true;
                        Beep_Ctr(BEEP_EVENT_TEST);

                        // RGB_Color_TypeDef Color = {0,255,0};
                        // State_Led_Show(TOTAL_LED_COUNT, Color, TOTAL_LED_COUNT);
                        printf("打开声光测试！\r\n");
                    }
                    return;
                }
            }
            else if (UART_RX_BUF[0] == 'I') {
                if (UART_RX_BUF[1] == 'F') {
                    printf("SN:%s\r\n", sys_cfg.SN);
                    printf("HW:%s\r\n", sys_cfg.hw_version);
                    printf("SW:%s\r\n", DEVICE_SOFTWARE_VERSION);
                    printf("SD:%s  %s\r\n",__DATE__,__TIME__);
                    /* 获取并打印 IP 地址 */
                    uint8_t ip[4] = {0};
                    W5500_Get_IP(ip);
                    printf("IP:%d.%d.%d.%d : ctrl -> %d data -> %d\r\n", \
                        ip[0], ip[1], ip[2], ip[3], SETTING_SOCKET_PORT, DATA_UPLOAD_SOCKET_PORT);
                    LORA_ConfigPrint();
                    return;
                }
            }
            else if (UART_RX_BUF[0] == 'P') {
                if (UART_RX_BUF[1] == 'M') {
                    printf("ADDR:%#02X\r\n", sys_cfg.dev_addr);
                    printf("SEN:%.2f cpm/uSv/h\r\n", sys_cfg.sensitivity);
                    return;
                }
            }
        }
		
        if (len != 0)
        {
            i = 0;
            p = strtok((char*)UART_RX_BUF, ",");
            /* gstr 仅 [0..15]：超过 16 段会越界曾导致 HardFault；单段过长用 strncpy */
            while (p != NULL && i < 16U)
            {
                strncpy(gstr[i], p, sizeof(gstr[0]) - 1U);
                gstr[i][sizeof(gstr[0]) - 1U] = '\0';
                /* 去掉串口工具可能附带的 \r\n，避免 end 匹配失败 */
                gstr[i][strcspn(gstr[i], "\r\n")] = '\0';
                i++;
                p = strtok(NULL, ",");
            }

            memset(UART_RX_BUF, 0, UART_RECV_LEN);
            
            if(!strcasecmp(gstr[0], "setsen") && !strcasecmp(gstr[2], "end"))
			{
                sys_cfg.sensitivity = (float)atof(gstr[1]);
				printf("灵敏度: %.2f cpm/uSv/h\r\n", (double)sys_cfg.sensitivity);
                if (!DeviceConfig_IsReady()) {
                    printf("[CMD] 配置未就绪（DeviceConfig 未初始化），灵敏度仅更新 RAM\r\n");
                } else if (Cmd_LocalFlashBusy()) {
                    printf("[CMD] USB 占用 Flash，未写入 %s\r\n", DEVICE_CFG_STORAGE_LABEL);
                } else if (DeviceConfig_SetSensitivity(sys_cfg.sensitivity) != 0) {
                    printf("[CMD] 写入 %s 失败\r\n", DEVICE_CFG_STORAGE_LABEL);
                } else {
                    printf("[CMD] 已同步灵敏度到 %s\r\n", DEVICE_CFG_STORAGE_LABEL);
                }
                return;
			}
            /* 剂量率 EWMA 算法配置指令 */
            else if(!strcasecmp(gstr[0], "drcfg") && !strcasecmp(gstr[1], "end"))
            {
                DoseRate_PrintConfig();
                return;
            }
            else if(!strcasecmp(gstr[0], "setdrmode") && !strcasecmp(gstr[2], "end"))
            {
                uint8_t mode = (uint8_t)atoi(gstr[1]);
                if(!DoseRate_SetInputMode(mode))
                {
                    printf("[CMD] setdrmode err! mode:0/1/2\r\n");
                    return;
                }
                printf("[CMD] dr mode: %d (0=real 1=sim 2=manual)\r\n", mode);
                return;
            }
            else if(!strcasecmp(gstr[0], "getdrmode") && !strcasecmp(gstr[1], "end"))
            {
                uint8_t mode = DoseRate_GetInputMode();
                const char* mode_str = (mode == 0) ? "real" : (mode == 1) ? "sim" : (mode == 2) ? "manual" : "unknown";
                printf("[CMD] current dr mode: %d (%s)\r\n", mode, mode_str);
                return;
            }
            else if(!strcasecmp(gstr[0], "setdrcps") && !strcasecmp(gstr[2], "end"))
            {
                uint32_t cps = (uint32_t)atoi(gstr[1]);
                DoseRate_SetManualCps(cps);
                printf("[CMD] manual cps: %lu\r\n", (unsigned long)DoseRate_GetManualCps());
                return;
            }
            else if(!strcasecmp(gstr[0], "setdrth") && !strcasecmp(gstr[2], "end"))
            {
                if(!DoseRate_SetThresholdCps(atoi(gstr[1])))
                {
                    printf("[CMD] setdrth err! th>=0\r\n");
                    return;
                }
                printf("[CMD] threshold cps: %d\r\n", g_ewma_config.threshold_cps);
                return;
            }
            else if(!strcasecmp(gstr[0], "setdrdelta") && !strcasecmp(gstr[2], "end"))
            {
                if(!DoseRate_SetThresholdDelta(atoi(gstr[1])))
                {
                    printf("[CMD] setdrdelta err! delta>=0\r\n");
                    return;
                }
                printf("[CMD] threshold delta: %d\r\n", g_ewma_config.threshold_delta);
                return;
            }
            else if(!strcasecmp(gstr[0], "setdralow") && !strcasecmp(gstr[2], "end"))
            {
                if(!DoseRate_SetAlphaLow((float)atof(gstr[1])))
                {
                    printf("[CMD] setdralow err! 0<alpha<=1\r\n");
                    return;
                }
                printf("[CMD] alpha low: %.3f\r\n", g_ewma_config.alpha_low);
                return;
            }
            else if(!strcasecmp(gstr[0], "setdrahigh") && !strcasecmp(gstr[2], "end"))
            {
                if(!DoseRate_SetAlphaHigh((float)atof(gstr[1])))
                {
                    printf("[CMD] setdrahigh err! 0<alpha<=1\r\n");
                    return;
                }
                printf("[CMD] alpha high: %.3f\r\n", g_ewma_config.alpha_high);
                return;
            }
            else if(!strcasecmp(gstr[0], "setdrboost") && !strcasecmp(gstr[2], "end"))
            {
                if(!DoseRate_SetBoostDuration(atoi(gstr[1])))
                {
                    printf("[CMD] setdrboost err! 0<=boost<=600\r\n");
                    return;
                }
                printf("[CMD] boost duration: %d s\r\n", g_ewma_config.boost_duration);
                return;
            }
            else if(!strcasecmp(gstr[0], "setdrall") && !strcasecmp(gstr[6], "end"))
            {
                if(!DoseRate_SetThresholdCps(atoi(gstr[1])) ||
                    !DoseRate_SetThresholdDelta(atoi(gstr[2])) ||
                    !DoseRate_SetAlphaLow((float)atof(gstr[3])) ||
                    !DoseRate_SetAlphaHigh((float)atof(gstr[4])) ||
                    !DoseRate_SetBoostDuration(atoi(gstr[5])))
                {
                    printf("[CMD] setdrall err! t>=0 d>=0 0<a<=1 0<a<=1 0<=b<=600\r\n");
                    return;
                }
                DoseRate_PrintConfig();
                return;
            }
            else if(!strcasecmp(gstr[0], "setdrreset") && !strcasecmp(gstr[1], "end"))
            {
                DoseRate_ResetFilter();
                printf("[CMD] ewma state reset ok\r\n");
                return;
            }
            else if(!strcasecmp(gstr[0], "settime") && !strcasecmp(gstr[3], "end"))
            {
                Cmd_Set_Time(gstr[1], gstr[2]);
                return;
            }
            else if(!strcasecmp(gstr[0], "lora") && i >= 4U && !strcasecmp(gstr[3], "end"))
            {
                Cmd_Lora_Handle(gstr[1], gstr[2]);
                return;
            }
            else if(!strcasecmp(gstr[0], "netmode") && !strcasecmp(gstr[2], "end"))
            {
                if(!strcasecmp(gstr[1], "dhcp"))
                {
                    ConfigMsg.dhcp = 1;
                    printf("[CMD] Switch network mode -> DHCP\r\n");
                    set_w5500_network();
                    return;
                }
                else if(!strcasecmp(gstr[1], "static"))
                {
                    ConfigMsg.dhcp = 0;
                    printf("[CMD] Switch network mode -> STATIC\r\n");
                    set_w5500_network();
                    return;
                }
                else
                {
                    printf("[CMD] netmode invalid, use: netmode,dhcp,end or netmode,static,end\r\n");
                    return;
                }
            }
            else if(!strcasecmp(gstr[0], "usb") && !strcasecmp(gstr[2], "end"))
            {
                if(!strcasecmp(gstr[1], "on"))
                {
                    USB_MSC_SetEnable(1);
                    printf("[CMD] USB MSC enable request=ON\r\n");
                    return;
                }
                else if(!strcasecmp(gstr[1], "off"))
                {
                    USB_MSC_SetEnable(0);
                    printf("[CMD] USB MSC enable request=OFF\r\n");
                    return;
                }
                else if(!strcasecmp(gstr[1], "status"))
                {
                    printf("[CMD] USB MSC req=%s, started=%s\r\n",
                            USB_MSC_GetEnable() ? "ON" : "OFF",
                            USB_MSC_IsStarted() ? "YES" : "NO");
                    return;
                }
                else
                {
                    printf("[CMD] usb invalid, use: usb,on,end | usb,off,end | usb,status,end\r\n");
                    return;
                }
            }
            else if(!strcasecmp(gstr[0], "cfg") && !strcasecmp(gstr[2], "end"))
            {
                if (!strcasecmp(gstr[1], "read")) {
                    DeviceConfig_PrintFromFile();
                    return;
                }
            }
            else if(!strcasecmp(gstr[0], "cfg") && (i >= 5U) && !strcasecmp(gstr[4], "end"))
            {
                if (Cmd_CfgEnsureWritable() != 0)
                    return;
                if (Cmd_CfgApply5Seg(gstr) != 0) {
                    printf("[CMD] 五段格式错误。例：cfg,dose,hi,xxx,end  cfg,alarm,sound,xxx,end\r\n");
                    return;
                }
                if (DeviceConfig_WriteFromSysCfg() != 0)
                    printf("[CMD] 写入 %s 失败\r\n", DEVICE_CFG_STORAGE_LABEL);
                else {
                    printf("[CMD] 已更新阈值/报警并写入 %s\r\n", DEVICE_CFG_STORAGE_LABEL);
                    /* 同步到协议寄存器：更新辐射、环境参数阈值和报警使能 */
                    Net_Config_Sync_To_Registers(CFG_IDX_RATE);
                    Net_Config_Sync_To_Registers(CFG_IDX_ENV);
                    Net_Config_Sync_To_Registers(CFG_IDX_ALARM_EN);
                }
                return;
            }
            else if(!strcasecmp(gstr[0], "cfg") && !strcasecmp(gstr[3], "end"))
            {
                if (Cmd_CfgEnsureWritable() != 0) {
                    return;
                }
                if (!strcasecmp(gstr[1], "display")) {
                    sys_cfg.display_enable = (uint8_t)(atoi(gstr[2]) ? 1 : 0);
                    if (DeviceConfig_WriteFromSysCfg() != 0) {
                        printf("[CMD] 写入 %s 失败\r\n", DEVICE_CFG_STORAGE_LABEL);
                    } else {
                        printf("[CMD] display_enable=%u 已写入 %s\r\n",
                                (unsigned)sys_cfg.display_enable, DEVICE_CFG_STORAGE_LABEL);
                        /* 同步到协议寄存器（仅更新控制位） */
                        Net_Config_Sync_To_Registers(CFG_IDX_DEVICE_ADDR);
                    }
                    return;
                }
                if (!strcasecmp(gstr[1], "addr")) {
                    int a = atoi(gstr[2]);
                    if (a < 0 || a > 255) {
                        printf("[CMD] addr 范围 0-255\r\n");
                        return;
                    }
                    sys_cfg.dev_addr = (uint8_t)a;
                    if (DeviceConfig_WriteFromSysCfg() != 0) {
                        printf("[CMD] 写入 %s 失败\r\n", DEVICE_CFG_STORAGE_LABEL);
                    } else {
                        uint32_t lora_addr = (uint32_t)sys_cfg.dev_addr;
                        printf("[CMD] dev_addr=%u 已写入 %s\r\n",
                                (unsigned)sys_cfg.dev_addr, DEVICE_CFG_STORAGE_LABEL);
                        /* 同步到协议寄存器（仅更新地址） */
                        Net_Config_Sync_To_Registers(CFG_IDX_DEVICE_ADDR);
                        /* 更新协议设备地址 */
                        Net_Device_Update_Addr();
                        /* 同步 LoRa 模块地址 */
                        if(!LORA_Param(LORA_PARAM_ADDR, &lora_addr, true))
                            printf("[CMD] LoRa 模块地址同步失败\r\n");
                    }
                    return;
                }
                if (!strcasecmp(gstr[1], "bright")) {
                    int b = atoi(gstr[2]);
                    if (b < 0 || b > 100) {
                        printf("[CMD] bright 范围 0-100\r\n");
                        return;
                    }
                    sys_cfg.bright_sz = (uint8_t)b;
                    if (DeviceConfig_WriteFromSysCfg() != 0) {
                        printf("[CMD] 写入 %s 失败\r\n", DEVICE_CFG_STORAGE_LABEL);
                    } else {
                        printf("[CMD] 屏幕亮度=%u%% 已写入 %s\r\n",
                                (unsigned)sys_cfg.bright_sz, DEVICE_CFG_STORAGE_LABEL);
                    }
                    return;
                }
                if (!strcasecmp(gstr[1], "lang")) {
                    int lang = atoi(gstr[2]);
                    if (lang < 0 || lang > 1) {
                        printf("[CMD] lang 范围 0=中文 1=English\r\n");
                        return;
                    }
                    sys_cfg.language = (uint8_t)lang;
                    if (DeviceConfig_WriteFromSysCfg() != 0)
                        printf("[CMD] 写入 %s 失败\r\n", DEVICE_CFG_STORAGE_LABEL);
                    else
                    {
                        printf("[CMD] 语言=%s\r\n", lang == 0 ? "中文" : "English");
                        NVIC_SystemReset();
                    }
                    return;
                }
                if (!strcasecmp(gstr[1], "sn")) {
                    if (DeviceConfig_SetSn(gstr[2]) != 0) {
                        printf("[CMD] 写入 SN 失败\r\n");
                    } else {
                        printf("[CMD] SN 已写入：%s\r\n", gstr[2]);
                        Net_Config_Sync_To_Registers(CFG_IDX_DEVICE_INFO);
                    }
                    return;
                }
                if (!strcasecmp(gstr[1], "hw")) {
                    if (DeviceConfig_SetHwVer(gstr[2]) != 0) {
                        printf("[CMD] 写入 HWV 失败\r\n");
                    } else {
                        printf("[CMD] HWV 已写入：%s\r\n", gstr[2]);
                    }
                    return;
                }
                if (!strcasecmp(gstr[1], "sens")) {
                    sys_cfg.sensitivity = (float)atof(gstr[2]);
                    if (DeviceConfig_SetSensitivity(sys_cfg.sensitivity) != 0) {
                        printf("[CMD] 写入灵敏度失败\r\n");
                    } else {
                        printf("[CMD] 灵敏度 %.4f 已写入 %s\r\n",
                                (double)sys_cfg.sensitivity, DEVICE_CFG_STORAGE_LABEL);
                        /* 同步到协议寄存器 */
                        // Net_Config_Sync_To_Registers();
                    }
                    return;
                }
                printf("[CMD] 用法见 CMD 帮助（cfg,read,end 与 cfg 四段/五段）\r\n");
                return;
            }
            else if(!strcasecmp(gstr[0], "data5min") &&
                    ((i == 3 && !strcasecmp(gstr[2], "end")) || (i == 4 && !strcasecmp(gstr[3], "end"))))
            {
                if (!strcasecmp(gstr[2], "end"))
                {
                    if(!strcasecmp(gstr[1], "all"))
                    {
                        printf("[CMD] Reading all 5-min data...\r\n");
                        HistRecord_ReadAll();
                        return;
                    }
                    if(!strcasecmp(gstr[1], "sim"))
                    {
                        printf("[CMD] 请使用四段格式: data5min,sim,n,end\r\n");
                        return;
                    }
                    {
                        int index = atoi(gstr[1]);
                        printf("[CMD] Reading 5-min record %d...\r\n", index);
                        if(HistRecord_Print((uint16_t)index) != 0)
                            printf("[CMD] Invalid index or no data\r\n");
                        return;
                    }
                }
                if(!strcasecmp(gstr[1], "all"))
                {
                    printf("[CMD] Reading all 5-min data...\r\n");
                    HistRecord_ReadAll();
                    return;
                }
                else if(!strcasecmp(gstr[1], "sim"))
                {
                    int n = atoi(gstr[2]);
                    if(n <= 0 || n > (int)DATA_5_MIN_MAX_RECORDS)
                    {
                        printf("[CMD] Invalid count: %s, use 1-%u\r\n", gstr[2], (unsigned)DATA_5_MIN_MAX_RECORDS);
                        return;
                    }
                    if (Cmd_LocalFlashBusy()) {
                        printf("[CMD] SPI Flash 被 USB 占用，无法写入文件。请先 usb,off,end 或拔掉 USB 后再模拟。\r\n");
                        return;
                    }
                    printf("[CMD] Simulating %d 5-min data records...\r\n", n);
                    {
                        /* TODO: 批量写入功能暂未实现，改用单条写入 */
                        printf("[CMD] 批量模拟功能暂未实现，改用单条写入...\r\n");
                        for (int i = 0; i < n; i++) {
                            char datetime[20];
                            int day = 24 + (i / 288);
                            int hour = (i % 288) / 12;
                            int minute = (i % 12) * 5;
                            float dose = 1.0f + (i % 100) * 0.15f;  /* mSv */
                            uint32_t dose_uSv;
                            
                            sprintf(datetime, "202603%02d,%02d%02d00", day, hour, minute);
                            
                            /* 转换为微西弗 */
                            if (dose < 10.0f) {
                                dose_uSv = (uint32_t)(dose * 1000.0f);  /* mSv → uSv */
                            } else {
                                dose_uSv = (uint32_t)(dose * 1000000.0f);  /* mSv → uSv */
                            }
                            
                            {
                                int wr = HistRecord_Write(datetime, dose_uSv);
                                if (wr != 0) {
                                    printf("[CMD] 写入失败 record %d err=%d\r\n", i, wr);
                                    return;
                                }
                            }
                        }
                    }
                    printf("[CMD] Simulated %d records. Total: %u\r\n",
                            n, HistRecord_GetValidCount());
                    return;
                }
                else if(!strcasecmp(gstr[1], "read"))
                {
                    int index = atoi(gstr[2]);
                    printf("[CMD] Reading 5-min record %d...\r\n", index);
                    if(HistRecord_Print((uint16_t)index) != 0)
                        printf("[CMD] Invalid index or no data\r\n");
                    return;
                }
                else
                {
                    int index = atoi(gstr[1]);
                    printf("[CMD] Reading 5-min record %d...\r\n", index);
                    if(HistRecord_Print((uint16_t)index) != 0)
                        printf("[CMD] Invalid index or no data\r\n");
                    return;
                }
            }
            /* 清空数据命令 */
            else if(!strcasecmp(gstr[0], "CLR"))
            {
                if (gstr[1][0] == '\0' || !strcasecmp(gstr[1], "end")) {
                    printf("[CMD] 用法：CLR,5min|all,end\r\n");
                    return;
                }
                Cmd_Clear_Data(gstr[1]);
                return;
            }
        }
        Uart_Part_Cmd_Tip();
    }
}


