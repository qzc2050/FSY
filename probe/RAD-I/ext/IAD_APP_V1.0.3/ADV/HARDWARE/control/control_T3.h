#ifndef __CONTROL_H
#define __CONTROL_H

#include "stm32l0xx_hal.h"

#include "tim.h"
#include "key.h"
#include "cmd.h"
#include "oled.h"
#include "main.h"
#include "usart.h"
#include "myiic.h"
#include "buzzer.h"
#include "pcf8563.h"
#include "max17048.h"
#include "oledfont.h"
#include "oled_data.h"
#include "stm_flash.h"
#include "muti_menu.h"
#include "timing_mode.h"
#include "battery_ctr.h"
#include "oled_set_val.h"
//#include "measure_test.h"
#include "oled_rtc_time.h"
#include "low_power_run.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define KEY_DEBUG  0
#if KEY_DEBUG
#define KEY_DEBUG_PRINTF(n)  printf(n)
#else
#define KEY_DEBUG_PRINTF(n)  
#endif


#define DEVICE_TYPE    0x01    //类型：0x1 个人型，0x2 专业型，0x3 半导体型
#define SOFTWARE_VERSION   0x01180B05    //例如：01170B17 --> 01代23年11月23日

//#define  DATA_EEPROM_SIZE   ((DATA_EEPROM_END - DATA_EEPROM_BASE + 1) / 4)    //可存放单个数据的个数（指可存放4个字节的数据的个数）

#define SN_LEN     11

#define DATAEEPROM_WriteAddress    		 0x08080000    //写在内部的Data EEPROM，防止破坏程序
#define DATAEEPROM_ReadAddress     		 DATAEEPROM_WriteAddress
//#define DATAEEPROM_TESTSIZE        		 20 * 3        //实际是DATAEEPROM_TESTSIZE*4个字节

#define DATA_WRITE_BASE_ADDR           0x08080000    //数据保存的初始地址(最后一组数据位置0x08080708)


//#define POWER_OFF_VOL_ADDR4            0x08080720    //报警关机电压保存（已关闭蜂鸣器）（ADC采集）
//#define POWER_OFF_VOL_ADDR3            0x08080724    //报警关机电压保存（未关闭蜂鸣器）（ADC采集）
//#define POWER_OFF_VOL_ADDR2            0x08080728    //报警关机电压保存（已关闭蜂鸣器）
//#define POWER_OFF_VOL_ADDR             0x0808072C    //报警关机电压保存（未关闭蜂鸣器）
#define EID_ADDR                       0x08080730    //电子身份证标识码（占用4个字节）
#define SN_ADDR           						 0x08080734    //序列号存储地址（占用44个字节）
#define FIRST_DAY_DATE_ADDR            0x08080760    //（首次使用设备，当日累计的日期）当日日期的地址
//#define PRODUCTION_DATE_ADDR           0x08080760    //出厂日期存储地址
//#define PRODUCTION_TIME_ADDR           0x08080764    //出厂时间存储地址
#define TIMING_MODE_SAVE_OFF_ADDR		   0x08080768    //计时模式数据保存偏移
#define TIMING_MODE_DATA_BASE_ADDR		 0x0808076C    //计时模式数据保存基地址  占用24字节
//#define BUZZER_GRADE_ADDR              0x08080784    //蜂鸣器音量等级的地址    
//#define LPR_REAL_RATE_GROUP_ADDR       0x0808078C    /*（低功耗模式）实时剂量率采集的总组数的地址，此时计算时间为(组数*data_var.sample_period)ms*/
#define FIRST_USE_STA_ADDR             0x08080790    //判断是否为初次使用，进行出厂初始化设置
//#define SWITCH_BOUND_ADDR              0x08080794    //切换低剂量/高剂量测量模式界限值的地址
//#define REAL_RATE_GROUP_ADDR           0x08080798    /*（有辐射源）实时剂量率采集的总组数的地址，此时计算时间为(组数*data_var.sample_period)ms*/
//#define SAMPLE_PERIOD_ADDR             0x0808079C    //盖革管触发信号采集周期的地址
#define SC_EXTINCT_TIME_ADDR           0x080807B0    //熄屏时间的地址
#define BRIGHT_GRADE_ADDR              0x080807B4    //亮度等级的地址
#define DAY_DATE_ADDR                  0x080807B8    //当日日期的地址
#define DAY_TIME_ADDR                  0x080807BC    //当日剂量值累计时间的地址
#define DAY_ALL_DOSE_ADDR              0x080807C0    //当日累计剂量值的地址
#define DAY_TOP_RATE_ADDR              0x080807C4    //当日最高剂量率的地址
#define DAY_ACC_ALL_TIME_ADDR          0x080807C8    //当日设备累计启动时间的地址
#define CURRENT_ALL_DOSE_ADDR          0x080807CC    //总累计剂量的地址

