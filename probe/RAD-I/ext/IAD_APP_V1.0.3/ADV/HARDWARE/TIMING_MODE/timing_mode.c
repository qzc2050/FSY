#include "timing_mode.h"
#include "low_power_run.h"
#include "ui_menu.h"
#include "sys_ctr.h"
#include "beep.h"

Timing_Mode_His_Struct timing_his;
Timing_Mode_Ctr timing_ctr;

/********************************************************************************************
* 函数名：Timing_Mode_Init
* 描述  ：计时模式初始化处理
* 输入  : 无
* 输出  ：无
* 调用  ：内部调用
********************************************************************************************/
static void Timing_Mode_Init(void)
{
    timing_ctr.dose = 0;
    timing_ctr.a_tk = 0;
    timing_ctr.s_tk = data_time.second;
    timing_his.date = Date_Conv(Get_Date_uint());
    timing_his.init_time = data_time.hour * 100 + data_time.minute;
}

/********************************************************************************************
* 函数名：Timing_Show_Icon
* 描述  ：显示计时模式图标
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Timing_Show_Icon(void)
{
	if(timing_ctr.mode == TIMING_MODE_RUN)  // 正在运行定时模式
		OLED_DrawSingleBMP(116,1,12,12,(uint8_t *)&time_icon);
    else
        OLED_ShowString(116,1,"$&",12);
}

/********************************************************************************************
* 函数名：Timing_Show_Time
* 描述  ：显示已累计时间
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Timing_Show_Time(void)
{
	if(timing_ctr.a_tk < 60)  // 小于1min
		sprintf(str_temp,"%d s  ",timing_ctr.a_tk);
	else if(timing_ctr.a_tk < 3600)  // 小于1h
	{
        // (time / 60.0f * 10 / 10.0f)  --> 转换为分，保留一位小数
		sprintf(str_temp,"%.1f m ",((uint32_t)(timing_ctr.a_tk / 6.0f)) / 10.0f);
	}
	else  // 大于等于1h,小于100h
	{
        // (time / 3600.0f * 10 / 10.0f)  --> 转换为时，保留一位小数
		sprintf(str_temp,"%.1f h ",((uint32_t)(timing_ctr.a_tk / 360.0f)) / 10.0f);
	}
	OLED_ShowString(132,1,(uint8_t *)str_temp,12);
}

/********************************************************************************************
* 函数名：Enter_Timing_Mode
* 描述  ：进入计时模式
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Enter_Timing_Mode(void)
{
    crt_inft = TIMING_RUN;
    timing_ctr.mode = TIMING_MODE_RUN;

    ref_sta = true;  // 刷新屏幕数据
    Timing_Mode_Init();
    Timing_Show_Icon();
	Timing_Show_Time();
    Beep_Ctr(BEEP_EVENT_TIMING);
}

/********************************************************************************************
* 函数名：Timing_SW
* 描述  ：暂停计时模式
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Timing_SW(void)
{
	if(timing_ctr.mode == TIMING_MODE_RUN)
	{
        crt_inft = TIMING_SW;
		timing_ctr.mode = TIMING_MODE_PAUSE;
	}
	else if(timing_ctr.mode == TIMING_MODE_PAUSE)
	{
		crt_inft = TIMING_RUN;
		timing_ctr.mode = TIMING_MODE_RUN;
	}
    Timing_Show_Icon();
	Timing_Show_Time();
}

/********************************************************************************************
* 函数名：Back_Timing_Mode
* 描述  ：返回计时模式
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Back_Timing_Mode(void)
{
	if(timing_ctr.mode == TIMING_MODE_RUN)
		crt_inft = TIMING_RUN;
    else if(timing_ctr.mode == TIMING_MODE_PAUSE)
        crt_inft = TIMING_SW;
    else
        return;
    
    Timing_Show_Icon();
	Timing_Show_Time();
}

/********************************************************************************************
* 函数名：Timing_Mode_Restart
* 描述  ：重启计时模式
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Timing_Mode_Restart(void)
{
	Timing_Mode_Data_Save();
	timing_ctr.mode = 0;
	Enter_Timing_Mode();
}

/********************************************************************************************
* 函数名：Exit_Timing_Mode
* 描述  ：退出计时模式
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Exit_Timing_Mode(void)
{
    crt_inft = MENU_HOME_1;
    timing_ctr.mode = TIMING_MODE_OFF;
    
    Timing_Mode_Data_Save();  // 保存数据
    
    OLED_Draw_Fill(116,1,44,12,0x00);
    Beep_Ctr(BEEP_EVENT_TIMING);
    ref_sta = true;    // 刷新屏幕数据
}

/********************************************************************************************
* 函数名：Timing_Mode_Save_EEPROM
* 描述  ：计时模式保存数据到EEPROM
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Timing_Mode_Save_EEPROM(void)
{
	uint32_t data_addr = TIMING_DATA_ADDR;

	if(sys_cfg.timing_ofs)
		data_addr += 8;

    Float_To_DataUnit(timing_ctr.dose,CRT_DOSE_SW);
    timing_his.sum_data = udata.dose.sum_data;
    timing_his.sum_unit = udata.dose.sum_unit;
    
    if(timing_ctr.a_tk >= AUTO_RESTART_TIME)
        timing_ctr.a_tk = AUTO_RESTART_TIME;

    timing_his.t_unit = timing_ctr.a_tk / 10000;
    timing_his.t_time = timing_ctr.a_tk % 10000;
    
	STMDATAEEPROM_Write(data_addr,(uint32_t *)(&timing_his),2);
}

/********************************************************************************************
* 函数名：Timing_Mode_Time_Up
* 描述  ：计时模式累计时间显示更新
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Timing_Mode_Time_Up(void)
{
	int8_t interval = 0;
    uint32_t DET_TK = 200;
	static uint32_t det_tk;
	
	if(sys_bits.run_md != RUN_MODE) // 低功耗模式下50秒累计一次
        DET_TK = 50000;
    
	if(System_Time_Wait(DET_TK,det_tk))
	{
        System_Time_Init(&det_tk);

        LPR_Critical_Execute(pcf8563_get_cur_time,&data_time);    // 外部1s刷新一次，此处200ms刷新一次（非低功耗模式）

		if(timing_ctr.s_tk != data_time.second)
		{
			if(timing_ctr.mode == TIMING_MODE_RUN)
			{
                interval = data_time.second - timing_ctr.s_tk;
                if(interval < 0)
                    interval += 60;
                
                timing_ctr.s_tk = data_time.second;
                timing_ctr.a_tk += interval;
				
				if(timing_ctr.a_tk >= AUTO_RESTART_TIME)    // 考虑改为自动关闭
					Timing_Mode_Restart();
				else if((crt_inft == TIMING_RUN) && (crt_depth == DEPTH_HOME_1))   //
					Timing_Show_Time();
			}
			timing_ctr.s_tk = data_time.second;
		}
	}
}

/********************************************************************************************
* 函数名：Timing_Mode_Data_Save
* 描述  ：计时模式数据保存
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Timing_Mode_Data_Save(void)
{
    Timing_Mode_Save_EEPROM();
    sys_cfg.timing_ofs = !sys_cfg.timing_ofs;
    Save_Sys_Config();
}

/********************************************************************************************
* 函数名：Enter_Timing_Mode_History
* 描述  ：进入计时模式历史记录
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Enter_Timing_Mode_History(void)
{    
	uint8_t valid = 0;
	char strbuf[30];
	char time_str[8];
	uint32_t acc_time;
	
	crt_depth = DEPTH_TIMING_HIS;
    
	OLED_Clear();
	OLED_ShowChinese(96,0,"计时记录",16);
    
//    printf("size %d\r\n",sizeof(timing_his));
    
	for(uint8_t i = 0;i < 2;i++)
	{
        STMDATAEEPROM_Read(TIMING_DATA_ADDR + (i * 8),(uint32_t *)(&timing_his),2);

		if(timing_his.date < 101)
			continue;
		
        acc_time = timing_his.t_unit * 1E4 + timing_his.t_time;
		if(timing_his.t_time < 60)
			sprintf(time_str,"%ds-",acc_time);
		else if(acc_time < 3600)
			sprintf(time_str,"%.1f m-",acc_time/60.0f);
		else
			sprintf(time_str,"%.1f h-",acc_time/3600.0f);
		
        udata.dose.sum_data = timing_his.sum_data;
        udata.dose.sum_unit = timing_his.sum_unit;
		Dose_To_Str(false);
        
		sprintf(strbuf,"%02d-%02d:%02d-%s%s",Date_Inconvert(timing_his.date),timing_his.init_time / 100,timing_his.init_time % 100,time_str,str_temp);
//		strcat(strbuf,time_str);
//		strcat(strbuf,str_temp);

        valid++;
        OLED_ShowString(0,24 * valid,(uint8_t *)strbuf,16);
	}
	
    if(!valid)
        OLED_ShowChinese(72,24,"未保存任何数据!",16);
}

/********************************************************************************************
* 函数名：Exit_Timing_Mode_History
* 描述  ：退出计时模式历史记录
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void Exit_Timing_Mode_History(void)
{
    menu_func(NULL,MENU_HOME_1);
}

