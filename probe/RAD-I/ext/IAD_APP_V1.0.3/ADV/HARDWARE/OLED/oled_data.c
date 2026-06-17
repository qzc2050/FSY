#include "oled_data.h"
#include "control.h"
#include "ui_menu.h"

//static __IO Data_Struct data_tp = {0};
__IO uint8_t rec_offset_addr[ALL_DATA_NUM] = {0};

/********************************************************************************************
* 函数名：Unit_Show
* 描述  ：刷新显示屏数据
* 输入  ：addr_val_x -> 剂量值x坐标, addr_val_y -> 剂量值y坐标, val_size -> 剂量值显示字体大小
				  addr_uint_x -> 单位x坐标,  addr_uint_y -> 单位y坐标,  uint_size -> 单位显示字体大小
          fdose -> 累计剂量 ，mode -> （0：主界面显示  1：剂量累计界面/历史记录界面显示 -> 左对齐）
* 说  明：以fdose作为显示，则fdose形参输入 ≥0
*         使用udata已有参数，则fdose形参输入 -1
********************************************************************************************/
void Unit_Show(uint8_t addr_val_x,uint8_t addr_val_y,uint8_t val_size,\
	           uint8_t addr_uint_x,uint8_t addr_uint_y,uint8_t uint_size,float fdose,uint8_t mode)
{
    uint8_t str[6];

    if(fdose >= 0)
        Float_To_DataUnit(fdose,RATE_SW);

    if((udata.day.rate_data < 1000) && !mode)  // if((rate_data / 100) < 10.0f)
        sprintf((char *)str, " %.2f", udata.day.rate_data / 100.0f);    //将数据转换为字符，方便OLED显示
    else
        sprintf((char *)str, "%.2f", udata.day.rate_data / 100.0f);
    
    if(mode)    // 剂量累计界面
    {
        if(udata.day.rate_data < 1000)  // if((rate_data / 100) < 10.0f)
        {
            OLED_Draw_Fill(addr_uint_x-6,addr_uint_y,18,val_size,0x00);
            OLED_ShowString(addr_val_x,addr_val_y,str,val_size);  //显示实时剂量率
            
            if(udata.day.rate_unit == UNIT_USV_H)
                OLED_ShowString(addr_uint_x,addr_uint_y,"uSv/h ",uint_size);
            else
                OLED_ShowString(addr_uint_x,addr_uint_y,"mSv/h ",uint_size);
            return;
        }
    }
    
    OLED_ShowString(addr_val_x,addr_val_y,str,val_size);  //显示实时剂量率
    
    if(udata.day.rate_unit == UNIT_USV_H)
        OLED_ShowString(addr_uint_x,addr_uint_y," uSv/h",uint_size);
    else
        OLED_ShowString(addr_uint_x,addr_uint_y," mSv/h",uint_size);
}

/********************************************************************************************
* 函数名：Dose_To_Str
* 描述  ：刷新显示屏数据
* 输入  ：data_type -> 数据类型（true -> 当日剂量累计/ false -> 总剂量累计）
* 输出  ：true -> 整数部分仅1位   fasle -> 整数部分有两位
* 调用  ：外部调用
********************************************************************************************/
bool Dose_To_Str(bool data_type)
{
	float temp = (data_type ? udata.day.sum_data : udata.dose.sum_data) / 100.0f;

    switch((data_type ? udata.day.sum_unit : udata.dose.sum_unit))
    {
        case UNIT_USV_H: sprintf(str_temp, "%.2f uSv ", temp);
            break;
        case UNIT_MSV_H: sprintf(str_temp, "%.2f mSv ", temp);
            break;
        case UNIT_SV_H: sprintf(str_temp, "%.2f Sv ", temp);
            break;
    }
    
    if(temp < 10.0f)
        return true;
    return false;
}

/********************************************************************************************
* 函数名：OLED_Dose_Show
* 描  述：刷新显示屏数据
* 输  入：addr_val_x  -> 剂量值x坐标,        addr_val_y  -> 剂量值y坐标  
            val_size  -> 剂量值显示字体大小        fdose -> 累计剂量        
* 输  出：无
* 说  明：以fdose作为显示，则fdose形参输入 ≥0
*         使用udata已有参数，则fdose形参输入 -1
* 调  用：外部调用
********************************************************************************************/
void OLED_Dose_Show(uint8_t addr_val_x,uint8_t addr_val_y,uint8_t val_size,float fdose)
{
    if(fdose >= 0)
        Float_To_DataUnit(fdose,CRT_DOSE_SW);

	Dose_To_Str(false);
	OLED_ShowString(addr_val_x,addr_val_y,(uint8_t *)str_temp,val_size);
}

