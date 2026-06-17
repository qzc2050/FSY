#include "control.h"
#include "ui_menu.h"


/********************************************************************************************
* 函数名：OLED_SHOW_ARROW
* 描述  ：箭头显示
* 输入  : x,y 坐标
********************************************************************************************/
void OLED_SHOW_ARROW(uint8_t ADDR_x,uint8_t ADDR_y)
{
	OLED_DrawSingleBMP(ADDR_x,ADDR_y,6,12,(uint8_t *)&arrow);
}

/********************************************************************************************
* 函数名：OLED_Draw_Border_Line
* 描述  ：菜单界面显示边界线
* 输入  ：无
********************************************************************************************/
void OLED_Draw_Border_Line(void)
{
	OLED_Draw_Fill(0,17,128,1,0xFF);
}

/********************************************************************************************
* 函数名：USB_Detect
* 描述  ：USB接入检测（上电检测）
********************************************************************************************/
void USB_Detect(void)
{   
	key_ctr.up_tk = 0;
	if(READ_USB)    //非充电状态
	{
		Low_Battery_Judge(0,true);
        if(crt_depth == DEPTH_HOME_1)
			OLED_Draw_Fill(209,0,10,12,0x00);
	}
	else            //充电状态
	{
//        if(LPR_Time_Cnt != 0xFFFF)
            LPR_Time_Cnt = 120000;
		
		BLUE_LED_OFF();
        if(crt_depth == DEPTH_HOME_1)
			OLED_DrawSingleBMP(209,0,19,12,(uint8_t *)&usb_icon);
	}
}

/********************************************************************************************
* 函数名：Base_Oper
* 描述  ：OLED初始界面显示
********************************************************************************************/
void Base_Oper(void)
{
	STMDATAEEPROM_Read(TH_REAL_RATE_ADDR,(uint32_t *)(&sys_cfg.th_real_rate),6);
    STMDATAEEPROM_Read(HISTORY_NUM_ADDR,(uint32_t *)(&data_var.history_data_num),7);
    
	Set_Bright_Grade(0);
	Set_Sc_Extinct_Time(0);
	
//	DateTime_Refresh(1);
//	cheak_date(0);     // 删除，DateTime_Refresh()运行过
    menu_func(NULL,MENU_HOME_1);
}

/********************************************************************************************
* 函数名：menu_home_1
* 描述  ：OLED显示主界面1（实时剂量率、总剂量值）
********************************************************************************************/
void menu_home_1(void)
{
    crt_depth = DEPTH_HOME_1;
    
	OLED_Clear();
    ref_sta = true;
    
    if(!READ_USB)
		OLED_DrawSingleBMP(209,0,19,12,(uint8_t *)&usb_icon);
	OLED_ShowString(24,24,"REAL",16);
	OLED_ShowString(100,48,"DOSE:",16);
	Back_Timing_Mode();
	OLED_DrawSingleBMP(218,19,36,36,(uint8_t *)&radiation_icon);
}

/********************************************************************************************
* 函数名：menu_home_2
* 描述  ：OLED显示主界面2（实时剂量率阈值、总剂量阈值）
********************************************************************************************/
void menu_home_2(void)
{
    crt_depth = DEPTH_HOME_2;
    
	OLED_Clear();
    ref_sta = true;
	OLED_ShowChinese(0,7,"当日累计：",12);
	OLED_ShowChinese(0,26,"当日平均：",12);
	OLED_ShowChinese(0,45,"当日最高：",12);
	OLED_ShowChinese(160,17,"总累计剂量：",12);
}

