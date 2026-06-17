#define _POWER_CTR_C_

#include "gpio.h"
#include <stdio.h>
#include "power_ctr.h"

#define MAX17048_ATHD_VAL  0x16     //设置低电量报警阈值


//初始化IIC
void POWER_CTR_IIC_Init(void)
{					     
	GPIO_InitTypeDef GPIO_InitStructure;
	__HAL_RCC_GPIOB_CLK_ENABLE();	//使能GPIOB时钟
	   
	GPIO_InitStructure.Pin = GPIO_PIN_8|GPIO_PIN_9;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP ;   //推挽输出
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	POWER_CTR_IIC_SCL(GPIO_PIN_SET);
	POWER_CTR_IIC_SDA(GPIO_PIN_SET); 	//PB8,PB9 输出高
	
	HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1, 0);
	HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
	
//	MAX17048_Compensation(0x98);
//	MAX17048_Compensation(0x97);
//	MAX17048_STATUS_INIT();
//	MAX17048_SET_ATHD(MAX17048_ATHD_VAL);   //设置低电量报警阈值
//	MAX17048_EN_SOC_ALERT();
}
//产生IIC起始信号
void POWER_CTR_IIC_Start(void)
{
	POWER_CTR_SDA_OUT();     //sda线输出
	POWER_CTR_IIC_SDA(GPIO_PIN_SET);	  	  
	POWER_CTR_IIC_SCL(GPIO_PIN_SET);
	HAL_Delay_us(4);
 	POWER_CTR_IIC_SDA(GPIO_PIN_RESET);//START:when CLK is high,DATA change form high to low 
	HAL_Delay_us(4);
	POWER_CTR_IIC_SCL(GPIO_PIN_RESET);//钳住I2C总线，准备发送或接收数据 
}	  
//产生IIC停止信号
void POWER_CTR_IIC_Stop(void)
{
	POWER_CTR_SDA_OUT();//sda线输出
	POWER_CTR_IIC_SCL(GPIO_PIN_RESET);
	POWER_CTR_IIC_SDA(GPIO_PIN_RESET);//STOP:when CLK is high DATA change form low to high
 	HAL_Delay_us(4);
	POWER_CTR_IIC_SCL(GPIO_PIN_SET); 
	POWER_CTR_IIC_SDA(GPIO_PIN_SET);//发送I2C总线结束信号
	HAL_Delay_us(4);							   	
}


//等待应答信号到来
//返回值：1，接收应答失败
//        0，接收应答成功
uint8_t POWER_CTR_IIC_Wait_Ack(void)
{
	uint16_t ucErrTime=0;
	POWER_CTR_SDA_IN();      //SDA设置为输入  
	POWER_CTR_IIC_SDA(GPIO_PIN_SET);HAL_Delay_us(1);	   
	POWER_CTR_IIC_SCL(GPIO_PIN_SET);HAL_Delay_us(1);	 
	while(POWER_CTR_READ_SDA)
	{
		ucErrTime++;
		if(ucErrTime>250)
		{
			printf("应答失败！");
			POWER_CTR_IIC_Stop();
			return 1;
		}
	}
	POWER_CTR_IIC_SCL(GPIO_PIN_RESET);//时钟输出0 	   
	return 0;  
} 
//产生ACK应答
void POWER_CTR_IIC_Ack(void)
{
	POWER_CTR_IIC_SCL(GPIO_PIN_RESET);
	POWER_CTR_SDA_OUT();
	POWER_CTR_IIC_SDA(GPIO_PIN_RESET);
	HAL_Delay_us(2);
	POWER_CTR_IIC_SCL(GPIO_PIN_SET);
	HAL_Delay_us(2);
	POWER_CTR_IIC_SCL(GPIO_PIN_RESET);
  //HAL_Delay_us(2);
}
//不产生ACK应答		    
void POWER_CTR_IIC_NAck(void)
{
	POWER_CTR_IIC_SCL(GPIO_PIN_RESET);
	POWER_CTR_SDA_OUT();
	POWER_CTR_IIC_SDA(GPIO_PIN_SET);
	HAL_Delay_us(2);
	POWER_CTR_IIC_SCL(GPIO_PIN_SET);
	HAL_Delay_us(2);
	POWER_CTR_IIC_SCL(GPIO_PIN_RESET);
  //HAL_Delay_us(2);
}	

