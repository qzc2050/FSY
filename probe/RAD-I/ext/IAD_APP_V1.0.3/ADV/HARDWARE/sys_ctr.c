#include <math.h>
#include <string.h>

#include "sys_ctr.h"
#include "stm_flash.h"


__IO Sys_Cfg_Struct sys_cfg = {0};
__IO KEY_Ctr_Struct key_ctr = {0};
__IO Data_Var_Struct data_var = {0};
__IO Sys_Bits_Struct sys_bits = {0};

__IO Dev_Data_Union udata = {0};

/********************************************************************************************
* 函数名：Date_Conv
* 描  述：将日期转换为数据保存格式
* 输  入：conv_date -> 转换的日期（年月日，格式：241108）
* 输  出：返回逆转换的日期（月日年，格式：110824）
********************************************************************************************/
uint32_t Date_Conv(uint32_t conv_date)
{
    return ((conv_date % 10000) * 100 + (conv_date / 10000));
}

/********************************************************************************************
* 函数名：Date_Inconvert
* 描  述：将数据保存格式转换为日期
* 输  入：conv_date -> 逆转换的日期（月日年，格式：110824）
* 输  出：返回逆转换的日期（年月日，格式：241108）
********************************************************************************************/
uint32_t Date_Inconvert(uint32_t conv_date)
{
    return ((conv_date % 100) * 10000 + (conv_date / 100));
}

/********************************************************************************************
* 函数名：Save_Sys_Config
* 描述  ：保存系统设置
********************************************************************************************/
void Save_Sys_Config(void)
{
    STMDATAEEPROM_Write(PW_TK_ADDR,(uint32_t *)&sys_cfg.power_tk,1);
}

/********************************************************************************************
* 函数名：Get_SN
* 描述  ：获取设备序列号
********************************************************************************************/
void Get_SN(uint32_t *des_SN)
{
    STMDATAEEPROM_Read(SN_ADDR,des_SN,(SN_LEN % 4) ? ((SN_LEN / 4) + 1) : (SN_LEN / 4));
}

/********************************************************************************************
* 函数名：Set_SN
* 描述  ：设置设备序列号
********************************************************************************************/
void Set_SN(char *str)
{
    memcpy((char *)sys_cfg.dev.u8_SN,str,SN_LEN+1);
    STMDATAEEPROM_Write(SN_ADDR,(uint32_t *)sys_cfg.dev.u32_SN,\
                        (SN_LEN % 4) ? ((SN_LEN / 4) + 1) : (SN_LEN / 4));
}

/********************************************************************************************
* 函数名: DataUnit_To_Float
* 输  入: data -> 数据值，unit -> 数据单位（代表10的n次方）
* 输  出: 转换后的数据值
* 描  述: float数据类型转换自定义数据类型
********************************************************************************************/
float DataUnit_To_Float(Data_Struct conv_st)
{
    return ((conv_st.data * pow(1000, conv_st.unit)) / 100.0f);
}

/********************************************************************************************
* 函数名: Float_To_DataUnit
* 输  入: data -> 需转换的浮点型数据，
*         data_type -> 数据类型（true -> 当日剂量累计/ false -> 总剂量累计）
* 描  述: float数据类型转换自定义数据类型
********************************************************************************************/
void Float_To_DataUnit(float data,uint8_t data_type)
{
    uint8_t unit;
    uint16_t val;
    
	if(data < 1E2)
	{
        val = (uint16_t)(data * 100);    //保留两位小数，故乘以100
		unit = UNIT_USV_H;
	}
	else if(data < 1E5)
	{
        if((data_type == RATE_SW) && (data >= 1E4))   // 剂量率最大值为 10 mSv/h
            val = 1E3;
        else
            val = (uint16_t)(data / 10);
//        data = (uint16_t)(data / 1000 * 100);    //保留两位小数，故乘以100
        unit = UNIT_MSV_H;
	}
	else if(data < 1E8)
	{
//        data = (uint16_t)(data / 1000000 * 100);    //保留两位小数，故乘以100
        val = (uint16_t)(data / 1E4);
		unit = UNIT_SV_H;
	}
	else   // 上限值99.99 uSv
	{
		val = 9999;
		unit = UNIT_SV_H;
	}
    
    if(data_type == CRT_DOSE_SW)
    {
        udata.dose.sum_data = val;
        udata.dose.sum_unit = unit;
    }
    else if(data_type == DAY_DOSE_SW)
    {
        udata.day.sum_data = val;
        udata.day.sum_unit = unit;
    }
    else
    {
        udata.day.rate_data = val;
        udata.day.rate_unit = unit;
    }
}

/********************************************************************************************
* 函数名：System_Time_Init
* 描  述：初始化计时
* 输  入: @time_tick: 起始时间指针变量
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void System_Time_Init(__IO uint32_t *time_tick)
{
    *time_tick = HAL_GetTick();
}

/********************************************************************************************
* 函数名：System_Time_Wait
* 描  述：计时等待
* 输  入: @time: 定时时间（单位：ms），@time_tick: 起始时间指针变量
* 输  出：false -> 等待中，true -> 等待完成
* 调  用：外部调用
********************************************************************************************/
bool System_Time_Wait(uint32_t time, uint32_t time_tick)
{
    uint32_t interval = 0;

    interval = HAL_GetTick() - time_tick;
    if(interval < time)
        return false;

    return true;
}