/********************************************************************************************
* 函数名：Clr_Crt_Dose
* 描述  ：保存并清空当前累计总剂量值
********************************************************************************************/
void Clr_Crt_Dose(void)
{
    OLED_Clear();
    OLED_ShowChinese(96,24,"清除成功!",16);

    udata.dose.init_type = DOSE_TYPE;
    udata.dose.init_date = data_var.clr_date;
    udata.dose.clr_date = Date_Conv(Get_Date_uint());
    Float_To_DataUnit(data_var.crt_dose,CRT_DOSE_SW);
    
    STMDATAEEPROM_Write(DATA_BASE_ADDR + data_var.data_ofs_num * HIS_DATA_SIZE,(uint32_t *)(&udata),2);
    Data_Save_Cnt_Add();

    data_var.crt_dose = 0;
    data_var.clr_date = udata.dose.clr_date;
    STMDATAEEPROM_Write(CRT_DOSE_ADDR,(uint32_t *)(&data_var.crt_dose),2);
	
	/********************返回菜单****************/
    menu_func(NULL,MENU_0_BACK);
	/********************返回菜单****************/
}

/********************************************************************************************
* 函数名：Show_Date_Time
* 描述  ：复制时间到字符串数组，用于OLED显示历史记录
* 输入  ：sizey -> 字体大小
*           x/y -> 显示地址
*          date -> 实际时间
********************************************************************************************/
void Show_Date_Time(uint8_t x,uint8_t y,uint32_t date)
{
	sprintf(str_temp,"%02d/%02d/%02d",date / 10000,(date / 100) % 100,date % 100);
    OLED_ShowString(x,y,(uint8_t *)str_temp,16);
}

/*************************************************************************************************
* 函数名：Rec_Mode_Get_Valid_Page
* 描述  ：在限制读取记录范围的情况下获取有效读取页数，同时提取符合条件的数据在EEPROM中的偏移地址
* 输出  ：有效读取页数
*************************************************************************************************/
uint8_t Rec_Mode_Get_Valid_Page(void)
{
    int16_t valid_cnt = 0;
	uint16_t valid_page = 0;
	uint32_t read_addr = 0;
	
	while(data_var.rec_rg_offset != data_var.history_data_num)
	{
        read_addr = DATA_BASE_ADDR + data_var.rec_rg_offset * HIS_DATA_SIZE;
        if(sys_cfg.rec_cir)
		{
            read_addr += (data_var.data_ofs_num + ALL_DATA_NUM - data_var.history_data_num) * HIS_DATA_SIZE;
			
			if(read_addr > DATA_MAX_ADDR)   //超过可读取的地址上限，返回至基地址进行读取，类似于循环
				read_addr -= (ALL_DATA_NUM * HIS_DATA_SIZE);
		}
        
		STMDATAEEPROM_Read(read_addr,(uint32_t *)(&udata),2);
        
		// 101 用于排除日期小于2000/01/01的数据，理论上不会出现这种日期的数据(但由于每次初始化设备时，会设置日期并保存数据而出现，不过不影响)
		if((Date_Inconvert(udata.day.rec_date) < 101) || ((Date_Inconvert(udata.dose.init_date) == data_var.day_date) && udata.dose.init_type)\
            || (Date_Inconvert(udata.day.rec_date) > data_var.day_date)) //过滤掉日期小于2000/01/01的和日期大于等于当前系统日期的数据
            ;
		else
			rec_offset_addr[valid_cnt++] = data_var.rec_rg_offset;   //偏移最小从0开始

		data_var.rec_rg_offset++;
	}
    
	if(valid_cnt)
	{
        /**************** 调转历史记录数据 ****************/
//		for(uint8_t i = valid_cnt - 1;i < ALL_DATA_NUM;i--)    //将保存的偏移地址反序保存，便于将新数据显示在前
//            rec_offset_addr[valid_cnt-1-i] = rec_offset_addr_buf[i];
        
        uint8_t end = valid_cnt - 1;
        for(uint8_t start = 0;start < end;start++,end--)    //将保存的偏移地址反序保存，便于将新数据显示在前
        {
            uint8_t temp = rec_offset_addr[start];
            rec_offset_addr[start] = rec_offset_addr[end];
            rec_offset_addr[end] = temp;
        }
        
        /**************** 调转历史记录数据 ****************/
        
//		if(valid_cnt != ALL_DATA_NUM)                 // --------------------> 暂时屏蔽，貌似没用
//            rec_offset_addr[valid_cnt] = 0xFF-1;    //用于结束寻址
        
		valid_page = valid_cnt / 3;   //计算数据页数，3个数据一页
		if(valid_cnt % 3)      //每三个数据一页
			valid_page++;
	}
	else
		valid_page = 0;
	return valid_page;
}

