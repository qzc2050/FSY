#ifndef __UI_MENU_H
#define __UI_MENU_H

#include <stdint.h>


// 定义按键编号
enum {
    CTR_IDX,    // 菜单索引
    CTR_PC,     // 开关键/确认键
    CTR_L,      // 左键
    CTR_R       // 右键
};

// 定义菜单层级
enum{
    DEPTH_HOME_1,
    DEPTH_HOME_2,
    DEPTH_MENU_HOME,
    DEPTH_MENU_TH,
    DEPTH_MENU_TH_DOSE,
    DEPTH_MENU_SYS,
    DEPTH_MENU_DISPLAY,
    DEPTH_SET_BAR,
    DEPTH_SYS_INFO_1,
    DEPTH_SYS_INFO_2,
    DEPTH_SYS_INFO_3,
    DEPTH_SET_RTH,
    DEPTH_SET_DAYTH,
    DEPTH_SET_CRTTH,
    DEPTH_SET_TIME,
    DEPTH_VAL_LIMIT,
    DEPTH_HISTORY,
    DEPTH_CLR_DAY,
    DEPTH_CLR_CRT,
    DEPTH_MENU_RST,
    DEPTH_TIMING_HIS,
    DEPTH_POWEROFF,
};

// 菜单索引
enum{
    // 主界面
    MENU_HOME_1,    //1 主界面（第一界面）
    MENU_HOME_2,    //2 剂量累计界面（第二界面）
    
    // 一级菜单（主菜单）
    MENU_0_BACK,    //3 返回主界面
    MENU_1_TH,      //4 阈值设置
    MENU_2_CFG,     //5 系统设置
    MENU_3_HIS,     //6 历史记录
    MENU_4_CLR,     //7 清除累计
    
    // 二级菜单（阈值设置）
    MENU_1A_RTH,    //8 阈值设置 - 剂量率阈值设置
    MENU_1B_DTH,    //9 阈值设置 - 剂量阈值设置
    MENU_1_BACK,    //10 阈值设置 - 返回上一级
    
    // 三级菜单（剂量阈值设置）
    MENU_1Ba_DAYD,  //11 剂量阈值设置 - 当日累计阈值
    MENU_1Bb_CRTD,  //12 剂量阈值设置 - 当前累计阈值
    MENU_1B_BACK,   //13 剂量阈值设置 - 返回上一级
    
    // 二级菜单（系统设置）
    MENU_2A_DT,     //14 系统设置 - 日期时间设置
    MENU_2B_SH,     //15 系统设置 - 显示设置
    MENU_2C_RE,     //16 系统设置 - 恢复默认设置
    MENU_2D_INFO,   //17 系统设置 - 关于本机
    MENU_2_BACK,    //18 系统设置 - 返回上一级
    
    // 三级菜单（显示设置）
    MENU_2Ba_CBR,   //19 显示设置 - 屏幕亮度
    MENU_2Bb_COT,   //20 显示设置 - 熄屏时间
    MENU_2B_BACK,   //21 显示设置 - 返回上一级
    
    // 屏幕亮度
    SET_CBR_INTF,   //22 亮度等级
    
    // 熄屏时间
    SET_COT_INTF,   //23 熄屏时长
    
    // 关于本机
    SYS_INFO_1,     //24 关于本机界面1
    SYS_INFO_2,     //25 关于本机界面2
    SYS_INFO_3,     //26 关于本机界面3
    
    // 剂量率阈值
    SET_RTH_INT,    //27 剂量率阈值设置
    SET_RTH_POINT,  //28 剂量率阈值设置
    SET_RTH_UNIT,   //29 剂量率阈值设置
    
    // 当日累计剂量阈值
    SET_DAYTH_INT,  //30 当日累计剂量阈值设置
    SET_DAYTH_POINT,//31 当日累计剂量阈值设置
    SET_DAYTH_UNIT, //32 当日累计剂量阈值设置
    
    // 当前累计剂量阈值
    SET_CRTTH_INT,  //33 当前累计剂量阈值设置
    SET_CRTTH_POINT,//34当前累计剂量阈值设置
    SET_CRTTH_UNIT, //35 当前累计剂量阈值设置
    
    // 日期时间设置
    SET_TIME_YEAR,  //36 日期 - 年
    SET_TIME_MONTH, //37 日期 - 月
    SET_TIME_DAY,   //38 日期 - 日
    SET_TIME_HOUR,  //39 时间 - 时
    SET_TIME_MIN,   //40 时间 - 分
    
    // 数值修改
    SET_VAL_INC,    //41 数值 - 增大
    SET_VAL_SUB,    //42 数值 - 减小
    SET_VAL_LIMIT,  //43 数值 - 错误提示
    SET_VAL_RE,     //44 数值 - 重新设置
//    SET_VAL_SAVE,   // 数值 - 保存（单独引用）
    
    // 历史记录
    HIS_PAGE_HOME,  //45 历史记录 - 首页
    HIS_PAGE_UP,    //46 历史记录 - 上一页
    HIS_PAGE_DOWN,  //47 历史记录 - 下一页
    
    // 清除当日数据
    CLR_DAY_DATA_Y, //48 清除当日累计 - 是
    CLR_DAY_DATA_N, //49 清除当日累计 - 否
    CLR_DAY_DATA,   //50 清除当日累计
    
    // 清除累计
    CLR_CRT_DOSE_Y, //51 清除累计 - 是
    CLR_CRT_DOSE_N, //52 清除累计 - 否
    CLR_CRT_DOSE,   // 清除累计
    
    // 恢复默认设置
    SYS_RESET_Y,    //53 恢复默认设置 - 是
    SYS_RESET_N,    //54 恢复默认设置 - 否
    SYS_RESET,      //55 恢复默认设置
    
    // 计时模式
    TIMING_RUN,     //56 计时模式 - 使能
    TIMING_SW,      //57 计时模式 - 切换（打开/关闭）
    TIMING_HIS,     //58 计时模式 - 历史记录
    
    MENU_CNT,       //59 界面总数（最大暂定：255）
    MENU_NULL,
};

extern uint8_t crt_inft;
extern uint8_t bef_inft;
extern uint8_t crt_depth;

extern void menu_func(uint8_t idx,uint8_t def_menu);

#endif




