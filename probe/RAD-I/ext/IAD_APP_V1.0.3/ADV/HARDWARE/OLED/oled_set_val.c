#include "control.h"
#include "ui_menu.h"

extern struct time_type__ data_time;

/********************************************************************************************
* 函数名：set_rth
* 描述  ：OLED修改剂量率阈值
********************************************************************************************/
void set_rth(void)
{
	if(crt_depth != DEPTH_SET_RTH)
	{
        crt_depth = DEPTH_SET_RTH;
        
		OLED_Clear();
        memset(str_temp,0,sizeof(str_temp));
		
		OLED_ShowChinese(6,2,"剂量率阈值",12);
		OLED_Draw_Border_Line();

		OLED_ShowString(24,24,"TH",32);
        
        if(sys_cfg.th_real_rate.unit == UNIT_USV_H)
            sprintf(str_temp, "%05.2f uSv/h",sys_cfg.th_real_rate.data / 100.0f);
        else
            sprintf(str_temp, "%05.2f mSv/h",sys_cfg.th_real_rate.data / 100.0f);

		OLED_ShowString(72,24,(uint8_t *)&str_temp,32);  //显示剂量率
	}
    
    if(crt_inft == SET_RTH_INT)
        Request_Twinkle(72,24,str_temp,2,true);
    else if(crt_inft == SET_RTH_POINT)
        Request_Twinkle(120,24,&str_temp[3],2,true);
    else
        Request_Twinkle(168,24,&str_temp[6],5,true);
    key_ctr.muti_long = true;
}

/********************************************************************************************
* 函数名：set_dth
* 描述  ：OLED修改总剂量阈值
* 输出  ：1：刷新OLED显示屏上的数据     0：不刷新OLED显示屏上的数据
********************************************************************************************/
void set_dth(void)
{
    Data_Struct *dp;
    
	if((crt_depth != DEPTH_SET_DAYTH) && (crt_depth != DEPTH_SET_CRTTH))
	{
		OLED_Clear();

		if(crt_inft == SET_DAYTH_INT)
		{
            crt_depth = DEPTH_SET_DAYTH;
			OLED_ShowChinese(6,2,"当日累计阈值",12);
            dp = (void *)&sys_cfg.th_day_dose;
		}
		else if(crt_inft == SET_CRTTH_INT)
		{
            crt_depth = DEPTH_SET_CRTTH;
			OLED_ShowChinese(6,2,"总累计阈值",12);
            dp = (void *)&sys_cfg.th_crt_dose;
		}
        OLED_Draw_Border_Line();
		OLED_ShowString(39,24,"TH",32);
        
		if(dp->unit == UNIT_USV_H)
            sprintf(str_temp, "%05.2f uSv",dp->data / 100.0f);
        else if(dp->unit == UNIT_MSV_H)
            sprintf(str_temp, "%05.2f mSv",dp->data / 100.0f);
        else
            sprintf(str_temp, "%05.2f  Sv",dp->data / 100.0f);

		OLED_ShowString(87,24,(uint8_t *)&str_temp,32);  //显示剂量
	}
    if((crt_inft == SET_DAYTH_INT) || (crt_inft == SET_CRTTH_INT))
        Request_Twinkle(87,24,str_temp,2,true);
    else if((crt_inft == SET_DAYTH_POINT) || (crt_inft == SET_CRTTH_POINT))
        Request_Twinkle(135,24,&str_temp[3],2,true);
    else
        Request_Twinkle(183,24,&str_temp[6],3,true);
    key_ctr.muti_long = true;
}

/********************************************************************************************
* 函数名：Set_Val_Up
* 描  述：增加选中数字的值
* 输  入：min -> 最小值，max -> 最大值，*str -> 数字的地址指针
********************************************************************************************/
void Set_Val_Up(uint8_t min,uint8_t max,char *str)
{
    uint8_t value = (*str - '0') * 10 + str[1] - '0'+ 1;
    
    if(value > max) 
        value = min;
    *str = value / 10 + '0';
    str[1] = value % 10 + '0';
    
    crt_inft = bef_inft;
    menu_func(NULL,crt_inft);
}

