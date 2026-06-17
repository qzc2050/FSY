#include <stdio.h>
#include <stdint.h>

#include "ui_menu.h"
#include "muti_menu.h"
#include "oled_set_val.h"
#include "oled_rtc_time.h"
#include "timing_mode.h"
#include "oled_data.h"

// 当前界面
uint8_t crt_inft = MENU_HOME_1;

// 上次界面
uint8_t bef_inft = MENU_HOME_1;

// 当前菜单层级
uint8_t crt_depth = DEPTH_HOME_1;

/* 多级菜单跳转列表 */
// {(菜单索引),(开关键/确认键短按)子菜单索引,(左键短按)子菜单索引,(右键短按)子菜单索引}
const uint8_t menu_idx[MENU_CNT][4] = {
    {MENU_HOME_1  ,  MENU_0_BACK  ,  MENU_HOME_2  ,  MENU_HOME_2},
    {MENU_HOME_2  ,  MENU_0_BACK  ,  MENU_HOME_1  ,  MENU_HOME_1},
    {MENU_0_BACK  ,  MENU_HOME_1  ,  MENU_4_CLR   ,  MENU_1_TH},
    {MENU_1_TH    ,  MENU_1A_RTH  ,  MENU_0_BACK  ,  MENU_2_CFG},
    {MENU_2_CFG   ,  MENU_2A_DT   ,  MENU_1_TH    ,  MENU_3_HIS},
    {MENU_3_HIS   ,  HIS_PAGE_HOME,  MENU_2_CFG   ,  MENU_4_CLR},
    {MENU_4_CLR   ,  CLR_CRT_DOSE_Y, MENU_3_HIS   ,  MENU_0_BACK},
    {MENU_1A_RTH  ,  SET_RTH_INT  ,  MENU_1_BACK  ,  MENU_1B_DTH},
    {MENU_1B_DTH  ,  MENU_1Ba_DAYD,  MENU_1A_RTH  ,  MENU_1_BACK},
    {MENU_1_BACK  ,  MENU_0_BACK  ,  MENU_1B_DTH  ,  MENU_1A_RTH},
    {MENU_1Ba_DAYD,  SET_DAYTH_INT,  MENU_1B_BACK ,  MENU_1Bb_CRTD},
    {MENU_1Bb_CRTD,  SET_CRTTH_INT,  MENU_1Ba_DAYD,  MENU_1B_BACK},
    {MENU_1B_BACK ,  MENU_1_BACK  ,  MENU_1Bb_CRTD,  MENU_1Ba_DAYD},
    {MENU_2A_DT   ,  SET_TIME_YEAR,  MENU_2_BACK  ,  MENU_2B_SH},
    {MENU_2B_SH   ,  MENU_2Ba_CBR ,  MENU_2A_DT   ,  MENU_2C_RE},
    {MENU_2C_RE   ,  SYS_RESET_Y  ,  MENU_2B_SH   ,  MENU_2D_INFO},
    {MENU_2D_INFO ,  SYS_INFO_1   ,  MENU_2C_RE   ,  MENU_2_BACK},
    {MENU_2_BACK  ,  MENU_0_BACK  ,  MENU_2D_INFO ,  MENU_2A_DT},
    {MENU_2Ba_CBR ,  SET_CBR_INTF ,  MENU_2B_BACK ,  MENU_2Bb_COT},
    {MENU_2Bb_COT ,  SET_COT_INTF ,  MENU_2Ba_CBR ,  MENU_2B_BACK},
    {MENU_2B_BACK ,  MENU_2_BACK  ,  MENU_2Bb_COT ,  MENU_2Ba_CBR},
    {SET_CBR_INTF ,  MENU_2B_BACK ,  SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_COT_INTF ,  MENU_2B_BACK ,  SET_VAL_SUB  ,  SET_VAL_INC},
    {SYS_INFO_1   ,  MENU_2_BACK  ,  SYS_INFO_1   ,  SYS_INFO_2},
    {SYS_INFO_2   ,  MENU_2_BACK  ,  SYS_INFO_1   ,  SYS_INFO_3},
    {SYS_INFO_3   ,  MENU_2_BACK  ,  SYS_INFO_2   ,  SYS_INFO_3},
    {SET_RTH_INT  ,  SET_RTH_POINT,  SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_RTH_POINT,  SET_RTH_UNIT ,  SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_RTH_UNIT ,  SET_RTH_INT  ,  SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_DAYTH_INT,  SET_DAYTH_POINT,SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_DAYTH_POINT,SET_DAYTH_UNIT, SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_DAYTH_UNIT, SET_DAYTH_INT,  SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_CRTTH_INT,  SET_CRTTH_POINT,SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_CRTTH_POINT,SET_CRTTH_UNIT, SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_CRTTH_UNIT, SET_CRTTH_INT,  SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_TIME_YEAR,  SET_TIME_MONTH, SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_TIME_MONTH, SET_TIME_DAY ,  SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_TIME_DAY ,  SET_TIME_HOUR,  SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_TIME_HOUR,  SET_TIME_MIN ,  SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_TIME_MIN ,  SET_TIME_YEAR,  SET_VAL_SUB  ,  SET_VAL_INC},
    {SET_VAL_INC  ,  MENU_NULL    ,  MENU_NULL    ,  MENU_NULL},
    {SET_VAL_SUB  ,  MENU_NULL    ,  MENU_NULL    ,  MENU_NULL},
    {SET_VAL_LIMIT,  SET_VAL_RE   ,  SET_VAL_RE   ,  SET_VAL_RE},
    {SET_VAL_RE   ,  MENU_NULL    ,  MENU_NULL    ,  MENU_NULL},
    {HIS_PAGE_HOME,  MENU_0_BACK  ,  HIS_PAGE_UP  ,  HIS_PAGE_DOWN},
    {HIS_PAGE_UP  ,  MENU_NULL    ,  MENU_NULL    ,  MENU_NULL},
    {HIS_PAGE_DOWN,  MENU_NULL    ,  MENU_NULL    ,  MENU_NULL},
    {CLR_DAY_DATA_Y, CLR_DAY_DATA ,  CLR_DAY_DATA_N, CLR_DAY_DATA_N},
    {CLR_DAY_DATA_N, MENU_2_BACK  ,  CLR_DAY_DATA_Y, CLR_DAY_DATA_Y},
    {CLR_DAY_DATA ,  MENU_NULL    ,  MENU_NULL    ,  MENU_NULL},
    {CLR_CRT_DOSE_Y, CLR_CRT_DOSE ,  CLR_CRT_DOSE_N, CLR_CRT_DOSE_N},
    {CLR_CRT_DOSE_N, MENU_0_BACK  ,  CLR_CRT_DOSE_Y, CLR_CRT_DOSE_Y},
    {CLR_CRT_DOSE ,  MENU_NULL    ,  MENU_NULL    ,  MENU_NULL},
    {SYS_RESET_Y  ,  SYS_RESET    ,  SYS_RESET_N  ,  SYS_RESET_N},
    {SYS_RESET_N  ,  MENU_2_BACK  ,  SYS_RESET_Y  ,  SYS_RESET_Y},
    {SYS_RESET    ,  MENU_NULL    ,  MENU_NULL    ,  MENU_NULL},
    {TIMING_RUN   ,  MENU_0_BACK  ,  MENU_HOME_2  ,  TIMING_SW},
    {TIMING_SW    ,  MENU_0_BACK  ,  MENU_HOME_2  ,  TIMING_SW},
    {TIMING_HIS   ,  MENU_HOME_1  ,  MENU_NULL    ,  MENU_NULL},
};

