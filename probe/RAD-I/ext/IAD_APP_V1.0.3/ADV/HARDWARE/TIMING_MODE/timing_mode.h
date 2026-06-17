#ifndef __TIMING_MODE_
#define __TIMING_MODE_

#include "control.h"
#include <stdint.h>

#define AUTO_RESTART_TIME  360000  // 计时模式自动重启时长（单次最大累计时长）（单位: 秒）

typedef enum{
    TIMING_MODE_OFF,         // 0：关闭计时模式  
    TIMING_MODE_RUN,         // 1：打开计时模式
    TIMING_MODE_PAUSE,       // 2：暂停计时模式
//    TIMING_MODE_RETURN,      // 3：等待返回计时模式
}Timing_Mode;

// 计时记录的数据结构体(64bits = 8字节)
typedef struct    // 根据位段的空间分配规则
{
    uint32_t keep:1;        // 保留（1 bit）
    uint32_t date:17;       // 日期格式：月日年，如：24年11月30日 -> 113024（17 bits）
    uint32_t init_time:12;  // 时间格式：时分，如：23时59分 -> 2359（12 bits）
    uint32_t sum_unit:2;    // 数据单位(数据值满1000则单位进1)（2 bits）
    uint32_t sum_data:14;   // 数据（原数据乘以100，16位无符号整型）（14 bits）
    uint32_t t_unit:4;      // 累计时间进制(满10000则单位进1)（4 bits）
    uint32_t t_time:14;     // 累计时间（单位：秒）（14 bits）
//    struct{
//        uint32_t fg;
//    }tt;
}Timing_Mode_His_Struct;

// 计时模式的控制结构体
typedef struct
{
    float dose;          //计时模式的累计剂量值           单位：uSv、mSv、Sv
    uint32_t s_tk;       //计时模式每次检测时的“秒”
    uint32_t a_tk;       //计时模式总累计时长（单位：秒）
	uint8_t mode;        //计时模式              0：关闭计时  1：打开计时  2：暂停计时		  3：等待返回计时
//	uint8_t mode:2;      //计时模式              0：关闭计时  1：打开计时  2：暂停计时		  3：等待返回计时
}Timing_Mode_Ctr;

extern Timing_Mode_His_Struct timing_his;
extern Timing_Mode_Ctr timing_ctr;

void Enter_Timing_Mode(void);
void Timing_Show_Time(void);
void Timing_SW(void);
void Back_Timing_Mode(void);
void Timing_Mode_Restart(void);
void Exit_Timing_Mode(void);
void Timing_Mode_Save_EEPROM(void);
void Timing_Mode_Time_Up(void);
void Timing_Mode_Data_Save(void);
void Enter_Timing_Mode_History(void);
void Exit_Timing_Mode_History(void);
#endif