/********************************************************************************************
* 函数名：Set_Val_Down
* 描述  ：减少选中数字的值
* 输  入：min -> 最小值，max -> 最大值，*str -> 数字的地址指针，type -> 数据类型
********************************************************************************************/
void Set_Val_Down(uint8_t min,uint8_t max,char *str)
{
    uint8_t value = (*str - '0') * 10 + str[1] - '0';

    if(value <= min)
        value = max;
    else
        value--;

    *str = (value / 10) + '0';
    str[1] = (value % 10) + '0';
    
    crt_inft = bef_inft;
    menu_func(NULL,crt_inft);
}

/********************************************************************************************
* 函数名：Set_Unit_Up
* 描  述：增大选中单位
* 输  入：无
********************************************************************************************/
void Set_Unit_Up(void)
{
    if(str_temp[6] == 'u')
            str_temp[6] = 'm';
        else if(str_temp[6] == 'm')
            str_temp[6] = ' ';
        else
            str_temp[6] = 'u';
    
    crt_inft = bef_inft;
    menu_func(NULL,crt_inft);
}

/********************************************************************************************
* 函数名：Set_Unit_Down
* 描  述：增大选中单位
* 输  入：无
********************************************************************************************/
void Set_Unit_Down(void)
{
    if(str_temp[6] == 'u')
            str_temp[6] = ' ';
        else if(str_temp[6] == 'm')
            str_temp[6] = 'u';
        else
            str_temp[6] = 'm';
    
    crt_inft = bef_inft;
    menu_func(NULL,crt_inft);
}

/********************************************************************************************
* 函数名：Set_Val_Inc
* 描述  ：增加选中数字的值
********************************************************************************************/
void Set_Val_Inc(void)
{
    switch(bef_inft)
    {
        case SET_RTH_INT:     Set_Val_Up(0,99,str_temp);break;
        case SET_RTH_POINT:   Set_Val_Up(0,99,&str_temp[3]);break;
        case SET_RTH_UNIT:    Set_Unit_Up();break;
        case SET_DAYTH_INT:   Set_Val_Up(0,99,str_temp);break;
        case SET_DAYTH_POINT: Set_Val_Up(0,99,&str_temp[3]);break;
        case SET_DAYTH_UNIT:  Set_Unit_Up();break;
        case SET_CRTTH_INT:   Set_Val_Up(0,99,str_temp);break;
        case SET_CRTTH_POINT: Set_Val_Up(0,99,&str_temp[3]);break;
        case SET_CRTTH_UNIT:  Set_Unit_Up();break;
        case SET_TIME_YEAR:   Set_Time_Val(TIME_VAL_UP);break;
        case SET_TIME_MONTH:  Set_Time_Val(TIME_VAL_UP);break;
        case SET_TIME_DAY:    Set_Time_Val(TIME_VAL_UP);break;
        case SET_TIME_HOUR:   Set_Val_Up(0,23,&str_temp[9]);break;
        case SET_TIME_MIN:    Set_Val_Up(0,59,&str_temp[12]);break;
        case SET_CBR_INTF:    Progress_Bar_Up();break;
        case SET_COT_INTF:    Progress_Bar_Up();break;
    }
}