/********************************************************************************************
* 函数名：menu_func
* 描述  ：菜单功能
* 输入  : idx -> 按键索引，def_menu -> 前往指定菜单
********************************************************************************************/
void menu_func(uint8_t idx,uint8_t def_menu)
{
    uint8_t i;
    
    bef_inft = crt_inft;
    if(def_menu == MENU_NULL)
    {
        // 获取当前菜单索引
        for(i = 0;i < MENU_CNT;i++)
            if(menu_idx[i][CTR_IDX] == crt_inft)
                break;
            
        if((i >= MENU_CNT) || (menu_idx[i][idx + 1] == MENU_NULL))
            return;
        crt_inft = menu_idx[i][idx + 1];
    }
    else
        crt_inft = def_menu;
//    printf("CRT Inft: %d!\r\n",crt_inft);
    Set_Sc_Extinct_Time(0);
    // 根据指令跳转界面/执行相应的指令
    switch(crt_inft)
    {
        case MENU_HOME_1:    menu_home_1();break;       // DEPTH_HOME_1
        case MENU_HOME_2:    menu_home_2();break;       // DEPTH_HOME_2
        case MENU_0_BACK:    Menu_Home(14,26);break;    // DEPTH_MENU_HOME
        case MENU_1_TH:      Menu_Home(64,13);break;    // NULL
        case MENU_2_CFG:     Menu_Home(155,13);break;   // NULL
        case MENU_3_HIS:     Menu_Home(64,39);break;    // NULL
        case MENU_4_CLR:     Menu_Home(155,39);break;   // NULL
        case MENU_1A_RTH:    Menu_TH_Set(17,34);break;  // DEPTH_MENU_TH
        case MENU_1B_DTH:    Menu_TH_Set(124,34);break; // NULL
        case MENU_1_BACK:    Menu_TH_Set(217,34);break; // NULL
        case MENU_1Ba_DAYD:  Menu_TH_Dose_Set(7,34);break;    // DEPTH_MENU_TH_DOSE
        case MENU_1Bb_CRTD:  Menu_TH_Dose_Set(118,34);break;  // NULL
        case MENU_1B_BACK:   Menu_TH_Dose_Set(216,34);break;  // NULL
        case MENU_2A_DT:     Menu_Sys_Set(11,24);break;       // DEPTH_MENU_SYS
        case MENU_2B_SH:     Menu_Sys_Set(126,24);break;      // NULL
        case MENU_2C_RE:     Menu_Sys_Set(11,44);break;       // NULL
        case MENU_2D_INFO:   Menu_Sys_Set(126,44);break;      // NULL
        case MENU_2_BACK:    Menu_Sys_Set(213,34);break;      // NULL
        case MENU_2Ba_CBR:   Menu_Display_Set(18,34);break;   // DEPTH_MENU_DISPLAY
        case MENU_2Bb_COT:   Menu_Display_Set(112,34);break;  // NULL
        case MENU_2B_BACK:   Menu_Display_Set(206,34);break;  // NULL
        case SET_CBR_INTF:   set_bar_intf();break;     // DEPTH_SET_BAR
        case SET_COT_INTF:   set_bar_intf();break;     // NULL
        case SYS_INFO_1:     sys_info_1();break;       // DEPTH_SYS_INFO_1
        case SYS_INFO_2:     sys_info_2();break;       // DEPTH_SYS_INFO_2
        case SYS_INFO_3:     sys_info_3();break;       // DEPTH_SYS_INFO_3
        case SET_RTH_INT:    set_rth();break;          // DEPTH_SET_RTH
        case SET_RTH_POINT:  set_rth();break;          // NULL
        case SET_RTH_UNIT:   set_rth();break;          // NULL
        case SET_DAYTH_INT:  set_dth();break;          // DEPTH_SET_DAYTH
        case SET_DAYTH_POINT:set_dth();break;          // NULL
        case SET_DAYTH_UNIT: set_dth();break;          // NULL
        case SET_CRTTH_INT:  set_dth();break;          // DEPTH_SET_CRTTH
        case SET_CRTTH_POINT:set_dth();break;          // NULL
        case SET_CRTTH_UNIT: set_dth();break;          // NULL
        case SET_TIME_YEAR:  set_datatime();break;     // DEPTH_SET_TIME
        case SET_TIME_MONTH: set_datatime();break;     // NULL
        case SET_TIME_DAY:   set_datatime();break;     // NULL
        case SET_TIME_HOUR:  set_datatime();break;     // NULL
        case SET_TIME_MIN:   set_datatime();break;     // NULL
        case SET_VAL_INC:    Set_Val_Inc();break;      // NULL
        case SET_VAL_SUB:    Set_Val_Sub();break;      // NULL
        case SET_VAL_LIMIT:  Set_Val_Limit();break;    // DEPTH_VAL_LIMIT
        case SET_VAL_RE:     Set_Val_Re();break;       // NULL
        case HIS_PAGE_HOME:  His_Page_Home();break;    // DEPTH_HISTORY
        case HIS_PAGE_UP:    His_Page_Up();break;      // NULL
        case HIS_PAGE_DOWN:  His_Page_Down();break;    // NULL
        case CLR_DAY_DATA_Y: Menu_Clr_Day_Data(64 ,38);break;   // DEPTH_CLR_DAY
        case CLR_DAY_DATA_N: Menu_Clr_Day_Data(156 ,38);break;  // NULL
        case CLR_DAY_DATA:   Clr_Day_Data();break;     // NULL
        case CLR_CRT_DOSE_Y: Menu_Clr_Crt_Dose(64 ,38);break;   // DEPTH_CLR_CRT
        case CLR_CRT_DOSE_N: Menu_Clr_Crt_Dose(156 ,38);break;
        case CLR_CRT_DOSE:   Clr_Crt_Dose();break;     // NULL
        case SYS_RESET_Y:    Menu_Reset(64 ,40);break; // DEPTH_MENU_RST
        case SYS_RESET_N:    Menu_Reset(156 ,40);break;// NULL
        case SYS_RESET:      Sys_Reset();break;        // NULL
        case TIMING_RUN:     break;                    // NULL
        case TIMING_SW:      Timing_SW();break;        // NULL
        case TIMING_HIS:     Enter_Timing_Mode_History();break; // DEPTH_TIMING_HIS
//        default: printf("Undef Inft: %d!\r\n",crt_inft);break;  // NULL
    }
}