/********************************************************************************************
* 函数名：His_Page_Home
* 描述  ：查看历史记录
* 输出  ：1：刷新OLED显示屏上的数据     0：不刷新OLED显示屏上的数据
********************************************************************************************/
void His_Page_Home(void)
{
    bool left_sta;
    uint16_t oled_hight;
	uint32_t read_addr = 0,ofs_read = 0xFFFFFFFF;
	
    crt_depth = DEPTH_HISTORY;
	OLED_Clear();
	
	if(data_var.history_data_num == 0)    //无数据保存
	{
		null_deal:
		OLED_ShowChinese(72,24,"未保存任何数据!\0",16);
        return;
	}
	else
	{
		if(sys_bits.rec_rg_prep == 0)   //未获取有效数据的个数和页数
		{
			sys_bits.rec_rg_prep = 1;
			data_var.rec_rg_valid_page = Rec_Mode_Get_Valid_Page();
			
			if(!data_var.rec_rg_valid_page)  //有数据保存，但是保存的数据不符合显示条件
				goto null_deal;
		}
	}

    for(uint8_t i = 0;i < 3;i++)
    {
        uint8_t read_ofs = i + data_var.crt_page * 3;
        if(((1+rec_offset_addr[read_ofs]) > data_var.history_data_num) || (read_ofs >= ALL_DATA_NUM))
        {
            OLED_ShowChinese(91,44,"最后一页!",16);
            break;
        }
        
        if(!sys_cfg.rec_cir)  //未保存一轮数据
            read_addr = DATA_BASE_ADDR + rec_offset_addr[read_ofs] * HIS_DATA_SIZE;
        else
            read_addr = DATA_BASE_ADDR + data_var.data_ofs_num * HIS_DATA_SIZE
                             + rec_offset_addr[read_ofs] * HIS_DATA_SIZE;
        
        if(ofs_read == read_addr)
            return;
        ofs_read = read_addr;
        
        if(read_addr > DATA_MAX_ADDR)
            read_addr -= (ALL_DATA_NUM * HIS_DATA_SIZE);
        
        STMDATAEEPROM_Read(read_addr,(uint32_t *)(&udata),2);

//        oled_hight = 4*(i+1)+i*16;
        oled_hight = 20 * i + 4;
        
        Show_Date_Time(0,oled_hight,Date_Inconvert(udata.day.rec_date));
        OLED_ShowChar(68,oled_hight,'-',16);
        if(udata.day.rec_type)     //每日累计的数据记录
        {
            left_sta = Dose_To_Str(true);
            OLED_ShowString(78,oled_hight,(uint8_t *)str_temp,16);
            
            uint8_t pos_ofs = 0;
            
            if(!left_sta)
                pos_ofs = 8;
            OLED_ShowChar(146 + pos_ofs,oled_hight,'-',16);
            Unit_Show(158 + pos_ofs,oled_hight,16,198 + pos_ofs,oled_hight,16,UDATA_DEF,1);
        }
        else     //当前累计总剂量值的数据记录
        {
            Show_Date_Time(78,oled_hight,Date_Inconvert(udata.dose.clr_date));
            OLED_ShowChar(146,oled_hight,'-',16);
            OLED_Dose_Show(158,oled_hight,16,UDATA_DEF);
        }
    }
}

/********************************************************************************************
* 函数名：His_Page_Up
* 描述  ：查看上一页历史记录
********************************************************************************************/
void His_Page_Up(void)
{
	if(data_var.rec_rg_valid_page)    //符合显示的页数不为0
	{	
		if((data_var.rec_rg_valid_page - 1) == 0)    //可显示页数只有一页
		{
			OLED_Clear();
			sys_bits.his_tip = 1;
			OLED_ShowChinese(96,24,"仅此一页!",16);
		}
		else if(data_var.crt_page == 0)    //当前显示页数为0
		{
			OLED_Clear();
			sys_bits.his_tip = 1;
			
			if(sys_bits.key_ls)   //S键长按，返回首页
			{
				sys_bits.key_ls = 0;
				OLED_ShowChinese(96,24,"处于首页!",16);
			}
			else
			{
				OLED_ShowChinese(96,24,"前往尾页!",16);
				data_var.crt_page = data_var.rec_rg_valid_page - 1;
			}
		}
		else
		{
			if(sys_bits.key_ls)   //S键长按，返回首页
			{
				OLED_Clear();
				sys_bits.key_ls = 0;
				sys_bits.his_tip = 1;
				data_var.crt_page = 0;
				OLED_ShowChinese(96,24,"前往首页!",16);
			}
			else
				data_var.crt_page--;
		}
		
		if(sys_bits.his_tip)
			return;
		
		His_Page_Home();     //显示对应的历史记录
	}
	else   //无任何数据，按下后刷新屏幕，给予反馈
    {
		OLED_Clear();
		OLED_ShowChinese(72,24,"未保存任何数据!\0",16);
	}
	
	crt_inft = HIS_PAGE_HOME;
}

