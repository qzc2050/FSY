#ifndef __SYS_CTR_H
#define __SYS_CTR_H

#include <stdint.h>
#include <stdbool.h>

#include "main.h"

#define BAT_LINK()          HAL_GPIO_WritePin(KEY_ON_GPIO_Port,KEY_ON_Pin,GPIO_PIN_SET)
#define BAT_UNLINK()        HAL_GPIO_WritePin(KEY_ON_GPIO_Port,KEY_ON_Pin,GPIO_PIN_RESET)

#define SN_LEN              11          // 此处应为4的倍数减1

#define DEVICE_TYPE         0x01        //类型：0x1 个人型，0x2 专业型，0x3 半导体型
#define SOFTWARE_VERSION    0x01250423  //例如：0x01250107 --> 01代25年01月07日

//#define GEIGER_SENSITY      0.66666667  //盖革管灵敏度


#define UDATA_DEF           -1.0f       //沿用udata联合体的参数

/****************************数据存储地址列表****************************/

/*----------------------------系统配置结构体----------------------------*/
//此处开始 A
#define TH_REAL_RATE_ADDR   0x080807B8      //(2)实时剂量率阈值的地址
//#define TH_DAY_DOSE_ADDR    0x080807BA      //(2)当日剂量阈值的地址
#define TH_CRT_DOSE_ADDR    0x080807BC      //(2)总剂量阈值的地址
//#define SENSIVITY_ADDR    0x080807BE      //(2)灵敏度的地址
#define SN_ADDR             0x080807C0      //(12)序列号存储地址（占用SN_LEN + 1个字节）

#define PW_TK_ADDR          0x080807CC      //(2)电池功耗（测试）持续时间的地址
//#define BRIGHT_SZ_ADDR      0x08080726      //(4b)亮度等级的地址
//#define SCR_OFF_IDX_ADDR    0x08080726      //(4b)熄屏时间的地址
//#define REC_CIR_ADDR        0x08080726      //(1b)数据记录完一轮的标志的地址
//#define TIMING_OFS_ADDR     0x08080726      //(1b)计时模式数据保存偏移
//#define SD_FUNC_ADDR        0x08080726      //(1b)自动关机功能
//#define KEEP_BITS           0x08080726      //(5b)
//!!!!!!!!!!!!!!!!!!!!!!!!!!A截止目前共4*6字节
/*----------------------------系统配置结构体----------------------------*/


/*----------------------------数据结构体----------------------------*/
#define HIS_DATA_SIZE       8             //单组历史记录大小
#define DATA_BASE_ADDR      0x08080000    //(2000 = 250*8字节)数据保存的初始地址
#define ALL_DATA_NUM        247           //DATAEEPROM保存历史记录组数（一轮数据有250组）
#define DATA_MAX_ADDR       DATA_BASE_ADDR + (ALL_DATA_NUM - 1) * HIS_DATA_SIZE    //最后一组数据的保存地址（非DATAEEPROM的尾地址）

//此处开始 B
#define TIMING_DATA_ADDR    0x080807D0    //(16)计时模式数据保存基地址  占用 2组 * 8字节
#define HISTORY_NUM_ADDR    0x080807E0    //(2)已保存历史记录数据个数的地址
//#define COVER_DATA_NUM_ADDR 0x080807E2    //(2)覆盖保存到DATAEEPROM的数据个数（即当前数据保存在DATAEEPROM的基地址的偏移位置）
#define CRT_DOSE_ADDR       0x080807E4    //(4)总累计剂量的地址
#define CLR_DATE_ADDR       0x080807E8    //(4)清除当前累计剂量值之后重新累计的日期的地址

#define DAY_DATE_ADDR       0x080807EC    //(4)当日日期的地址
#define DAY_DOSE_ADDR       0x080807F0    //(4)当日累计剂量值的地址
#define DAY_ACC_TIME_ADDR   0x080807F4    //(4)当日设备累计启动时间的地址
#define DAY_TOP_RATE_ADDR   0x080807F8    //(4\2)当日最高剂量率的地址

//!!!!!!!!!!!!!!!!!!!!!!!!!!B 截止目前共4*(4+7)字节
/*----------------------------数据结构体----------------------------*/



// 独立出来
#define FIRST_USE_STA_ADDR  0x080807FC    //(4\2)判断是否为初次使用，进行出厂初始化设置


