#include "control.h"
#include "ui_menu.h"
#include "beep.h"

/********************************************************************************************
* 函数名：Battery_Get_Percent
* 描  述：获取电池电量百分比 -> 剩余电量五等分(获取当前电量格数)
* 输  入：vol -> 当前电池电压，cmd -> 拉满超过时间
* 输  出：true -> 超过低电量电压一段时间，false -> 低电量状态
********************************************************************************************/
bool Low_Battery_Judge(float vol,bool cmd)
{
    static uint32_t nlb_tk;

    if(vol <= LOW_BAT_VOL)
        System_Time_Init(&nlb_tk);
    else if(System_Time_Wait(10000,nlb_tk))
        return true;
    return false;
}    

/********************************************************************************************
* 函数名：Battery_Get_Percent
* 描  述：获取电池电量百分比 -> 剩余电量五等分(获取当前电量格数)
* 输  入：无
* 输  出：剩余电量格数（最大为5格）
********************************************************************************************/
uint8_t Battery_Get_Percent(void)
{
	float vol;
	uint8_t bat_perc;

    LPR_Critical_Execute(max17048_get_millivolt,&vol);      // 获取电池电压
    LPR_Critical_Execute(max17048_get_percent,&bat_perc);   // 获取剩余电量百分比
    
    // 低电量，自动关机测试
//    vol = 3000;
//    bat_perc = 0;
    // 低电量，自动关机测试
    
    //电池电压大于LOW_BAT_VOL，但读取电量为0%，补偿至1%的电量（避免剩余较多电量却自动关机）
//    if(Low_Battery_Judge(vol,0) && ((vol > LOW_BAT_VOL) || READ_USB) && !bat_perc)
//        bat_perc = 1;
    if((Low_Battery_Judge(vol,0) && (vol > LOW_BAT_VOL) && !bat_perc) || (!bat_perc && !READ_USB))
        bat_perc = 1;
    
    return (bat_perc/20) + ((bat_perc%20) ? 1:0);
}

/********************************************************************************************
* 函数名：Low_Battery_Handle
* 描  述：获取电池电量百分比 -> 剩余电量五等分(获取当前电量格数)
* 输  入：grid -> 当前电量格数（电量格数0 -> 低电量）
* 输  出：无
********************************************************************************************/
void Low_Battery_Handle(uint8_t crt_grid)
{
    uint32_t wait_time = 180000;
    static uint32_t lbh_tk;         // 低电量处理计时
    static uint8_t alarm_lb = 0;    // 低电量报警次数
    
	if(crt_grid)
        alarm_lb = 0;
    else
    {
        if(alarm_lb > 1)
            wait_time = 60000;
        
        if((System_Time_Wait(wait_time,lbh_tk) || !alarm_lb))    // 首次报警不等待
        {
            System_Time_Init(&lbh_tk);
            if(alarm_lb++ < 2)   // 低电量提示两次
                Beep_Ctr(BEEP_EVENT_LB);
            else    // 已报警两次，等待自动关机
                Sys_Shutdown();
        }
    }
}

/********************************************************************************************
* 函数名：Battery_Detect
* 描  述：刷新电池图标、百分比
* 输  入：ref -> true 立即刷新
*                false 等待刷新
********************************************************************************************/
void Battery_Detect(bool ref)
{
    uint8_t xpos;
//    static uint32_t bdt_tk;  // 计时的起始时间
    static uint8_t crt_grid = 6; // 当前电池电量显示格数
    
//    if(!(System_Time_Wait(1000,bdt_tk) || ref))
//        return;
//    System_Time_Init(&bdt_tk);  // 重置计时

    uint8_t grid = Battery_Get_Percent();
    
    if((crt_grid != grid) || ref)
    {
        if((sys_bits.run_md == RUN_MODE) && (crt_depth == DEPTH_HOME_1))
        {
            if(grid == 0)  // 电量不足
                OLED_DrawSingleBMP(232,1,19,10,(uint8_t *)&draw_lack_bat);    //显示电池电量不足图标
            else
            {
                if(ref || !crt_grid)    // 上次电池图标为电量不足
                    OLED_DrawSingleBMP(232,1,19,10,(uint8_t *)&draw_empty_bat);    //空电池图标
                
                for(uint8_t i = 0;i < grid;i++)
                {
                    xpos = 234 + i * 3;
                    if(xpos % 2)
                    {
                        OLED_Draw_Fill(xpos - 1,4,1,4,0x0F);
                        OLED_Draw_Fill(xpos + 1,4,1,4,0xF0);
                    }
                    else
                        OLED_Draw_Fill(xpos,4,1,4,0xFF);
                }
                for(uint8_t i = grid; i < 5;i++)
                {
                    xpos = 234 + i * 3;
                    if(xpos % 2)
                    {
                        OLED_Draw_Fill(xpos - 1,4,1,4,0x00);
                        OLED_Draw_Fill(xpos + 1,4,1,4,0x00);
                    }
                    else
                        OLED_Draw_Fill(xpos,4,1,4,0x00);
                }
            }
        }
        crt_grid = grid;
    }
    Low_Battery_Handle(crt_grid);    // 低电量处理
}

/********************************************************************************************
* 函数名：Power_Test
* 描述  ：电池功耗计算
********************************************************************************************/
void Power_Test(void)
{
	static uint32_t pw_tk = 0;
	
	if(System_Time_Wait(300000,pw_tk))
	{
		System_Time_Init(&pw_tk);
		sys_cfg.power_tk++;
		Save_Sys_Config();    //保存总剂量值
	}
}