/********************************************************************************************
* 函数名：His_Page_Down
* 描述  ：查看下一页历史记录
********************************************************************************************/
void His_Page_Down(void)
{
	uint16_t all_valid_page = 0;
	
	if(data_var.rec_rg_valid_page)    //符合显示的页数不为0
	{
        all_valid_page = data_var.rec_rg_valid_page - 1;
		
		if(all_valid_page == 0)    //可显示页数只有一页
		{
			OLED_Clear();
			sys_bits.his_tip = 1;
			OLED_ShowChinese(96,24,"仅此一页!",16);
		}
		else if(data_var.crt_page == all_valid_page)    //当前页数为最大可显示页数
		{
			OLED_Clear();
			sys_bits.his_tip = 1;
			
			if(sys_bits.key_ls)   //S键长按，返回尾页
			{
				sys_bits.key_ls = 0;
				OLED_ShowChinese(96,24,"处于尾页!",16);
			}
			else
			{
				data_var.crt_page = 0;
				OLED_ShowChinese(96,24,"前往首页!",16);
			}
		}
		else
		{
			if(sys_bits.key_ls)   //S键长按，返回尾页
			{
				OLED_Clear();
				sys_bits.his_tip = 1;
				sys_bits.key_ls = 0;
				data_var.crt_page = all_valid_page;
				OLED_ShowChinese(96,24,"前往尾页!",16);
			}
			else
				data_var.crt_page++;	
		}
		
		if(sys_bits.his_tip)
			return;
		
		His_Page_Home();
	}
	else{            //无任何数据，按下后刷新屏幕，给予反馈
		OLED_Clear();
		OLED_ShowChinese(72,24,"未保存任何数据!\0",16);
	}
	crt_inft = HIS_PAGE_HOME;
}

/********************************************************************************************
* 函数名：History_tip_keep
* 描述  ：提示信息维持1s计时
********************************************************************************************/
void History_tip_keep(void)
{
	static uint32_t tip_tk;
	
	if(sys_bits.his_tip == 1)
	{
        System_Time_Init(&tip_tk);
		sys_bits.his_tip = 2;
	}
	
	if(System_Time_Wait(1000,tip_tk))
	{
        menu_func(NULL,HIS_PAGE_HOME);
		sys_bits.his_tip = 0;
	}
}

/********************************************************************************************
* 函数名：Clr_Day_Data
* 描述  ：清空并保存当日数据
********************************************************************************************/
void Clr_Day_Data(void)
{
    OLED_Clear();
    OLED_ShowChinese(96,24,"清除成功!",16);
    Update_DayData_To_EEPROM(true);
    menu_func(NULL,MENU_2_BACK);
}

/********************************************************************************************
* 函数名：Adjust History
* 描述  ：调整历史记录（包括显示，下一条记录的保存位置[或者覆盖位置]）
* 输入  ：当前日期
********************************************************************************************/
void Adjust_History(uint32_t date_temp)
{
	int16_t delta_num;
	
	/* 调整历史数据保存 */
	if(date_temp != data_var.day_date)
	{
		delta_num = Adjust_Offset_Position(data_var.day_date);
        STMDATAEEPROM_Write(HISTORY_NUM_ADDR,(uint32_t *)(&data_var.history_data_num),1);
		
		if(delta_num != -ALL_DATA_NUM)
		{
			if(delta_num + data_var.history_data_num >= ALL_DATA_NUM)
				data_var.history_data_num = ALL_DATA_NUM;
			else
				data_var.history_data_num += delta_num;
		}
		else if(delta_num == (ALL_DATA_NUM + 1))
			data_var.history_data_num = ALL_DATA_NUM;
		else
			data_var.history_data_num = 0;
		
		STMDATAEEPROM_Write(HISTORY_NUM_ADDR,(uint32_t *)(&data_var.history_data_num),1);
	}
	else
		printf("无调整!\r\n");
	/* 调整历史数据保存 */
}