/****************************数据存储地址列表****************************/
enum{
    UNIT_USV_H,         // uSv或uSv/h单位
    UNIT_MSV_H,         // mSv或mSv/h单位
    UNIT_SV_H,          // uSv或Sv/h单位
};

enum{
    DOSE_TYPE,
    DAY_TYPE,
};

enum{
    CRT_DOSE_SW,
    DAY_DOSE_SW,
    RATE_SW,
};

typedef struct
{
    uint16_t unit:2;    // 高2位单位(数据值满1000则单位进1)
    uint16_t data:14;   // 低14位数据（原数据乘以100，16位无符号整型）
}Data_Struct;           //! 14bits = 16384(即数据大小应小于该值，否则请将单位进1)

typedef struct
{
    uint8_t type:1;     // 1：每日累计数据  0：当前累计数据
    uint32_t date:17;   // 日期格式：月日年，如：24年11月30日 -> 113024
}Date_Struct;

//typedef union    // 根据位段的空间分配规则，暂时无法将历史记录的数据通过位段方式压缩至2字节
//{
//    struct{
//        Date_Struct rec;    // 18bits
//        uint16_t keep:14;   // 14bits
//        Data_Struct sum;    // 16bits
//        Data_Struct rate;   // 16bits
//    }day;    // 每日累计记录(64bits = 8字节)
//    struct{
//        Date_Struct init;   // 18bits
//        uint16_t keep:12;   // 12bits
//        Data_Struct sum;    // 16bits
//        Date_Struct clr;    // 18bits
//    }dose;   // 总累计记录(64bits = 8字节)
//}Dev_Data_Union;

typedef union    // 根据位段的空间分配规则，暂时无法将历史记录的数据通过位段方式压缩至2字节
{
    struct{
        uint8_t rec_type:1;     // 1：每日累计数据  0：当前累计数据（1 bits）
        uint32_t rec_date:17;   // 日期格式：月日年，如：24年11月30日 -> 113024（17 bits）
        uint16_t keep:14;       // 保留（14 bits）
        uint16_t sum_unit:2;    // 数据单位(数据值满1000则单位进1)（2 bits）
        uint16_t sum_data:14;   // 数据（原数据乘以100，16位无符号整型）（14 bits）
        uint16_t rate_unit:2;   // 数据单位(数据值满1000则单位进1)（2 bits）
        uint16_t rate_data:14;  // 数据（原数据乘以100，16位无符号整型）（14 bits）
    }day;    // 每日累计记录(64bits = 8字节)
    struct{
        uint8_t init_type:1;    // 1：每日累计数据  0：当前累计数据（1 bits）
        uint32_t init_date:17;  // 日期格式：月日年，如：24年11月30日 -> 113024（17 bits）
        uint16_t keep:12;       // 保留（12 bits）
        uint8_t sum_unit:2;     // 数据单位(数据值满1000则单位进1)（2 bits）
        uint32_t sum_data:14;   // 数据（原数据乘以100，16位无符号整型）（14 bits）
        uint8_t clr_type:1;     // 1：每日累计数据  0：当前累计数据（1 bits）
        uint32_t clr_date:17;   // 日期格式：月日年，如：24年11月30日 -> 113024（17 bits）
    }dose;   // 总累计记录(64bits = 8字节)
}Dev_Data_Union;

typedef union
{
    char u8_SN[SN_LEN + 1]; // 设备序列号
    uint32_t u32_SN[(SN_LEN + 1) / 4];
}Dev_SN_Union;


typedef struct Sys_Config
{
    // 其他变量（暂放）（需要保存）
    Data_Struct th_real_rate;   // 实时剂量率报警阈值   单位：uSv/h、mSv/h、Sv/h(低14位数据，高2位单位)
    Data_Struct th_day_dose;    // 当日累计剂量值的阈值 单位：uSv、mSv、Sv(低14位数据，高2位单位)
    Data_Struct th_crt_dose;    // 当前累计剂量值的阈值 单位：uSv、mSv、Sv(低14位数据，高2位单位)
    Data_Struct sensitivity;    // 盖革管灵敏度(单位：cpm/uGy/h)
    
    Dev_SN_Union dev;           // 设备序列号

    uint16_t power_tk;      // 功耗测试累计时间
    uint8_t bright_sz:4;    // 亮度等级（索引值：0-8）
    uint8_t scr_off_idx:4;  // 熄屏时间索引（索引值：0-8）
	uint8_t rec_cir:1;      // 记录一轮数据的标志        0：第一轮数据记录  1：过完一轮数据记录
    uint8_t timing_ofs:1;   // 计时模式数据保存偏移      0：第一组          1：第二组
	uint8_t sd_func:1;      // 自动关机功能控制    	    0：开启自动关机    1：关闭自动关机
    uint8_t keep:5;         // 保留位（与上方凑齐32位）
    
    // 6*4字节
}Sys_Cfg_Struct;