/********************************************************************************************
* 函数名：Set_Val_Sub
* 描述  ：增加选中数字的值
********************************************************************************************/
void Set_Val_Sub(void)
{
    switch(bef_inft)
    {
        case SET_RTH_INT:     Set_Val_Down(0,99,str_temp);break;
        case SET_RTH_POINT:   Set_Val_Down(0,99,&str_temp[3]);break;
        case SET_RTH_UNIT:    Set_Unit_Down();break;
        case SET_DAYTH_INT:   Set_Val_Down(0,99,str_temp);break;
        case SET_DAYTH_POINT: Set_Val_Down(0,99,&str_temp[3]);break;
        case SET_DAYTH_UNIT:  Set_Unit_Down();break;
        case SET_CRTTH_INT:   Set_Val_Down(0,99,str_temp);break;
        case SET_CRTTH_POINT: Set_Val_Down(0,99,&str_temp[3]);break;
        case SET_CRTTH_UNIT:  Set_Unit_Down();break;
        case SET_TIME_YEAR:   Set_Time_Val(TIME_VAL_DOWN);break;
        case SET_TIME_MONTH:  Set_Time_Val(TIME_VAL_DOWN);break;
        case SET_TIME_DAY:    Set_Time_Val(TIME_VAL_DOWN);break;
        case SET_TIME_HOUR:   Set_Val_Down(0,23,&str_temp[9]);break;
        case SET_TIME_MIN:    Set_Val_Down(0,59,&str_temp[12]);break;
        case SET_CBR_INTF:    Progress_Bar_Sub();break;
        case SET_COT_INTF:    Progress_Bar_Sub();break;
    }
}

/********************************************************************************************
* 函数名：Save_Val
* 描述  ：保存设置的阈值
********************************************************************************************/
void Save_Val(void)
{
    uint8_t unit;
	char *endptr;
	uint16_t str_val;
	
	str_val = strtof(str_temp,&endptr) * 100;
    
	if(!str_val)   //禁止设置为 0
    {
        menu_func(NULL,SET_VAL_LIMIT);
        return;
    }
    
    if(str_temp[6] == 'u')
        unit = UNIT_USV_H;
    else if(str_temp[6] == 'm')
        unit = UNIT_MSV_H;
    else
        unit = UNIT_SV_H;

	if(crt_depth == DEPTH_SET_RTH)
	{
		/*****************剂量率阈值上限处理*****************/
		if(((str_val > 1000) && (str_temp[6] == 'm')) || (str_temp[6] == ' '))
		{
            menu_func(NULL,SET_VAL_LIMIT);
			return;
		}
		/*****************剂量率阈值上限处理*****************/
        sys_cfg.th_real_rate.data = str_val;
        sys_cfg.th_real_rate.unit = unit;
        menu_func(NULL,MENU_1_BACK);
	}
	else if(crt_depth == DEPTH_SET_DAYTH)
	{
        sys_cfg.th_day_dose.data = str_val;
        sys_cfg.th_day_dose.unit = unit;
	}
	else if(crt_depth == DEPTH_SET_CRTTH)
	{
        sys_cfg.th_crt_dose.data = str_val;
        sys_cfg.th_crt_dose.unit = unit;
	}
    
	STMDATAEEPROM_Write(TH_REAL_RATE_ADDR,(uint32_t *)(&sys_cfg.th_real_rate),2);
    if(crt_depth != DEPTH_MENU_TH)
        menu_func(NULL,MENU_1B_BACK);
}

/********************************************************************************************
* 函数名：Set_Val_Limit
* 描述  ：限值提示界面
********************************************************************************************/
void Set_Val_Limit(void)
{
	OLED_Clear();
	OLED_ShowChinese(58,7,"当前阈值设置超出范围",12);
	OLED_ShowChinese(90,26,"请重新设置",12);
	OLED_ShowChar(160,26,'!',12);
	OLED_ShowChinese(86,45,"按任意键返回",12);
}

/********************************************************************************************
* 函数名：Set_Val_Re
* 描述  ：返回阈值设置界面
********************************************************************************************/
void Set_Val_Re(void)
{
    uint8_t target_inft;

    switch(crt_depth)
    {
        case DEPTH_SET_RTH:  target_inft = SET_RTH_INT;   break;
        case DEPTH_SET_DAYTH:target_inft = SET_DAYTH_INT; break;
        case DEPTH_SET_CRTTH:target_inft = SET_CRTTH_INT; break;
    }
    crt_depth = DEPTH_MENU_TH;
    menu_func(NULL,target_inft);
}


