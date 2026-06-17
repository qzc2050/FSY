#include "control.h"

//初始化IIC
void IIC_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};
    
	__HAL_RCC_GPIOB_CLK_ENABLE();	//使能GPIOB时钟
	
	GPIO_InitStructure.Pin = I2C1_SCL_Pin|I2C1_SDA_Pin;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP ;   //推挽输出
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	IIC_SCL(GPIO_PIN_SET);
	IIC_SDA(GPIO_PIN_SET); 	//PB10,PB11 输出高
}
//产生IIC起始信号
void IIC_Start(void)
{
	SDA_OUT();     //sda线输出
	IIC_SDA(GPIO_PIN_SET);	  	  
	IIC_SCL(GPIO_PIN_SET);
	HAL_Delay_us(4);
 	IIC_SDA(GPIO_PIN_RESET);//START:when CLK is high,DATA change form high to low 
	HAL_Delay_us(4);
	IIC_SCL(GPIO_PIN_RESET);//钳住I2C总线，准备发送或接收数据 
}	  
//产生IIC停止信号
void IIC_Stop(void)
{
	SDA_OUT();//sda线输出
	IIC_SCL(GPIO_PIN_RESET);
	IIC_SDA(GPIO_PIN_RESET);//STOP:when CLK is high DATA change form low to high
 	HAL_Delay_us(4);
	IIC_SCL(GPIO_PIN_SET); 
	IIC_SDA(GPIO_PIN_SET);//发送I2C总线结束信号
	HAL_Delay_us(4);							   	
}
//等待应答信号到来
//返回值：1，接收应答失败
//        0，接收应答成功
uint8_t IIC_Wait_Ack(void)
{
	uint16_t ucErrTime=0;
	SDA_IN();      //SDA设置为输入  
	IIC_SDA(GPIO_PIN_SET);HAL_Delay_us(1);	   
	IIC_SCL(GPIO_PIN_SET);HAL_Delay_us(1);	 
	while(READ_SDA)
	{
		ucErrTime++;
		if(ucErrTime>250)
		{
//			printf("应答失败！");
			IIC_Stop();
			return 1;
		}
	}
	IIC_SCL(GPIO_PIN_RESET);//时钟输出0 	   
	return 0;  
}

//产生ACK应答
void IIC_Ack(void)
{
	IIC_SCL(GPIO_PIN_RESET);
	SDA_OUT();
	IIC_SDA(GPIO_PIN_RESET);
	HAL_Delay_us(2);
	IIC_SCL(GPIO_PIN_SET);
	HAL_Delay_us(2);
	IIC_SCL(GPIO_PIN_RESET);
  //HAL_Delay_us(2);
}

//不产生ACK应答		    
void IIC_NAck(void)
{
	IIC_SCL(GPIO_PIN_RESET);
	SDA_OUT();
	IIC_SDA(GPIO_PIN_SET);
	HAL_Delay_us(2);
	IIC_SCL(GPIO_PIN_SET);
	HAL_Delay_us(2);
	IIC_SCL(GPIO_PIN_RESET);
  //HAL_Delay_us(2);
}

//IIC发送一个字节
//返回从机有无应答
//1，有应答
//0，无应答			  
uint8_t IIC_Send_Byte(uint8_t txd)
{
	uint8_t t;
    
	SDA_OUT(); 	    
	IIC_SCL(GPIO_PIN_RESET);//拉低时钟开始数据传输
	for(t=0;t<8;t++)
	{              
			//IIC_SDA=(txd&0x80)>>7;
		if(txd&0x80) IIC_SDA(GPIO_PIN_SET);
		else IIC_SDA(GPIO_PIN_RESET);
		txd<<=1; 	  
		HAL_Delay_us(2);   //对TEA5767这三个延时都是必须的
		IIC_SCL(GPIO_PIN_SET);
		HAL_Delay_us(2); 
		IIC_SCL(GPIO_PIN_RESET);	
		HAL_Delay_us(2); 
	}	 
  return IIC_Wait_Ack();
} 	    
//读1个字节，ack=1时，发送ACK，ack=0，发送nACK   
uint8_t IIC_Read_Byte(unsigned char ack)
{
	unsigned char i,receive=0;
	
  IIC_SDA(GPIO_PIN_SET);
	SDA_IN();//SDA设置为输入
  for(i=0;i<8;i++ )
	{  
    IIC_SCL(GPIO_PIN_SET);  
		HAL_Delay_us(2);
    receive<<=1;
    if(READ_SDA) receive |= 0X01;     
    HAL_Delay_us(2);
		IIC_SCL(GPIO_PIN_RESET);
    HAL_Delay_us(2);
  }		
	if(!ack)
		IIC_NAck();//发送nACK
	else
	  IIC_Ack(); //发送ACK   0
	return receive;
}