typedef struct KEY_Ctr
{
    uint8_t sd_tk;          // 长按关机总计时(shutdown)
    bool muti_long;         // 多次长按触发
    uint16_t sd_cd_tk;      // 开关机按键长按显示关机界面后，再松开的累计时间
    uint32_t up_tk;         // 按键无操作时间
}KEY_Ctr_Struct;

typedef struct Data_Var
{
    // 数据变量（需要保存）
    uint16_t history_data_num;  // 当前保存到DATAEEPROM的历史记录数据个数
    uint16_t data_ofs_num;      // 覆盖保存到DATAEEPROM的历史记录数据个数（当前数据保存在DATAEEPROM的基地址的偏移位置）
    float crt_dose;             // 总累计剂量值                   单位：uSv、mSv、Sv
    uint32_t clr_date;          // 总累计剂量值最近清除的日期（待更替变量入程序）   //日期格式：月日年，如：24年11月30日 -> 113024
    
    uint32_t day_date;          // 当日日期    格式：年月日，如：21年02月14日 --> 210214
    float day_dose;             // 当日累计剂量值                 单位：uSv、mSv、Sv
    uint32_t day_acc_tk;        // 当日累计总时长
    float day_top_rate;         // 当日最高剂量率                 单位：uSv/h、mSv/h、Sv/h
    
    float main_dose;            // 开机后至关机累计的剂量值       单位：uSv、mSv、Sv（主界面的DOSE值）

    // 数据变量（不保存）
    float real_rate;            // 实时剂量率                     单位：uSv/h、mSv/h、Sv/h

    // 其他变量（暂放）
    uint16_t rec_rg_offset;     // 读取记录时(限定范围)，数据的偏移位数（即第n-1的位置）
    uint16_t over_num;          // 累计计数器溢出次数
    uint32_t geiger_crt_cnt;    // 本次(实际)盖革管触发的信号次数
    uint8_t crt_page;           // 历史记录当前页面
    uint8_t rec_rg_valid_page;  // 读取记录时(限定范围)，符合显示条件的页数
}Data_Var_Struct;


typedef struct Sys_Bits
{
    /******** 不需要保存 ********/
    
    // 运行相关
	uint8_t run_md:2;           // 正常运行模式标志位
	uint8_t power_sta:1;        // 判断是否处于开机状态   1：待开机    0：已开机
	uint8_t sd_req:1;           // 关机请求               1：请求关机  0：正常运行
    // 按键相关
	uint8_t key_ls:1;           // 按键长按状态           1：长按状态  0：非长按
 
    // 历史记录相关
	uint8_t his_tip:2;          // 历史记录非阻塞延时标志  0：关延时  1：标记起始计时时间   2：计时完毕
	uint8_t rec_rg_prep:1;      // 每次查看历史记录时，判断是否已筛选出符合条件的数据       1：已筛选  0：未筛选
	
    // 测试相关
//    uint8_t buz_md:1;           // 蜂鸣器测试功能        0：关闭       1：打开
	uint8_t aging_md:2;         // 老化测试标志位        0：关闭测试   1：正常运行3小时      2：低功耗3小时
//    uint8_t keep:1;             // 保留（凑足32位）
}Sys_Bits_Struct;

extern __IO Sys_Cfg_Struct sys_cfg;
extern __IO KEY_Ctr_Struct key_ctr;
extern __IO Data_Var_Struct data_var;
extern __IO Sys_Bits_Struct sys_bits;

extern __IO Dev_Data_Union udata;

uint32_t Date_Conv(uint32_t conv_date);
uint32_t Date_Inconvert(uint32_t conv_date);
void Save_Sys_Config(void);
void Get_SN(uint32_t *des_SN);
void Set_SN(char *str);
float DataUnit_To_Float(Data_Struct conv_st);
void Float_To_DataUnit(float data,uint8_t data_type);



extern void System_Time_Init(__IO uint32_t *time_tick);
extern bool System_Time_Wait(uint32_t time, uint32_t time_tick);
#endif



