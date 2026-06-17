#define _PCF8563_C_
#include "gpio.h"
#include "control.h"
#include "ui_menu.h"
#include "low_power_run.h"

//__IO char str_calendar[18] = {0};

extern struct time_type__ data_time;

//void pcf8563_iic_init(void)
//{
//	GPIO_InitTypeDef GPIO_InitStructure;

//	__HAL_RCC_GPIOB_CLK_ENABLE();

//	GPIO_InitStructure.Pin = GPIO_PIN_8|GPIO_PIN_9;
//	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP ;   //推挽输出
//	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
//	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);

//	PCF8563_IIC_SDA(1);
//	PCF8563_IIC_SCL(1);  
//}

//产生IIC起始信号
//void pcf8563_iic_start(void)
//{
//	 PCF8563_SDA_OUT();     //sda线输出
//	 PCF8563_IIC_SDA(1);	  	  
//	 PCF8563_IIC_SCL(1);
//	 HAL_Delay_us(4);
//	 PCF8563_IIC_SDA(0);//START:when CLK is high,DATA change form high to low 
//	 HAL_Delay_us(4);
//	 PCF8563_IIC_SCL(0);//钳住I2C总线，准备发送或接收数据 
//}	  

////产生IIC停止信号
//void pcf8563_iic_stop(void)
//{
//	 PCF8563_SDA_OUT();//sda线输出
//	 PCF8563_IIC_SCL(0);
//	 PCF8563_IIC_SDA(0);//STOP:when CLK is high DATA change form low to high
//	 HAL_Delay_us(4);
//	 PCF8563_IIC_SCL(1); 
//	 PCF8563_IIC_SDA(1);//发送I2C总线结束信号
//	 HAL_Delay_us(4);							   	
//}

//等待应答信号到来
//返回值：1，接收应答失败
//        0，接收应答成功
//uint8_t pcf8563_iic_wait_ack(void)
//{
//	 uint8_t ucErrTime=0;
//	 PCF8563_SDA_IN();      //SDA设置为输入  
//	 PCF8563_IIC_SDA(1);HAL_Delay_us(1);	   
//	 PCF8563_IIC_SCL(1);HAL_Delay_us(1);	 
//	 while(PCF8563_READ_SDA())
//	 {
//		ucErrTime++;
//		if(ucErrTime>250)
//		{
//			IIC_Stop();
//			return 1;
//		}
//	 }
//	 PCF8563_IIC_SCL(0);//时钟输出0 	   
//	 return 0;  
//} 

////产生ACK应答
//void pcf8563_iic_ack(void)
//{
//	 PCF8563_IIC_SCL(0);
//	 PCF8563_SDA_OUT();
//	 PCF8563_IIC_SDA(0);
//	 HAL_Delay_us(2);
//	 PCF8563_IIC_SCL(1);
//	 HAL_Delay_us(2);
//	 PCF8563_IIC_SCL(0);
//}

////不产生ACK应答		    
//void pcf8563_iic_nack(void)
//{
//	 PCF8563_IIC_SCL(0);
//	 PCF8563_SDA_OUT();
//	 PCF8563_IIC_SDA(1);
//	 HAL_Delay_us(2);
//	 PCF8563_IIC_SCL(1);
//	 HAL_Delay_us(2);
//	 PCF8563_IIC_SCL(0);
//}		

//IIC发送一个字节
//返回从机有无应答
//1，有应答
//0，无应答			  
void pcf8563_iic_send_byte(uint8_t txd)
{                        
	 uint8_t t;   
	 SDA_OUT(); 	    
	 IIC_SCL(GPIO_PIN_RESET);//拉低时钟开始数据传输
	 for(t=0;t<8;t++)
	 {              
//		IIC_SDA((txd&0x80)>>7);
			if(txd&0x80) IIC_SDA(GPIO_PIN_SET);
			else IIC_SDA(GPIO_PIN_RESET);
			txd<<=1; 	  
			HAL_Delay_us(2);   //对TEA5767这三个延时都是必须的
			IIC_SCL(GPIO_PIN_SET);
			HAL_Delay_us(2); 
			IIC_SCL(GPIO_PIN_RESET);	
			HAL_Delay_us(2);
	 }	 
} 	    

