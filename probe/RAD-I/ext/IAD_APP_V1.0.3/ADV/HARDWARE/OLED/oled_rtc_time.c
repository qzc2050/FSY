#include "control.h"
#include "ui_menu.h"

#include <stdio.h>


/********************************************************************************************
* 函数名：set_datatime
* 描述  ：修改系统时间
********************************************************************************************/
void set_datatime(void)
{
    char *str_p;
    uint8_t ofs,xpos;
    
	if(crt_depth != DEPTH_SET_TIME)
	{
        crt_depth = DEPTH_SET_TIME;
        
		OLED_Clear();
		OLED_ShowChinese(6,2,"日期和时间",12);
		OLED_Draw_Border_Line();
        sprintf(str_temp,"%02d/%02d/%02d %02d:%02d",data_time.year,data_time.month,data_time.day,\
                                                data_time.hour,data_time.minute);
		OLED_ShowString(16,24,(uint8_t *)str_temp,32);
	}

    switch(crt_inft)
    {
        case SET_TIME_YEAR:  ofs = 0; break;
        case SET_TIME_MONTH: ofs = 3; break;
        case SET_TIME_DAY:   ofs = 6; break;
        case SET_TIME_HOUR:  ofs = 9; break;
        case SET_TIME_MIN:   ofs = 12; break;
    }
    str_p = &str_temp[ofs];
    xpos = 16 * ++ofs;
    Request_Twinkle(xpos,24,str_p,2,true);
    key_ctr.muti_long = true;
}

/********************************************************************************************
* 函数名：Set_Time_Val
* 描  述：增大/减小选中数字的值
* 输  入：TIME_VAL_UP -> 增大  TIME_VAL_DOWN -> 减小
********************************************************************************************/
void Set_Time_Val(bool ctr)
{
    uint8_t year,month,day;
    
    void (*fun_ptr)(uint8_t,uint8_t,char *);

    if(ctr)
        fun_ptr = Set_Val_Up;
    else
        fun_ptr = Set_Val_Down;
    
	if(bef_inft == SET_TIME_YEAR)   //对‘年’进行操作（时间设置为为闰年且为2月30、31日时，修改日期为29日；平年同理）
	{
		fun_ptr(0,99,str_temp);
        sscanf(str_temp,"%hhd/%hhd/%hhd",&year,&month,&day);
		if((month == 2) && (day >= 29))
		{
            Feb_28_day:
            str_temp[6] = '2';
            if(year % 4)
                str_temp[7] = '8';
            else
                str_temp[7] = '9';
            OLED_ShowString(16,24,(uint8_t *)str_temp,32);  // 主要刷新 -> 日
		}
	}
	else if(bef_inft == SET_TIME_MONTH)   //对‘月’进行操作（范围1-12）
	{
		fun_ptr(1,12,&str_temp[3]);
		sscanf(str_temp,"%hhd/%hhd/%hhd",&year,&month,&day);
		if(day == 31)
		{
			if(month == 4||month == 6||month == 9||month == 11)
				str_temp[7] = '0';
            else if(month == 2)
                goto Feb_28_day;

			OLED_ShowString(16,24,(uint8_t *)str_temp,32);  // 主要刷新 -> 日
		}
		else if(((day == 30) || (day == 29)) && (month == 2))
			goto Feb_28_day;
	}
	else if(bef_inft == SET_TIME_DAY)   //对‘日’进行操作（闰年的2月上限29，平年为28天，1-3-5-7-8-10-12月上限31天，4-6-9-11月上限30天）
	{
		sscanf(str_temp,"%hhd/%hhd/%hhd",&year,&month,&day);
		if((month == 1) || (month == 3) || (month == 5) || (month == 7) || (month == 8)\
        || (month == 10) || (month == 12))
			fun_ptr(1,31,&str_temp[6]);
		else if((month == 4) || (month == 6) || (month == 9) || (month == 11))
			fun_ptr(1,30,&str_temp[6]);
		else if(month == 2)
		{
			if(year % 4)
				fun_ptr(1,28,&str_temp[6]);
			else
				fun_ptr(1,29,&str_temp[6]);
		}
	}
}

/********************************************************************************************
* 函数名：SAVE_RTC_TIME
* 描  述：保存设置的时间
********************************************************************************************/
void SAVE_RTC_TIME(void)
{
	uint32_t date_temp = Get_Date_uint();
	
    sscanf(str_temp,"%hd/%hhd/%hhd %hhd:%hhd",&data_time.year,\
                    &data_time.month,&data_time.day,&data_time.hour,&data_time.minute);
	pcf8563_set_cur_time(&data_time);
	
	data_var.day_date = Get_Date_uint();
	STMDATAEEPROM_Write(DAY_DATE_ADDR,(uint32_t *)(&data_var.day_date),1);
	
	/* 调整历史数据保存 */
	Adjust_History(date_temp);
    menu_func(NULL,CLR_DAY_DATA_Y);
}