/********************************************************************************************
* 函数名：Menu_Home
* 描述  ：主菜单显示
* 输出  ：1：刷新OLED显示屏上的数据     0：不刷新OLED显示屏上的数据
* 输入  ：（addr_x，addr_y）箭头的坐标
********************************************************************************************/
void Menu_Home(uint8_t addr_x,uint8_t addr_y)
{
	if(crt_depth != DEPTH_MENU_HOME)
	{
        crt_depth = DEPTH_MENU_HOME;
        
		OLED_Clear();
		sys_bits.rec_rg_prep = 0;
		data_var.crt_page = 0;
		data_var.rec_rg_offset = 0;
		
		OLED_DrawSingleBMP(23,18,28,28,(uint8_t *)&return_icon);
		OLED_SHOW_ARROW(addr_x,addr_y);
		
		OLED_ShowString(74,13,"1.",12);OLED_ShowChinese(86,13,"阈值设置",12);
		OLED_ShowString(165,13,"2.",12);OLED_ShowChinese(177,13,"系统设置",12);
		OLED_ShowString(74,39,"3.",12);OLED_ShowChinese(86,39,"历史记录",12);
		OLED_ShowString(165,39,"4.",12);OLED_ShowChinese(177,39,"清除累计",12);
	}
	else 
	{
		OLED_Draw_Fill(64,13,4,40,0x00);
		OLED_Draw_Fill(155,13,4,40,0x00);
		OLED_Draw_Fill(14,26,4,12,0x00);
		OLED_SHOW_ARROW(addr_x,addr_y);
	}
}

/********************************************************************************************
* 函数名：Menu_TH_Set
* 描述  ：剂量值阈值菜单显示
* 输入  ：（addr_x，addr_y）箭头的坐标
********************************************************************************************/
void Menu_TH_Set(uint8_t addr_x,uint8_t addr_y)
{	
	if(crt_depth != DEPTH_MENU_TH)
	{
        crt_depth = DEPTH_MENU_TH;
        
		OLED_Clear();
		OLED_SHOW_ARROW(addr_x,addr_y);
		
		OLED_ShowString(6,2,"1.",12);
		OLED_ShowChinese(18,2,"阈值设置",12);
		OLED_Draw_Border_Line();
		OLED_ShowString(27,34,"A",12);OLED_ShowChinese(37,34,"剂量率阈值",12);
		OLED_ShowString(134,34,"B",12);OLED_ShowChinese(144,34,"剂量阈值",12);
		OLED_DrawSingleBMP(227,34,12,12,(uint8_t *)&three_point);
	}
	else 
	{
		OLED_Draw_Fill(17,34,4,12,0x00);
		OLED_Draw_Fill(124,34,4,12,0x00);
		OLED_Draw_Fill(217,34,4,12,0x00);
		OLED_SHOW_ARROW(addr_x,addr_y);
	}
}

/********************************************************************************************
* 函数名：Menu_TH_Dose_Set
* 描述  ：剂量值阈值菜单显示
* 输入  ：（addr_x，addr_y）箭头的坐标
********************************************************************************************/
void Menu_TH_Dose_Set(uint8_t addr_x,uint8_t addr_y)
{
	if(crt_depth != DEPTH_MENU_TH_DOSE)
	{
        crt_depth = DEPTH_MENU_TH_DOSE;
        
		OLED_Clear();
		OLED_SHOW_ARROW(addr_x,addr_y);
		
		OLED_ShowString(6,2,"1.",12);
		OLED_ShowChinese(18,2,"阈值设置",12);
		OLED_ShowString(74,2,"-B",12);
		OLED_ShowChinese(90,2,"剂量阈值",12);
		
		OLED_Draw_Border_Line();
		OLED_ShowString(17,34,"a",12);OLED_ShowChinese(27,34,"当日累计阈值",12);
        OLED_ShowString(128,34,"b",12);OLED_ShowChinese(138,34,"总累计阈值",12);
		OLED_DrawSingleBMP(226,34,12,12,(uint8_t *)&three_point);
	}
	else 
	{
		OLED_Draw_Fill(7,34,4,12,0x00);
		OLED_Draw_Fill(118,34,4,12,0x00);
		OLED_Draw_Fill(216,34,4,12,0x00);
		OLED_SHOW_ARROW(addr_x,addr_y);
	}
}

/********************************************************************************************
* 函数名：Show_Clr_Crt_Dose
* 描述  ：询问是否清除当日数据
* 输入  ：（addr_x，addr_y）箭头的坐标
********************************************************************************************/
void Menu_Clr_Day_Data(uint8_t addr_x,uint8_t addr_y)
{
	if(crt_depth != DEPTH_CLR_DAY)
	{
        crt_depth = DEPTH_CLR_DAY;
        
		OLED_Clear();
		OLED_ShowChinese(64,10,"是否清除当日累计??",16); //英文? --> 一个字节（以两个英文符号代单个汉字）
		OLED_ShowChinese(74,38,"是",16); //单个汉字 --> 两个字节
		OLED_ShowChinese(166,38,"否",16);
	}
	
	OLED_Draw_Fill(64,38,4,12,0x00);
	OLED_Draw_Fill(156,38,4,12,0x00);
	OLED_SHOW_ARROW(addr_x,addr_y);
}