//IIC发送一个字节
//返回从机有无应答
//1，有应答
//0，无应答			  
uint8_t POWER_CTR_IIC_Send_Byte(uint8_t txd)
{                        
    uint8_t t;   
    POWER_CTR_SDA_OUT(); 	    
    POWER_CTR_IIC_SCL(GPIO_PIN_RESET);//拉低时钟开始数据传输
    for(t=0;t<8;t++)
    {              
        //IIC_SDA=(txd&0x80)>>7;
		if(txd&0x80) POWER_CTR_IIC_SDA(GPIO_PIN_SET);
		else POWER_CTR_IIC_SDA(GPIO_PIN_RESET);
		txd<<=1; 	  
		HAL_Delay_us(2);   //对TEA5767这三个延时都是必须的
		POWER_CTR_IIC_SCL(GPIO_PIN_SET);
		HAL_Delay_us(2); 
		POWER_CTR_IIC_SCL(GPIO_PIN_RESET);	

    }	 
  return POWER_CTR_IIC_Wait_Ack();
} 	    
//读1个字节，ack=1时，发送ACK，ack=0，发送nACK   
uint8_t POWER_CTR_IIC_Read_Byte(unsigned char ack)
{
	unsigned char i,receive=0;
  POWER_CTR_IIC_SDA(GPIO_PIN_SET);
	POWER_CTR_SDA_IN();//SDA设置为输入
  for(i=0;i<8;i++ )
	{  
    POWER_CTR_IIC_SCL(GPIO_PIN_SET);  
		HAL_Delay_us(1);
    receive<<=1;
    if(POWER_CTR_READ_SDA) receive |= 0X01;     
    HAL_Delay_us(1);
		POWER_CTR_IIC_SCL(GPIO_PIN_RESET);
    HAL_Delay_us(2);
      
   }		

    if (!ack)
        POWER_CTR_IIC_NAck();//发送nACK
    else
        POWER_CTR_IIC_Ack(); //发送ACK   0
    return receive;
}

// /**
//  * @brief  读取剩余电量
//  * @note   
//  * @retval 
//  */
uint16_t max17048_get_permille(void)
{
    uint16_t p_value;

	// memoryWrite[0] = REGISTER_SOC;
	
    // if (write_with_address(MAX17048_ADDRESS, memoryWrite, 1) != 1)
    //     return 0;
    // if (read_with_address(MAX17048_ADDRESS, memoryRead, 2) != 2)
    //     return 0;

    // uint16_t value = ((uint16_t)memoryRead[0] << 8) | memoryRead[1];
    // // remove rounding error when converting percent to per mille
    // if (value > 100 * 256)
    // {
    //     value = 100 * 256;
    // }
    // *per = ((float)value / 256.0f);
    // return 1;

    uint8_t buf[2];

    max17048_read_rag(REGISTER_SOC, buf, 2);

    p_value = ((uint16_t)buf[0] << 8) | buf[1];

    if (p_value > (100 * 256))
    {
        p_value = 100 * 256;
    }

    return (p_value / 256);

}

// /**
//  * @brief  读取电池电压。单位MV
//  * @note   
//  * @param  callback: 
//  * @retval None
//  */
uint8_t max17048_get_millivolt(float *volt)
{
    uint8_t buf[2];

    max17048_read_rag(REGISTER_VCELL, buf, 2);

    uint16_t value = ((uint16_t)buf[0] << 8) | buf[1];

    *volt = (((float)value) * 78.125f / 1000.0f);

    return 1;
}

/**
 * @brief  获取芯片版本
 * @note   
 * @retval 
 */
uint16_t max17048_get_verison(void)
{
    uint8_t buf[2];

    max17048_read_rag(REGISTER_VRESET_ID, buf, 2);

    return ((uint16_t)buf[0] << 8) + buf[1];
}

// /**
//  * @brief  读取配置寄存器
//  * @note   
//  * @param  *config: 
//  * @retval 
//  */
uint16_t max17048_get_config(void)
{
    uint8_t buf[2];

    max17048_read_rag(REGISTER_CONFIG, buf, 2);

    return ((uint16_t)buf[0] << 8) + buf[1];
}

// /**
//  * @brief  读取状态寄存器
//  * @note   
//  * @param  *config: 
//  * @retval 
//  */
uint16_t max17048_get_status(void)
{
    uint8_t buf[2];

    max17048_read_rag(REGISTER_STATUS, buf, 2);

    return ((uint16_t)buf[0] << 8) + buf[1];
}

// /**
//  * @brief  
//  * @note   
//  * @param  *config: 
//  * @retval 
//  */
// uint8_t max17048_get_valrt(uint16_t *val)
// {
//     return max_getRegister(REGISTER_VALRT, val);
// }