#define HISTORY_DATA_NUM_ADDR          0x080807D0    //已保存历史记录数据个数的地址
#define CLR_CUR_DOSE_INIT_DATE_ADDR    0x080807D4    //清除当前累计剂量值之后重新累计的日期的地址
#define COVER_SAVE_DATA_NUM_ADDR       0x080807D8    //覆盖保存到DATAEEPROM的数据个数（也指当前数据保存在DATAEEPROM的基地址的偏移位置）------------------------------->>
#define RECODE_ONE_ROUND_ADDR          0x080807DC    //数据记录完一轮的标志的地址

#define LOW_BAT_FUN_ADDR               0x080807E0    //自动关机功能
#define USER_GM_EOCF_ADDR              0x080807E4    //灵敏度的校正系数地址
#define POWER_CONSUME_TIME_ADDR        0x080807E8    //电池功耗（测试）持续时间的地址

//#define TH_AVER_RATE_ADDR              0x080807EC    //平均剂量率阈值的地址
#define TH_DOSE_RATE_ADDR              0x080807F0    //实时剂量率阈值的地址
//#define TH_DOSE_VAL_ADDR               0x080807F4    //界面DOSE剂量阈值的地址
#define TH_TODAY_ALL_DOSE_ADDR         0x080807F8    //当日剂量阈值的地址
#define TH_CURRENT_ALL_DOSE_ADDR       0x080807FC    //总剂量阈值的地址

#define LONG_KET_TIME_BASE             25            //按键长按时间

#define DATAEEPROM_SAVE_DATA_NUM       150           //DATAEEPROM保存历史记录组数（一轮数据有150组）
#define DATA_WRITE_MAX_ADDR            DATA_WRITE_BASE_ADDR + (DATAEEPROM_SAVE_DATA_NUM - 1) * 12    //最后一组数据的保存地址（非DATAEEPROM的尾地址）

#define GEIGER_TUBE_SENSITY            0.66666667   //盖革管灵敏度
#define SWITCH_BOUND_VAL               (10 / GEIGER_TUBE_SENSITY)  //切换高剂量模式的界限值，即按照单次100ms采样到盖革管计到1次计数来计算得到的剂量率

#define POWER_DETECT_TIME          		 1      //检测电量的时间间隔，单位：秒
//#define POWER_DETECT_TIME          		 180    //测试，检测电量的时间间隔，单位：秒
//#define LPR_POWER_DETECT_TIME          8      //测试，（低功耗模式下）检测电量的时间间隔，单位：秒
#define LPR_POWER_DETECT_TIME          300    //（低功耗模式下）检测电量的时间间隔，单位：秒

#define LPR_LED_TWINKLE_TIME           5      //LED闪烁时间,单位：秒
#define LPR_LED_TWINKLE_TIME_CNT       LPR_LED_TWINKLE_TIME*1000

//#define DAY_PER_SAVE_TIME              40     //当日数据保存一次的时间间隔，单位：秒
#define DAY_PER_SAVE_TIME              300    //当日数据保存一次的时间间隔，单位：秒
#define DAY_PER_SAVE_TIME_CNT          DAY_PER_SAVE_TIME * 1000

#define READ_USB        HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_12) //读取USB接口状态

#define ONE_GRID_LED_TWINKLE_TIME      (2 * 1000)  //当电量仅剩一格电的时候，状态灯2s闪烁一次
#define LOW_BAT_VOL                    3100        //低电量时的电池电压
//#define LOW_BAT_BUZ_TIME               5000        //测试，5s一次低电量报警
#define LOW_BAT_BUZ_TIME               180000      //3分钟一次低电量报警，再1分钟之后关机

#define RATE_LIMIT      10000             //剂量率上限 10.00 mSv/h = 10000 uSv/h
#define DOSE_LIMIT      99.99*1000000     //剂量值上限   99.99 Sv  = 99990000 uSv


