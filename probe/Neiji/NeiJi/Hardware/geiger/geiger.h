#ifndef GEIGER_H
#define GEIGER_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "sys_cfg_defaults.h"

#ifndef GEIGER_HV_ENABLE
#define GEIGER_HV_ENABLE  1
#endif

/* 上电冷启动：HV 爬升期前 N ms 丢弃盖革计数，剂量率强制 0（仍正常 0x23 上报） */
#ifndef GEIGER_BOOT_BLANK_MS
#define GEIGER_BOOT_BLANK_MS  2000U
#endif

#define GEIGER_PIN_Pin GPIO_PIN_0
#define GEIGER_PIN_GPIO_Port GPIOA

#define SN_LEN                  DEVICE_CFG_SN_LEN
#define HW_VER_LEN              DEVICE_CFG_HW_VER_LEN
#define LONG_KET_TIME_BASE      25          // 按键长按时间
#define DAY_PER_SAVE_TIME       900000      // 当日数据保存一次的时间间隔，单位：ms

#define RATE_LIMIT              10000.0f     // 剂量率上限 1.00 mSv/h = 1000 uSv/h
#define DOSE_LIMIT              99999999.0f // 剂量值上限 99.99 Sv  = 99990000 uSv

/* 5 分钟剂量账本：10×30s 块累加 → D5（μSv）；与 reg1 EWMA 剂量率分离
 * DOSE_5MIN_TEST_FAST=1：测试加速（1s 块 / 5s 上报），量产务必改回 0 */
#ifndef DOSE_5MIN_TEST_FAST
#define DOSE_5MIN_TEST_FAST         0
#endif
#if DOSE_5MIN_TEST_FAST
#define DOSE_BLOCK_SEC              1U
#define DOSE_REPORT_INTERVAL_SEC    5U
/* 5s 本底窗太小，×100 会整段变 0；测试用 ×10000（0.0001μSv） */
#define DOSE_5MIN_PROTOCOL_SCALE    10000.0f
#else
#define DOSE_BLOCK_SEC              30U
#define DOSE_REPORT_INTERVAL_SEC    300U
#define DOSE_5MIN_PROTOCOL_SCALE    100.0f
#endif
#define DOSE_BLOCK_30S_MS           (DOSE_BLOCK_SEC * 1000U)
#define DOSE_REPORT_5MIN_MS         (DOSE_REPORT_INTERVAL_SEC * 1000U)

/*-----------数据采集相关-----------*/
typedef struct 
{
    uint8_t over_cnt;    // n次采集的总次数超过m个时，切换到高剂量模式
    uint8_t muti_cnt;    // 连续n次采集计数超过m个时，切换到高剂量模式
    uint8_t once_cnt;    // 单次采集计数超过m个时，切换到高剂量模式
    uint8_t keep_cnt;    // （处于高剂量模式时）一旦单次采集计数≥ xxx 个时，维持高剂量模式
}HD_Mode_Param;


#define SAMPLE_PERIOD           100  // 采集时间间隔，单位：ms
#define LONG_DEAL_TIME          30   // 低剂量（剂量率）计算时长 300 * 100 = 30000 ms = 30s
#define SHORT_DEAL_TIME         3    // 高剂量（剂量率）计算时长 30 * 100 = 3000 ms = 3s
#define HD_MD_EN_OVER_CNT       6    // n次采集的总次数超过m个时，切换到高剂量模式
#define HD_MD_EN_MULTI_CNT      2    // 连续n次采集到的盖革管计数≥3个时，切换到高剂量模式
#define HD_MD_EN_ONCE_CNT       4    // 高剂量模式使能计数（单次采集超过该计数，则使能）
#define HD_MD_EN_KEEP_CNT       2    // （处于高剂量模式时）一旦单次采集计数≥ xxx 个时，维持高剂量模式
#define HD_MD_TIMES             3    // 高剂量模式维持次数
/*-----------数据采集相关-----------*/