// /**
//  * @brief  
//  * @note   
//  * @param  *val: 
//  * @retval 
//  */
// uint8_t max17048_get_status(uint16_t *val)
// {
//     return max_getRegister(REGISTER_STATUS, val);
// }


uint16_t max17048_get_mode(void)
{
    uint8_t buf[2];

    max17048_read_rag(REGISTER_MODE, buf, 2);

    return ((uint16_t)buf[0] << 8) + buf[1];
}



uint8_t max17048_write_rag(max_register_t reg, uint8_t *p_data, uint8_t len)
{
  uint8_t ret=0;
	uint8_t i; 

  POWER_CTR_IIC_Start();
	POWER_CTR_IIC_Send_Byte(MAX17048_WRITE_ADDRESS);
//	POWER_CTR_IIC_Wait_Ack();
	POWER_CTR_IIC_Send_Byte(reg);
//	POWER_CTR_IIC_Wait_Ack();
	
	// POWER_CTR_IIC_Send_Byte(p_data >> 8);
	// POWER_CTR_IIC_Wait_Ack();
    // POWER_CTR_IIC_Send_Byte(p_data);
	// POWER_CTR_IIC_Wait_Ack();

    for(i = 0; i < len; i++)
	{	   
    	ret = POWER_CTR_IIC_Send_Byte(p_data[i]);  	//发数据
//		POWER_CTR_IIC_Wait_Ack();
		if(ret)break;  
	}

	POWER_CTR_IIC_Stop();

    return ret; 
}


void max17048_read_rag(max_register_t reg, uint8_t *p_data, uint8_t len)
{
    uint8_t i; 

	POWER_CTR_IIC_Start();
	POWER_CTR_IIC_Send_Byte(MAX17048_WRITE_ADDRESS);  //函数结束时放回应答信号，再次等待应答信号会出错
	POWER_CTR_IIC_Send_Byte(reg);
	
	POWER_CTR_IIC_Start();
	POWER_CTR_IIC_Send_Byte(MAX17048_READ_ADDRESS);

    for(i = 0; i < len; i++)
	{	   
    	p_data[i] = POWER_CTR_IIC_Read_Byte(i==(len-1)?0:1); //发数据	  
	} 

	POWER_CTR_IIC_Stop();
}


/*============================================================================*/
void MAX17048_Compensation(uint8_t RComp)
{
    uint8_t buf[2];
    uint16_t data = max17048_get_config();
    data &= 0x00FF;
    data |= RComp << 8;
    //max17048_write_rag(MAX17048_CONFIG, data, 2);

    buf[0] = data;
    buf[1] = data >> 8;
    max17048_write_rag(REGISTER_CONFIG, buf, 2);
}

/********************************************************************************************
* 函数名：MAX17048_POR
* 描述  ：上电复位
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void MAX17048_POR(void)
{
    uint8_t buf[2];
    uint16_t data = 0;
	
    data = 0x5400;
    
    buf[0] = data;
//	  printf("A:%#x\r\n",buf[0]);
    buf[1] = data >> 8;
//	  printf("B:%#x\r\n",buf[1]);
    max17048_write_rag(REGISTER_CMD, buf, 2);
//	  MAX17048_QStart();
}

/*============================================================================*/
void MAX17048_SleepEnable(void)
{
//   uint16_t value = MAX17048_Read(Obj, MAX17048_MODE, 2);
//   MAX17048_Write(Obj, MAX17048_MODE,(value | (0x0001<<MAX17048_MODE_EN_SLEEP_BIT)), 2);
    #define MAX17048_MODE_EN_SLEEP_BIT	        ( 13 )
    uint16_t value = max17048_get_mode();
    uint8_t buf[2];

    value = (value | (0x0001 << MAX17048_MODE_EN_SLEEP_BIT));

    buf[0] = value;
    buf[1] = value >> 8;
    max17048_write_rag(REGISTER_MODE, buf, 2);

}


/*============================================================================*/
void MAX17048_Sleep(uint8_t On_Off)
{
#define MAX17048_CONFIG_SLEEP_BIT	        	( 7 )
   uint16_t value = max17048_get_config();
   uint8_t buf[2];
  
    if(On_Off)
    {
        //MAX17048_Write(Obj, MAX17048_CONFIG, (value | (0x0001<<MAX17048_CONFIG_SLEEP_BIT)), 2);
        value = (value | (0x0001 << MAX17048_CONFIG_SLEEP_BIT));
    }
    else
    {
        //MAX17048_Write(Obj, MAX17048_CONFIG, (value & ~(0x0001<<MAX17048_CONFIG_SLEEP_BIT)), 2);
        value = (value & ~(0x0001 << MAX17048_CONFIG_SLEEP_BIT));
    }

    buf[0] = value;
    buf[1] = value >> 8;
    max17048_write_rag(REGISTER_CONFIG, buf, 2);
}