/********************************************************************************************
* 函数名：Menu_Clr_Crt_Dose
* 描述  ：询问是否清除当前累计总剂量值
* 输入  ：（addr_x，addr_y）箭头的坐标
********************************************************************************************/
void Menu_Clr_Crt_Dose(uint8_t addr_x,uint8_t addr_y)
{
	if(crt_depth != DEPTH_CLR_CRT)
	{
        crt_depth = DEPTH_CLR_CRT;
        
		OLED_Clear();
		OLED_ShowChinese(52,10,"是否清除总累计剂量??",16); //英文? --> 一个字节（以两个英文符号代单个汉字）
		OLED_ShowChinese(74,38,"是",16); //单个汉字 --> 两个字节
		OLED_ShowChinese(166,38,"否",16);
	}
	
	OLED_Draw_Fill(64,38,4,12,0x00);
	OLED_Draw_Fill(156,38,4,12,0x00);
	OLED_SHOW_ARROW(addr_x,addr_y);
}

/********************************************************************************************
* 函数名：Sys_Setting_Menu
* 描述  ：系统设置子菜单
* 输入  ：（addr_x，addr_y）箭头的坐标
********************************************************************************************/
void Menu_Sys_Set(uint8_t addr_x,uint8_t addr_y)
{
	if(crt_depth != DEPTH_MENU_SYS)
	{
        crt_depth = DEPTH_MENU_SYS;
        
		OLED_Clear();
		sys_bits.rec_rg_prep = 0;
		data_var.crt_page = 0;
		data_var.rec_rg_offset = 0;
		
		OLED_SHOW_ARROW(addr_x,addr_y);
		OLED_ShowString(6,2,"2.",12);
		OLED_ShowChinese(18,2,"系统设置",12);
		OLED_Draw_Border_Line();
		OLED_ShowString(21,24,"A",12);OLED_ShowChinese(31,24,"日期时间设置",12);
		OLED_ShowString(136,24,"B",12);OLED_ShowChinese(146,24,"显示设置",12);
		OLED_ShowString(21,44,"C",12);OLED_ShowChinese(31,44,"恢复默认设置",12);
		OLED_ShowString(136,44,"*",12);OLED_ShowChinese(146,44,"关于本机",12);
		OLED_DrawSingleBMP(223,34,12,12,(uint8_t *)&three_point);
	}
	else 
	{
		OLED_Draw_Fill(11,24,4,34,0x00);
		OLED_Draw_Fill(126,24,4,34,0x00);
		OLED_Draw_Fill(213,34,6,12,0x00);
		OLED_SHOW_ARROW(addr_x,addr_y);
	}
}

/********************************************************************************************
* 函数名：Menu_Display_Set
* 描述  ：显示设置子菜单
* 输入  ：（addr_x，addr_y）箭头的坐标
********************************************************************************************/
void Menu_Display_Set(uint8_t addr_x,uint8_t addr_y)
{
	if(crt_depth != DEPTH_MENU_DISPLAY)
	{
        crt_depth = DEPTH_MENU_DISPLAY;
        key_ctr.muti_long = false;  //关闭多次触发长按
        
		OLED_Clear();
		OLED_SHOW_ARROW(addr_x,addr_y);
		OLED_ShowString(6,2,"2.",12);
		OLED_ShowChinese(18,2,"系统设置",12);
		OLED_ShowString(74,2,"-A",12);
		OLED_ShowChinese(90,2,"显示设置",12);
		OLED_Draw_Border_Line();
		
		OLED_ShowString(28,34,"a",12);OLED_ShowChinese(38,34,"屏幕亮度",12);
		OLED_ShowString(122,34,"b",12);OLED_ShowChinese(132,34,"息屏时间",12);
		OLED_DrawSingleBMP(216,34,12,12,(uint8_t *)&three_point);
	}
	else
	{
		OLED_Draw_Fill(18,34,4,12,0x00);
		OLED_Draw_Fill(112,34,4,12,0x00);
		OLED_Draw_Fill(206,34,4,12,0x00);
		OLED_SHOW_ARROW(addr_x,addr_y);
	}
}