//读1个字节，ack=1时，发送ACK，ack=0，发送nACK   
uint8_t pcf8563_iic_read_byte(unsigned char ack)
{
	 unsigned char i,receive=0;
	 SDA_IN();//SDA设置为输入
	 for(i=0;i<8;i++ )
	 {
		IIC_SCL(GPIO_PIN_RESET); 
		HAL_Delay_us(2);
		IIC_SCL(GPIO_PIN_SET);
		receive<<=1;
		if(READ_SDA)receive++;   
		HAL_Delay_us(1); 
	 }					 
	 if (!ack)
		IIC_NAck();//发送nACK
	 else
		IIC_Ack(); //发送ACK   
	 return receive;
}

uint8_t pcf8563_send_byte(uint8_t reg, uint8_t reg_val)
{
	//Reg 是寄存器的地址
	//RegVal 是要写进寄存器的值
	//0 成功 1  失败
	IIC_Start();
	pcf8563_iic_send_byte(PCF8563_WRITE_ADDR);
	if(IIC_Wait_Ack())
		return 1;
	pcf8563_iic_send_byte(reg);
	if(IIC_Wait_Ack())
		return 1;
	pcf8563_iic_send_byte(reg_val);
	if(IIC_Wait_Ack())
		return 1;
	IIC_Stop();
	return 0;
}

//Reg 要读的寄存器的地址
//RegVal 读出来的数据保存的地址
//0 成功  1失败
uint8_t pcf8563_read_byte(uint8_t reg)
{
	uint8_t re_gval = 0;

	IIC_Start();
	pcf8563_iic_send_byte(PCF8563_WRITE_ADDR);
	if(IIC_Wait_Ack())
		return 0xff;
	pcf8563_iic_send_byte(reg);
	if(IIC_Wait_Ack())
		return 0xff;
	IIC_Start();
	pcf8563_iic_send_byte(PCF8563_READ_ADDR);
	if(IIC_Wait_Ack())
		return 0xff;
	re_gval = pcf8563_iic_read_byte(0);
	//I2C_SendNoAck();
	IIC_Stop();
	return re_gval;
}

/******************************************************
函数名：GetTime
功能：	  获取8563时间		
参数：    ClockTimeType
*/
uint8_t pcf8563_get_time(struct time_type__ *p)
{
	static struct time_type__ local_time;

    //读数必须去掉无效位，防止影响正常数据
	local_time.year   = pcf8563_read_byte(0x0A); //写年
	local_time.month  = pcf8563_read_byte(0x09) & 0x1F; //写月
//	local_time.Week   = read_8563(0x08) & 0x07; //写周
	local_time.day    = pcf8563_read_byte(0x07) & 0x3F; //写日
	local_time.hour   = pcf8563_read_byte(0x06) & 0x3F; //写时
	local_time.minute = pcf8563_read_byte(0x05) & 0x7F; //写分
	local_time.second = pcf8563_read_byte(0x04) & 0x7F; //写秒


	p->year   = local_time.year;
	p->month  = local_time.month;
	p->week   = local_time.week;
	p->day    = local_time.day;
	p->hour   = local_time.hour;
	p->minute = local_time.minute;
	p->second = local_time.second;

	return 0;
}

//******************************************************
//函数名：?aSetTime
//功能：    设定8563时间				
//参数：    ClockTimeType
//*/
void pcf8563_set_time(struct time_type__ *p)
{
	pcf8563_send_byte(0x0A, p->year);
	pcf8563_send_byte(0x09, p->month);
//	pcf8563_send_byte(0x08, 0);
	pcf8563_send_byte(0x07, p->day);
	pcf8563_send_byte(0x06, p->hour); 
	pcf8563_send_byte(0x05, p->minute); 
	pcf8563_send_byte(0x04,  p->second);
} 

void pcf8563_set_cap(uint8_t val)
{
	uint8_t reg_val = 0;
	
	reg_val = pcf8563_read_byte(0x00);
	
	if(val)
		reg_val |= val; 
	else
		reg_val &= ~(0x01);
	
	pcf8563_send_byte(0x00, reg_val);
} 