/*============================================================================*/
void MAX17048_QStart(void)
{
	#define MAX17048_MODE_QUICK_START_BIT	        ( 14 )
//   uint16_t value = MAX17048_Read(Obj, MAX17048_MODE, 2);
//   MAX17048_Write(Obj, MAX17048_MODE,(value | (0x0001<<MAX17048_MODE_QUICK_START_BIT)), 2);
    uint8_t buf[2];
    uint16_t value = max17048_get_mode();

    value = (value | (0x0001 << MAX17048_MODE_QUICK_START_BIT));

    buf[0] = value;
    buf[1] = value >> 8;
    max17048_write_rag(REGISTER_MODE, buf, 2);
}

//    MAX17048_Compensation(MAX17048_RCOMP0);
//    MAX17048_SleepEnable();
//    MAX17048_Sleep(0);
//    MAX17048_QStart();

/*********使能SOC充电检测**********/
//void MAX17048_EN_SOC_ALERT(void)
//{
//	uint8_t buf[2];
//	uint16_t data = max17048_get_config();
//	printf("%#x\r\n",data);
//	data &= 0xFF9F;
//	data |= 0x0040;
//	
//	data |= 0x0020;
//	//max17048_write_rag(MAX17048_CONFIG, data, 2);

//	buf[0] = data;
//	buf[1] = data >> 8;
//	max17048_write_rag(REGISTER_CONFIG, buf, 2);
//	data = max17048_get_config();
//	printf("%#x\r\n",data);
//}

/*============================================================================*/
/*(32 - ATHD)% (e.g., 00000b → 32%, 00001b → 31%, 
                      00010b → 30%, 11111b → 1%) */     //输入0x16时，电量剩余10%时报警
void MAX17048_SET_ATHD(uint8_t ATHD)     
{
    uint8_t buf[2];
    uint16_t data = max17048_get_config();
    data &= 0xFFEF;
    data |= ATHD;
    //max17048_write_rag(MAX17048_CONFIG, data, 2);

    buf[0] = data;
    buf[1] = data >> 8;
    max17048_write_rag(REGISTER_CONFIG, buf, 2);
}


void MAX17048_STATUS_INIT(void)
{
	uint8_t buf[2];
	uint16_t data = max17048_get_status();
	uint16_t temp = 0;
	data &= 0x80FF;
	//max17048_write_rag(MAX17048_CONFIG, data, 2);

	buf[0] = data;
	buf[1] = data >> 8;
	max17048_write_rag(REGISTER_STATUS, buf, 2);
	
	temp = max17048_get_status();
	printf("状态寄存器数值：%#x\r\n",temp);
}

void MAX17048_CLR_ALERT(uint16_t bit)
{
	uint8_t buf[2];
	uint16_t data = max17048_get_status();
	uint16_t temp = 0;
	data &= ~bit;
	//max17048_write_rag(MAX17048_CONFIG, data, 2);

	buf[0] = data;
	buf[1] = data >> 8;
	max17048_write_rag(REGISTER_STATUS, buf, 2);
	
	temp = max17048_get_status();
	printf("状态寄存器数值：%#x\r\n",temp);
	temp &= bit;
	printf("清除操作后该位数值：%d\r\n",temp);
}

uint16_t status_index = 0;

uint16_t MAX17048_DEAL_STATUS(void)
{
	uint16_t status_val = 0;
	
	status_val = max17048_get_status();
	status_val = status_val >> 8;
	
	if((status_val &= 0x20) == 1)
		status_index = MAX17048_CHARGE_STA;   //充电状态
//	else if((status_val &= 0x10) == 1)
//		status_index = MAX17048_ATHD_STA;   //电压电量小于CONFIG.ATHD设置的百分比
//	if((status_val &= 0x02) == 1)
//		status_index = 3;   //电压大于ALRT.VALRTMAX
//	if((status_val &= 0x04) == 1)
//		status_index = 4;   //电压小于ALRT.VALRTMIN
	printf("触发指令：%#x\r\n",status_index);
	return status_index;
}