/********************************************************************************************
* 函数名：Progress_Bar_SC_Time
* 描述  ：熄屏时间提示
********************************************************************************************/
void Progress_Bar_SC_Time(uint32_t grade)
{
    OLED_Draw_Fill(110,44,18,12,0x00);
	switch(grade)
	{
		case 0:OLED_ShowString(116,44,"15 s",12);   break;
		case 1:OLED_ShowString(116,44,"30 s",12);   break;
        case 2:OLED_ShowString(113,44,"1 min",12);  break;
		case 3:OLED_ShowString(113,44,"2 min",12);  break;
		case 4:OLED_ShowString(113,44,"5 min",12);  break;
		case 5:OLED_ShowString(110,44,"10 min",12); break;
		case 6:OLED_ShowString(113,44,"0.5 h",12);  break;
		case 7:OLED_ShowString(119,44,"1 h",12);    break;
		case 8:OLED_ShowChinese(116,44,"永不",12);  break;
		default: break;
	}
}

/********************************************************************************************
* 函数名：Progress_Bar_Draw
* 描述  ：填充进度条
********************************************************************************************/
void Progress_Bar_Draw(void)
{
	uint8_t off_add = 0;
	uint32_t grade = 0;
	
	if(crt_inft == SET_CBR_INTF)
		grade = sys_cfg.bright_sz;
	else if(crt_inft == SET_COT_INTF)
	{
		off_add = 10;
		grade = sys_cfg.scr_off_idx;
		Progress_Bar_SC_Time(grade);
	}

	OLED_Draw_Fill(92,36-off_add,1,1,0x0F);
	OLED_Draw_Fill(94,36-off_add,1+grade*4,1,0xFF);
	OLED_Draw_Fill(92,37-off_add,3+grade*4,2,0xFF);
	
	OLED_Draw_Fill(90,37-off_add,1,6,0x0F);
	OLED_Draw_Fill(90,39-off_add,1,2,0xFF);
	OLED_Draw_Fill(92,39-off_add,4+grade*4,2,0xFF);
	OLED_Draw_Fill(98+grade*8,39-off_add,1,2,0xF0);
	
	OLED_Draw_Fill(92,41-off_add,3+grade*4,2,0xFF);
	OLED_Draw_Fill(94,43-off_add,1+grade*4,1,0xFF);
	OLED_Draw_Fill(92,43-off_add,1,1,0x0F);
}

/********************************************************************************************
* 函数名：Progress_Bar_Clear
* 描述  ：清除进度条
********************************************************************************************/
void Progress_Bar_Clear(void)
{
	uint8_t off_add = 0;
	uint32_t grade = 0;
	
	if(crt_inft == SET_CBR_INTF)
		grade = sys_cfg.bright_sz;
	else if(crt_inft == SET_COT_INTF)
	{
		off_add = 10;
		grade = sys_cfg.scr_off_idx;
		Progress_Bar_SC_Time(grade);
	}
	OLED_Draw_Fill(96+grade*8,36-off_add,4,1,0x00);
	OLED_Draw_Fill(98+grade*8,37-off_add,4,2,0x00);
	
	OLED_Draw_Fill(98+grade*8,39-off_add,5,2,0x00);
	OLED_Draw_Fill(98+grade*8,39-off_add,1,2,0xF0);
	
	OLED_Draw_Fill(98+grade*8,41-off_add,4,2,0x00);
	OLED_Draw_Fill(96+grade*8,43-off_add,4,1,0x00);
}

/********************************************************************************************
* 函数名：Draw_Bar
* 描  述：画出进度条
* 输  入：y位置
********************************************************************************************/
void Draw_Bar(uint8_t ypos)
{
    OLED_DrawSingleBMP(88,ypos,6,12,(uint8_t *)bar_left);
    OLED_DrawSingleBMP(160,ypos+1,6,10,(uint8_t *)bar_right);
    OLED_Draw_Fill(94,ypos,33,1,0xFF);
    OLED_Draw_Fill(94,ypos+11,33,1,0xFF);
}