typedef struct Sys_Config
{
    float th_rl_rate;       // 实时剂量率低位报警阈值   单位：uSv/h、mSv/h
    float th_rh_rate;       // 实时剂量率高位报警阈值   单位：uSv/h、mSv/h
    float th_rh_rate_saved; // reg82 禁止上阈值时暂存（落 Flash）
    float th_rl_rate_saved; // reg82 禁止下阈值时暂存（落 Flash）
    uint8_t dose_th_shadow_flags; /* NET_DOSE_SHADOW_* 有效标志 */
    float sensitivity;      // 盖革管灵敏度(单位：cpm/uGy/h)

    float temp_th_hi;       // 温度上阈值（℃）
    float temp_th_lo;       // 温度下阈值（℃）
    float press_th_hi;      // 气压上阈值（hPa）
    float press_th_lo;      // 气压下阈值（hPa）
    float hum_th_hi;        // 湿度上阈值（%RH）
    float hum_th_lo;        // 湿度下阈值（%RH）
    uint32_t co2_th_hi;     // 二氧化碳上阈值（ppm，整型）
    uint32_t co2_th_lo;     // 二氧化碳下阈值（ppm）
    uint16_t pm25_th_hi;    // PM2.5 上阈值
    uint16_t pm25_th_lo;    // PM2.5 下阈值

    uint8_t alarm_sound;    // 声报警开关 0/1
    uint8_t alarm_light;    // 光报警开关 0/1
    uint8_t alarm_volume;   // 报警音量（百分比 0-100）
    uint8_t alarm_volume_saved; // 声报警关闭时暂存音量（落 Flash）
    uint8_t display_enable; // 显示屏开关 0/1
    uint8_t dev_addr;       // 设备地址（1 字节）
    uint8_t language;       // 语言设置 0=中文，1=英文
    
    uint32_t alarm_status;  // 报警状态标志位（默认值：0）

    char SN[SN_LEN + 1];             // 设备序列号（长度见 DEVICE_CFG_SN_LEN）
    char hw_version[HW_VER_LEN + 1]; // 硬件版本（长度见 DEVICE_CFG_HW_VER_LEN）
    float bright_sz;      // 亮度（百分比）
}Sys_Cfg_Struct;

typedef struct Data_Var
{
    // 数据变量（需要保存）
    // float crt_dose;             // 总累计剂量值                   单位：uSv、mSv、Sv
    // uint32_t clr_date;          // 总累计剂量值最近清除的日期（待更替变量入程序）   //日期格式：月日年，如：24年11月30日 -> 113024
    
    // uint32_t day_date;          // 当日日期    格式：年月日，如：21年02月14日 --> 210214
    // float day_dose;             // 当日累计剂量值                 单位：uSv、mSv、Sv
    // uint32_t day_acc_tk;        // 当日累计总时长
    // float day_top_rate;         // 当日最高剂量率                 单位：uSv/h、mSv/h、Sv/h
    
    // float main_dose;            // 开机后至关机累计的剂量值        单位：uSv、mSv、Sv（主界面的DOSE值）
    float dose_5min_acc;             // 当前 5min 窗累计剂量（μSv），由 10×D30 相加
    float dose_30s_acc;              // 当前 30s 窗累计剂量（μSv）

    // 数据变量（不保存）
    float real_rate;            // 实时剂量率                     单位：uSv/h、mSv/h、Sv/h

    uint32_t over_num;          // 累计计数器溢出次数
    uint32_t geiger_crt_cnt;    // 本次(实际)盖革管触发的信号次数
}Data_Var_Struct;



// extern volatile uint32_t over_num;
extern bool print_format;
extern float alpha_arge[];
extern bool one_second_cnt_func;
extern HD_Mode_Param hd_param;
extern Data_Var_Struct data_var;
extern volatile Sys_Cfg_Struct sys_cfg;

void Geiger_Init(void);
void Geiger_Doserate_Calculate(void);
void Real_Data_deal(void);
void Dose_Rate_TH_Alarm(void);

/* 报警状态更新辅助函数 */
void Alarm_Status_Update(uint8_t bit_pos, bool is_alarm);  // 更新指定位的报警状态
void Alarm_Status_Clear(void);                              // 清除所有报警状态
uint32_t Alarm_Status_Get(void);                            // 获取当前报警状态

#endif