#define BEEP_DUTY       625
#define BEEP_ON()       __HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3,BEEP_DUTY)
#define BEEP_OFF()      __HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3,0);      //关闭蜂鸣器
#define OVER_TH_CTR()   TIM_Var.Key_Inactive_time = 0,Sys_sta.ex_th_rate = 2

#define ALARM_LED_ON()     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
#define ALARM_LED_OFF()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);

#define BLUE_LED_OFF()     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET)
#define BLUE_LED_ON()      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET)

/*-----------数据采集相关-----------*/
#define SAMPLE_PERIOD   					100  //采集时间间隔，单位：ms
#define LONG_DEAL_TIME  					30   //低剂量（剂量率）计算时长 300 * 100 = 30000 ms = 30s
#define SHORT_DEAL_TIME  					3    //高剂量（剂量率）计算时长 30 * 100 = 3000 ms = 3s
#define SWITCH_LONG_SHORT_CNT 		3    //当1s内采集到的（盖革管）计数为3个时，切换到高剂量计算时长
/*-----------数据采集相关-----------*/


#define  __CTR_EXTERN  extern

typedef enum 
{ 
  AVG = 0,    //平均值
  REAL,       //实时值
}Cal_mode;

//typedef enum 
//{ 
//  CPS = 0,    //每秒次数
//  CPM,        //每分次数
//}Cal_uint;

typedef enum 
{ 
  POWER_ON = 0,    //开机
  POWER_OFF,       //关机
}Power_sta;

typedef enum 
{ 
  ALARM_BRIGHT = 0,    //报警亮屏
  KEY_BRIGHT,          //按键亮屏
}Sys_Run_sta;

typedef enum 
{ 
	AGING_OFF = 0,         //关闭老化测试
  KEEPING_BRIGHT,        //保持亮屏
  KEEPING_LPR,           //低功耗
}Aging_sta;

typedef enum 
{ 
  KEEPING_POWER_ON = 0,    //保持开机
  REQUEST_POWER_OFF,       //请求关机
}Request_sta;

typedef enum 
{ 
  FAILED = 0,     //测试失败
  PASSED,         //测试通过
}TestStatus;

typedef enum 
{ 
  LPR_MODE = 0,    //低功耗运行模式
  RUN_MODE,        //正常运行模式	
}Sys_Mode;

typedef struct TIM_Opration
{
	 uint8_t  Key_Time;               //按键时间
	 uint8_t  Power_off_Time;         //长按关机总计时
	 uint16_t add_time;               //累计时间，达到设定数值则开始采样并清零
   uint16_t beep_time;              //蜂鸣器鸣叫时间
	 uint16_t Refresh_time;           //定时刷新显示屏
	 uint16_t get_power_time;         //电量检测总计时
	 uint16_t Key_Power_OFF;          //开关机按键长按显示关机界面后，再松开的累计时间
	 uint32_t low_bat_alarm_time;     //(电量不足)报警时间间隔
	 uint32_t Key_Inactive_time;      //按键无操作时间
	 uint32_t startup_time;           //启动时长
	 uint32_t day_all_time;           //当日累计总时长
//	 uint32_t Production_date;        //出产日期
//	 uint32_t Production_time;        //出产时间
	 uint32_t timing_mode_second;     //计时模式每次检测时的“秒”
	 uint32_t timing_mode_add_time;   //计时模式累计时长
	 uint32_t first_day_date;         //初始累计日期，用于校验
} TIM_Opration_st, *TIM_Opration_pst;