/********************************************************************************************
* 函数名：set_bar_intf
* 描述  ：显示亮度进度条（等级）
********************************************************************************************/
void set_bar_intf(void)
{
    key_ctr.muti_long = true;
	if(crt_depth != DEPTH_SET_BAR)
	{
        crt_depth = DEPTH_SET_BAR;
        
		OLED_Clear();
		
		OLED_Draw_Border_Line();
		if(crt_inft == SET_CBR_INTF)
		{
			OLED_ShowChinese(6,2,"屏幕亮度",12);
            Draw_Bar(34);
			OLED_DrawSingleBMP(64,32,16,16,(uint8_t *)grade_icon2);
			OLED_DrawSingleBMP(174,32,17,17,(uint8_t *)grade_icon1);
		}
		else if(crt_inft == SET_COT_INTF)
		{
			OLED_ShowChinese(6,2,"息屏时间",12);
            Draw_Bar(24);
			OLED_DrawSingleBMP(64,22,16,16,(uint8_t *)grade_icon2+64);
			OLED_DrawSingleBMP(174,22,16,16,(uint8_t *)grade_icon2+96);
		}
		Progress_Bar_Draw();		
	}
}

/********************************************************************************************
* 函数名：Progress_Bar_Up
* 描述  ：提高亮度、延长熄屏时间
********************************************************************************************/
void Progress_Bar_Up(void)
{
    crt_inft = bef_inft;
    
	if(bef_inft == SET_CBR_INTF)
    {
        if(sys_cfg.bright_sz == 8)
            return;
		sys_cfg.bright_sz++;
		Set_Bright_Grade(1);
        
    }
	else if(bef_inft == SET_COT_INTF)
    {
		if(sys_cfg.scr_off_idx == 8)
            return;
		sys_cfg.scr_off_idx++;
		Set_Sc_Extinct_Time(1);
    }
	Progress_Bar_Draw();
}

/********************************************************************************************
* 函数名：Progress_Bar_Sub
* 描述  ：降低亮度、缩短熄屏时间
********************************************************************************************/
void Progress_Bar_Sub(void)
{
    crt_inft = bef_inft;
    
	if(bef_inft == SET_CBR_INTF)
    {
		if(!sys_cfg.bright_sz)
            return;
        sys_cfg.bright_sz--;
        Set_Bright_Grade(1);
    }
	else if(bef_inft == SET_COT_INTF)
    {
		if(!sys_cfg.scr_off_idx)
            return;
        sys_cfg.scr_off_idx--;
        Set_Sc_Extinct_Time(1);
    }
	Progress_Bar_Clear();
}

/********************************************************************************************
* 函数名：Set_Bright_Grade
* 输  入：write_sta -> 1: 保存设置至EEPROM, 0:不保存 
* 描  述：设置亮度并保存亮度等级
********************************************************************************************/
void Set_Bright_Grade(uint8_t write_sta)
{
	uint16_t bright_temp = 0x00;
	
	switch(sys_cfg.bright_sz){
		case 0:bright_temp = 0x00;break;
		case 1:bright_temp = 0x10;break;
		case 2:bright_temp = 0x20;break;
		case 3:bright_temp = 0x30;break;
		case 4:bright_temp = 0x40;break;
		case 5:bright_temp = 0x50;break;
		case 6:bright_temp = 0x60;break;
		case 7:bright_temp = 0x90;break;
		case 8:bright_temp = 0xFF;break;
		default:break;
	}
	OLED_WR_REG(0x81); //对比度设置
	OLED_WR_REG((uint16_t)bright_temp); /*    <-----------------------------------改变亮度    */
	
    if(write_sta)
        Save_Sys_Config();
}

/********************************************************************************************
* 函数名：Menu_Reset
* 描述  ：询问是否恢复出厂设置
* 输入  ：（addr_x，addr_y）箭头的坐标
********************************************************************************************/
void Menu_Reset(uint8_t addr_x,uint8_t addr_y)
{
	if(crt_depth != DEPTH_MENU_RST)
	{
        crt_depth = DEPTH_MENU_RST;
        
		OLED_Clear();
		OLED_ShowChinese(60,10,"确定恢复默认设置??",16);
		OLED_ShowChinese(74,38,"是",16);
		OLED_ShowChinese(166,38,"否",16);
	}
	
	OLED_Draw_Fill(64,40,4,12,0x00);
	OLED_Draw_Fill(156,40,4,12,0x00);
	OLED_SHOW_ARROW(addr_x,addr_y);
}