uint8_t BcdToHex(uint8_t bcd)  //BCD码转16进制
{
	uint8_t hex;
	hex  = (bcd >> 4) * 10;
	hex += (0x0F & bcd);
	return (hex);
}

uint8_t HexToBcd(uint8_t hex)  //16进制转BCD码
{
	uint8_t bcd;
	bcd = hex/10;
	bcd = bcd<<4;
	bcd |= hex % 10;
	return (bcd);	
}

void pcf8563_set_cur_time(struct time_type__ *tTime)
{	
	struct time_type__ times;	
	times.year   = HexToBcd((uint8_t)tTime->year);
	times.month  = HexToBcd((uint8_t)tTime->month);
	times.day    = HexToBcd((uint8_t)tTime->day);
	times.hour   = HexToBcd((uint8_t)tTime->hour);
	times.minute = HexToBcd((uint8_t)tTime->minute);
	times.second = HexToBcd((uint8_t)tTime->second);
	pcf8563_set_time(&times);  
}

void pcf8563_get_cur_time(struct time_type__ *tTime)
{
	struct time_type__ times;

	pcf8563_get_time(&times);
	tTime->year   = BcdToHex(times.year);
	tTime->month  = BcdToHex(times.month);
	tTime->day    = BcdToHex(times.day);
	tTime->hour   = BcdToHex(times.hour);
	tTime->minute = BcdToHex(times.minute);
	tTime->second = BcdToHex(times.second);

	//断电后没有设置的情况下，可能读出来的秒>60
	if (tTime->year >= 100)
	{
		tTime->year %= 100;
	}

	if (tTime->month > 12)
	{
		tTime->month %= 12;
	}

	if (tTime->day > 31)
	{
		tTime->day %= 31;
	}

	if (tTime->hour >= 24)
	{
		tTime->hour %= 24;
	}

	if (tTime->minute >= 60)
	{
		tTime->minute %= 60;
	}

	if (tTime->second > 60)
	{
		tTime->second %= 60;
	}
} 

void pcf8563_init(void)
{
	
}

/********************************************************************************************
* 函数名：Get_Date_uint
* 描  述：获取年月日的整数，例如：24年12月11日 -> 241211
* 输  入：time_type__结构体变量
********************************************************************************************/
uint32_t Get_Date_uint(void)
{
	return (data_time.year * 10000 + data_time.month * 100 + data_time.day);
}

/********************************************************************************************
* 函数名：cheak_date
* 描述  ：检测日期，若为设备时间的第二天，则保存前一天的数据
********************************************************************************************/
void cheak_date(uint8_t sta)
{
	uint32_t date = Get_Date_uint();

	if(data_var.day_date != date)
	{
		if(sta && date)
			Flash_Save_Day_Data();   //保存并清空每日数据
		data_var.day_date = date;
		STMDATAEEPROM_Write(DAY_DATE_ADDR,(uint32_t *)(&data_var.day_date),1);

		Update_DayData_To_EEPROM(true);
	}
}

/********************************************************************************************
* 函数名：DateTime_Refresh
* 描  述：刷新屏幕上的日期时间
* 输  入：upd -> 1 立即刷新屏幕
                 0 等待至下一分钟
********************************************************************************************/
void DateTime_Refresh(bool ref)
{
	static uint8_t minute = 100;
	
//	pcf8563_get_cur_time(&data_time);
    LPR_Critical_Execute(pcf8563_get_cur_time,&data_time);

	if((minute != data_time.minute) || ref)
	{
		cheak_date(1);
		minute = data_time.minute;
        
		if((sys_bits.run_md != RUN_MODE) || (crt_depth != DEPTH_HOME_1))
            return;

        sprintf(str_temp,"20%02d/%02d/%02d %02d:%02d",data_time.year,data_time.month,data_time.day,\
                                                data_time.hour,data_time.minute);
		
//		if((crt_inft == MENU_HOME_1) || (crt_inft == TIMING_RUN) || (crt_inft == TIMING_SW))
		OLED_ShowString(2,1,(uint8_t *)str_temp,12);
//		printf("时钟%d年%d月%d日 星期:%d   %d:%d:%d\r\n", data_time.year + 2000, data_time.month, data_time.day, data_time.week, data_time.hour, data_time.minute, data_time.second);
	}
}