typedef struct Data_var
{
	 float user_def_coef;     		//盖革管灵敏度校正系数         盖革管灵敏度 = 0.75 cps/uGy/h * user_def_coef
	 float dose_rate;        		  //实时剂量率             	     单位：uSv/h、mSv/h、Sv/h
	 float prev_dose_rate;        //(低剂量且≠0的)实时剂量率  	 单位：uSv/h、mSv/h、Sv/h
//	 float Aver_dose_rate;        //(开机以来)平均剂量率   	     单位：uSv/h、mSv/h、Sv/h
	 float main_dose;             //开机后至关机累计的剂量值     单位：uSv、mSv、Sv（主界面的DOSE值）
	 float day_all_dose;      	  //当日累计剂量值         			 单位：uSv、mSv、Sv
	 float Current_all_dose;			//总累计剂量值      	 	       单位：uSv、mSv、Sv
	 float timing_mode_dose;      //计时模式的累计剂量值				 单位：uSv、mSv、Sv
	 float day_top_rate;          //当日最高剂量率         			 单位：uSv/h、mSv/h、Sv/h
//	 float switch_bound;          //低/高剂量测量模式切换界限值  单位：uSv/h、mSv/h、Sv/h
	 float Th_dose_rate;          //实时剂量率报警阈值     			 单位：uSv/h、mSv/h、Sv/h
//	 float Th_aver_rate;          //平均剂量率报警阈值    			 单位：uSv/h、mSv/h、Sv/h
//	 float Th_main_dose;          //主界面的DOSE值的阈值         单位：uSv、mSv、Sv
	 float Th_day_all_dose;       //当日累计剂量值的阈值  			 单位：uSv、mSv、Sv
	 float Th_current_all_dose;   //总累计剂量值的阈值  			   单位：uSv、mSv、Sv
	 uint8_t day_top_unit;        //当日最高剂量率的单位         单位：uSv/h、mSv/h、Sv/h
	 uint8_t current_page;        //历史记录当前页面
	 uint8_t rec_rg_valid_page;   //读取记录时(限定范围)，符合显示条件的页数
	 uint8_t Rec_RG_real_cnt;     //筛选后，实际有效历史记录数据个数
	 uint16_t BG_time;            //低剂量（不超过剂量率阈值）时，最新的信号触发时间间隔
	 uint16_t trigger_times;      //触发时间间隔
	 uint16_t last_cnt;       		//计数器上次读取时的值
	 uint16_t present_cnt;        //计数器当前读取时的值
	 uint16_t rec_rg_offset;      //读取记录时(限定范围)，数据的偏移位数（即第n-1的位置）
	 uint16_t over_num;      		  //累计计数器溢出次数
	 uint16_t save_over_num;      //保存计数器溢出次数
	 uint32_t day_date;           //当日日期    格式：年月日，如：21年02月14日 --> 210214
	 uint32_t day_time;           //当日时间    格式：时分，  如：14时30分 --> 1430
//	 uint32_t rate_group;         //设定正常模式下(高剂量模式)需要计算多少个100ms内的平均剂量率
//	 uint32_t lpr_rate_group;     //设定低功耗模式下需要计算多少个100ms内的平均剂量率
//	 uint32_t sample_period;      //盖革管触发信号采集周期    单位：ms
	 uint32_t Bright_grade;       //亮度等级
//	 uint32_t Buzzer_grade;       //蜂鸣器音量等级
	 uint32_t history_data_num;     //当前保存到DATAEEPROM的历史记录数据个数
	 uint32_t data_offset_num;    //覆盖保存到DATAEEPROM的历史记录数据个数（当前数据保存在DATAEEPROM的基地址的偏移位置）
	 uint32_t prev_GM_cnt;   		  //上次盖革管触发的信号次数(低剂量且≠0)
	 uint32_t present_GM_cnt;     //本次(实际)盖革管触发的信号次数
} Data_var_st, *Data_var_pst;