/********************************************************************************************
* 函数名：Set_Sc_Extinct_Time
* 描述  ：设置熄屏时间
********************************************************************************************/
void Set_Sc_Extinct_Time(uint8_t write_sta)
{
	switch(sys_cfg.scr_off_idx){
		case 0:LPR_Time_Cnt = 15000;break;    //15秒
		case 1:LPR_Time_Cnt = 30000;break;    //30秒
		case 2:LPR_Time_Cnt = 60000;break;    //1分钟
		case 3:LPR_Time_Cnt = 120000;break;   //2分钟
		case 4:LPR_Time_Cnt = 300000;break;   //5分钟
		case 5:LPR_Time_Cnt = 600000;break;   //10分钟
		case 6:LPR_Time_Cnt = 1800000;break;  //0.5小时
		case 7:LPR_Time_Cnt = 3600000;break; //1小时
		case 8:LPR_Time_Cnt = 0xFFFF;break;     //永不
		default:LPR_Time_Cnt = 60000;break;   //1分钟
	}

    if(write_sta)
        Save_Sys_Config();
}

/********************************************************************************************
* 函数名：page_up_down_icon
* 描述  ：显示翻页图标
********************************************************************************************/
void page_up_down_icon(uint8_t addr_x,uint8_t addr_y,uint8_t crt_page)
{
	if(crt_page > 1)//显示左三角
		OLED_DrawSingleBMP(addr_x,addr_y,3,5,(uint8_t *)&left_arrow);
	else
		OLED_Draw_Fill(addr_x,addr_y,4,5,0x00);       //清除标志
	
	if(crt_page < 3)//显示右三角
		OLED_DrawSingleBMP(addr_x+16,addr_y,3,5,(uint8_t *)&right_arrow);
	else
		OLED_Draw_Fill(addr_x+16,addr_y,4,5,0x00);    //清除标志
}

/********************************************************************************************
* 函数名：sys_info_1
* 描述  ：显示本机信息
********************************************************************************************/
void sys_info_1(void)
{
	if(bef_inft == SYS_INFO_2)
		OLED_Draw_Fill(0,18,128,46,0x00);
	else if(crt_depth == DEPTH_SYS_INFO_1)
		return;
	else 
		OLED_Clear();
	
    crt_depth = DEPTH_SYS_INFO_1;
	OLED_ShowChinese(6,2,"关于本机",12);
	OLED_Draw_Border_Line();
	
	OLED_ShowChinese(6,24,"名称：个人剂量计",12);
	OLED_ShowChinese(6,44,"型号：",12);
    OLED_ShowString(51,44,"IAD-I",12);
	
	page_up_down_icon(124,58,1);
}

/********************************************************************************************
* 函数名：sys_info_2
* 描述  ：显示本机信息
********************************************************************************************/
void sys_info_2(void)
{
    crt_depth = DEPTH_SYS_INFO_2;
	OLED_Draw_Fill(0,18,128,46,0x00);
	
	OLED_ShowChinese(6,24,"生产商：瑞多思医疗",12);
	
	OLED_ShowChinese(6,44,"序列号：",12);
    OLED_ShowString(62,44,(uint8_t *)sys_cfg.dev.u8_SN,12);
	page_up_down_icon(124,58,2);
}

/********************************************************************************************
* 函数名：sys_info_3
* 描述  ：显示本机信息
********************************************************************************************/
void sys_info_3(void)
{
	if(crt_depth == DEPTH_SYS_INFO_3)
		return;
	
    crt_depth = DEPTH_SYS_INFO_3;
	OLED_Draw_Fill(0,18,128,46,0x00);
	
	sprintf(str_temp,"%02x%08x",DEVICE_TYPE,SOFTWARE_VERSION);
	OLED_ShowChinese(6,24,"版本：",12);
    OLED_ShowString(48,24,(uint8_t *)str_temp,12);

	OLED_DrawSingleBMP(212,19,42,42,(uint8_t *)&quickmark);
	page_up_down_icon(124,58,3);
}