typedef struct Opration_Sta
{
	uint8_t run_sta:1;           		 //正常运行模式标志位
	uint8_t cal_mode:1;         	   //数据类型切换（实时值/平均值）
//	uint8_t cal_uint:1;              //单位切换（CPS/CPM）
	
	uint8_t refresh_sta:1;       		 //设置界面，使选中的数字闪烁        1：开启   0：关闭
	uint8_t light_sta:1;   	     	   //修改阈值时，确保切换到其他数字之后，其余数字在屏幕上显示
	uint8_t time_light_sta:1;        //修改时间时，确保切换到其他数字之后，其余数字在屏幕上显示
	
	uint8_t updata_sta:1;        		 //数据更新标志，时间间隔为 SAMPLE_PERIOD
	uint8_t warning_sta:1;       		 //警报标志        		  1：请求显示  0：显示完毕/未请求
	
	uint8_t rec_s_m_keylong:1;   		 //记录按键长按状态     1：长按状态  0：非长按
	uint8_t usb_power_on_sta:1;  		 //USB接入状态          1：接入      0：未接入
	
	uint8_t power_sta:1;             //判断是否处于开机状态    1：待开机   0：已开机
	uint8_t power_off_sta:1;     		 //关机请求        		  1：请求关机  0：正常运行
	uint8_t auto_off_func:1;     		 //自动关机功能控制    	0：关闭自动关机    1：打开自动关机
	
	uint8_t low_bat:1;           		 //主要记录是否显示低电量的图标以及低电量报警           1：显示    0：不显示
	uint8_t time_alarm_sta:1;    		 //低电量定时报警  		  1：开始计时  0：非低电量状态/充电状态(停止计时)
	uint8_t low_power_off_sta:1; 		 //低电量下蜂鸣器报警1次，然后3分钟后自动关机
	uint8_t clr_buz_times_sta:1; 		 //蜂鸣器报警次数清零  	0：不清零    1：清零
	uint8_t low_bat_alert_sta:2;  	 //低电量警报          	0：无警报    1：警报     2：已报警（按照电量情况，判断之后是否报警）
	uint8_t bat_one_grid_sta:1;      //一格电量标志位       0：电量两格及以上    1：电量仅剩一格
	
	uint8_t timing_mode_save_sta:1;	 //计时模式数据保存状态	0：未保存    1：已保存
	uint8_t timing_mode_sta:2;       //计时模式             0：关闭计时模式     1：打开计时模式     2：暂停计时模式		3：等待返回计时模式
	uint8_t timing_mode_p_sta:2;     //计时模式进入/退出（报警）反馈     0：无操作    1：进入/退出反馈    2：反馈完毕
	
	uint8_t ex_th_rate:2;         	 //当前剂量率的数值范围      0：小于阈值    1：超过阈值  2：报警完成  3：达到/超过极限值（10.00 mSv/h）
	uint8_t refresh_rate_sta:2;      //实时剂量率数值刷新标志    0：无需再延时  1：请求延时    2：准备延时 
	
	uint8_t history_tip_sta:2;       //历史记录非阻塞延时标志    0：关闭延时    1：标记起始计时时间   2：计时完毕
	uint8_t rec_rg_prep:1;       		 //每次查看历史记录时，判断是否已筛选出符合条件的数据   1：已筛选  0：未筛选
	uint8_t buz_test_sta:1;	         //蜂鸣器测试功能       0：关闭      1：打开
//	uint8_t volume_demo_sta:2;       //音量演示状态            0：无演示      1：请求演示    2：演示完毕
	
	uint32_t timing_mode_save_off;	 //计时模式数据保存偏移	     0：第一组      1：第二组
	uint32_t sc_extinct_time;        //熄屏时间索引
	uint32_t recode_one_round;       //记录一轮数据的标志        0：第一轮数据记录    1：过完一轮数据记录 

	uint8_t aging_test_sta:2;        //老化测试标志位       0：关闭测试    1：正常运行3小时    2：低功耗3小时
}Opration_Sta_st, *Opration_Sta_pst;


__CTR_EXTERN int unit_sta;
__CTR_EXTERN int aver_unit_sta;
__CTR_EXTERN char str_dose_rate[];
__CTR_EXTERN char str_ave_dose_rate[];
__CTR_EXTERN uint8_t index_val;
__CTR_EXTERN uint8_t last_index_val;
__CTR_EXTERN uint32_t power_continue_cnt;
__CTR_EXTERN __IO TIM_Opration_st   TIM_Var;
__CTR_EXTERN __IO Opration_Sta_st   Sys_sta;
__CTR_EXTERN __IO Data_var_st       data_var;

TestStatus Buffercmp(int sta);
uint8_t uint_deal(char *str,float dose_rate);
uint32_t abtain_deal_cnt(uint8_t crt_pos_num,uint32_t group,uint32_t *cnt_buf);
void Real_Data_deal(void);
void Sum_Dose(void);
void Get_tim_cnt(void);
void Data_Cal(void);
void Exit_LPR(uint8_t sta);
void LPR_Data_Cal(void);
void Data_Save_Cnt_Add(void);
void Flash_Save_Day_Data(void);
void Update_DayData_To_EEPROM(uint8_t sta);
void Flash_Save_Data(void);
void Time_Operate(void);
int Init_Setup(void);
uint8_t Detect_first_use(void);
void checkout_sys_init(void);
void Get_SN(uint32_t *des_SN);
void Set_SN(char *des_SN,char *str);
void Decrease_Offset_Position(void);
void Increase_Offset_Position(void);
int16_t Adjust_Offset_Position(uint32_t date);
void Flash_Save_Test(void);
void Aging_Test(void);
#endif

