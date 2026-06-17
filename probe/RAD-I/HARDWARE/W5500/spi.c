#define _SPI_C_

#include "stm32h7xx_hal.h"
#include "types.h"
#include "spi.h"

SPI_HandleTypeDef SPI1_Handler;  //SPI1句柄
SPI_HandleTypeDef SPI4_Handler;  //SPI4句柄


//以下是SPI模块的初始化代码，配置成主机模式 						  
//SPI口初始化
//这里针是对SPI1的初始化
//void spi1_init(void)
//{
//	GPIO_InitTypeDef GPIO_Initure;
//	RCC_PeriphCLKInitTypeDef SPI1ClkInit;

//	__HAL_RCC_GPIOA_CLK_ENABLE();                   //使能GPIOF时钟
//	__HAL_RCC_SPI1_CLK_ENABLE();                    //使能SPI2时钟

//	//设置SPI2的时钟源 
//	SPI1ClkInit.PeriphClockSelection = RCC_PERIPHCLK_SPI1;	    //设置SPI2时钟源
//	SPI1ClkInit.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL;	//SPI2时钟源使用PLL1Q
//	HAL_RCCEx_PeriphCLKConfig(&SPI1ClkInit);

//	//PB13,14,15
//	GPIO_Initure.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
//	GPIO_Initure.Mode = GPIO_MODE_AF_PP;              //复用推挽输出
//	GPIO_Initure.Pull = GPIO_PULLUP;                  //上拉
//	GPIO_Initure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;   //快速    
//	GPIO_Initure.Alternate = GPIO_AF5_SPI1;           //复用为SPI1
//	HAL_GPIO_Init(GPIOA, &GPIO_Initure);             //初始化

//    SPI1_Handler.Instance = SPI1;                      //SPI1
//    SPI1_Handler.Init.Mode = SPI_MODE_MASTER;          //设置SPI工作模式，设置为主模式
//    SPI1_Handler.Init.Direction = SPI_DIRECTION_2LINES;//设置SPI单向或者双向的数据模式:SPI设置为双线模式
//    SPI1_Handler.Init.DataSize = SPI_DATASIZE_8BIT;    //设置SPI的数据大小:SPI发送接收8位帧结构
//	
//	SPI1_Handler.Init.CLKPolarity = SPI_POLARITY_HIGH;  //串行同步时钟的空闲状态为高电平
//    SPI1_Handler.Init.CLKPhase = SPI_PHASE_2EDGE;      //串行同步时钟的第二个跳变沿（上升或下降）数据被采样

////    SPI3_Handler.Init.CLKPolarity = SPI_POLARITY_LOW;  //串行同步时钟的空闲状态为高电平
////    SPI3_Handler.Init.CLKPhase = SPI_PHASE_1EDGE;      //串行同步时钟的第二个跳变沿（上升或下降）数据被采样
//    SPI1_Handler.Init.NSS = SPI_NSS_SOFT;              //NSS信号由硬件（NSS管脚）还是软件（使用SSI位）管理:内部NSS信号有SSI位控制
//    SPI1_Handler.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;//NSS信号脉冲失能
//    SPI1_Handler.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;  //SPI主模式IO状态保持使能
//    SPI1_Handler.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;//定义波特率预分频的值:波特率预分频值为256
//    SPI1_Handler.Init.FirstBit = SPI_FIRSTBIT_MSB;     //指定数据传输从MSB位还是LSB位开始:数据传输从MSB位开始
//    SPI1_Handler.Init.TIMode = SPI_TIMODE_DISABLE;     //关闭TI模式
//    SPI1_Handler.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;//关闭硬件CRC校验
//    SPI1_Handler.Init.CRCPolynomial = 7;               //CRC值计算的多项式
//    HAL_SPI_Init(&SPI1_Handler);
//    
//    __HAL_SPI_ENABLE(&SPI1_Handler);                 //使能SPI1
//    spi1_read_write_byte(0Xff);                        //启动传输
//}

//SPI速度设置函数
//SPI速度=PLL1Q/分频系数
//@ref SPI_BaudRate_Prescaler:SPI_BAUDRATEPRESCALER_2~SPI_BAUDRATEPRESCALER_256
//PLL1Q时钟一般为200Mhz：
//void spi3_set_speed(uint32_t SPI_BaudRatePrescaler)
//{
//    assert_param(IS_SPI_BAUDRATE_PRESCALER(SPI_BaudRatePrescaler));//判断有效性
//    __HAL_SPI_DISABLE(&SPI3_Handler);            //关闭SPI
//    SPI3_Handler.Instance->CFG1 &= ~(0X7<<28);     //位30-28清零，用来设置波特率
//    SPI3_Handler.Instance->CFG1 |= SPI_BaudRatePrescaler;//设置SPI速度
//    __HAL_SPI_ENABLE(&SPI3_Handler);             //使能SPI
//}


//SPI2速度设置函数
//SpeedSet:0~7
//SPI速度=spi_ker_ck/2^(SpeedSet+1)
//spi_ker_ck我们选择来自pll1_q_ck,为200Mhz
//void spi1_set_speed(uint8_t SpeedSet)
//{
//	SpeedSet&=0X07;					//限制范围
//	SPI1->CR1&=~(1<<0); 			//SPE=0,SPI设备失能
//	SPI1->CFG1&=~(7<<28); 			//MBR[2:0]=0,清除原来的分频设置
//	SPI1->CFG1|=(uint32_t)SpeedSet<<28;	//MBR[2:0]=SpeedSet,设置SPI2速度  
//	SPI1->CR1|=1<<0; 				//SPE=1,SPI设备使能	 	  
//} 


//SPI2 读写一个字节
//TxData:要写入的字节
//返回值:读取到的字节
uint8_t spi1_read_write_byte(uint8_t TxData)
{
   // uint8_t Rxdata;

//    HAL_SPI_TransmitReceive(&SPI3_Handler, &TxData, &Rxdata, 1, 1000);

// 	return Rxdata;          		    //返回收到的数据
	
	uint8_t RxData=0;	
	SPI1->CR1|=1<<0;				//SPE=1,使能SPI2
	SPI1->CR1|=1<<9;  				//CSTART=1,启动传输
	
	while((SPI1->SR&1<<1)==0);		//等待发送区空 
	*(vuint8_t *)&SPI1->TXDR=TxData;		//发送一个byte,以传输长度访问TXDR寄存器   
	while((SPI1->SR&1<<0)==0);		//等待接收完一个byte  
	RxData=*(vuint8_t *)&SPI1->RXDR;		//接收一个byte,以传输长度访问RXDR寄存器	
	
	SPI1->IFCR|=3<<3;				//EOTC和TXTFC置1,清除EOT和TXTFC位 
	SPI1->CR1&=~(1<<0);				//SPE=0,关闭SPI2,会执行状态机复位/FIFO重置等操作
	return RxData;					//返回收到的数据
	
	
}

//void spi4_init(void)
//{
//	GPIO_InitTypeDef GPIO_Initure;
//	RCC_PeriphCLKInitTypeDef SPI4ClkInit;

//	__HAL_RCC_GPIOE_CLK_ENABLE();                   //使能GPIOF时钟
//	__HAL_RCC_SPI4_CLK_ENABLE();                    //使能SPI2时钟

//	//设置SPI2的时钟源 
//	SPI4ClkInit.PeriphClockSelection = RCC_PERIPHCLK_SPI4;	    //设置SPI2时钟源
//	SPI4ClkInit.Spi45ClockSelection = RCC_SPI45CLKSOURCE_PLL2;	//SPI2时钟源使用PLL1Q
//	HAL_RCCEx_PeriphCLKConfig(&SPI4ClkInit);

//	//PE2,PE5,PE6
//	GPIO_Initure.Pin = GPIO_PIN_2 | GPIO_PIN_5 | GPIO_PIN_6 ;
//	GPIO_Initure.Mode = GPIO_MODE_AF_PP;              //复用推挽输出
//	GPIO_Initure.Pull = GPIO_PULLUP;                  //上拉
//	GPIO_Initure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;   //快速    
//	GPIO_Initure.Alternate = GPIO_AF5_SPI4;           //复用为SPI1
//	HAL_GPIO_Init(GPIOE, &GPIO_Initure);             //初始化

//	SPI4_Handler.Instance = SPI4;                      //SPI1
//	SPI4_Handler.Init.Mode = SPI_MODE_MASTER;          //设置SPI工作模式，设置为主模式
//	SPI4_Handler.Init.Direction = SPI_DIRECTION_2LINES;//设置SPI单向或者双向的数据模式:SPI设置为双线模式
//	SPI4_Handler.Init.DataSize = SPI_DATASIZE_8BIT;    //设置SPI的数据大小:SPI发送接收8位帧结构
//	
//	SPI4_Handler.Init.CLKPolarity = SPI_POLARITY_HIGH;  //串行同步时钟的空闲状态为高电平
//	SPI4_Handler.Init.CLKPhase = SPI_PHASE_2EDGE;      //串行同步时钟的第二个跳变沿（上升或下降）数据被采样

//	//    SPI3_Handler.Init.CLKPolarity = SPI_POLARITY_LOW;  //串行同步时钟的空闲状态为高电平
//	//    SPI3_Handler.Init.CLKPhase = SPI_PHASE_1EDGE;      //串行同步时钟的第二个跳变沿（上升或下降）数据被采样
//	SPI4_Handler.Init.NSS = SPI_NSS_SOFT;              //NSS信号由硬件（NSS管脚）还是软件（使用SSI位）管理:内部NSS信号有SSI位控制
//	SPI4_Handler.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;//NSS信号脉冲失能
//	SPI4_Handler.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;  //SPI主模式IO状态保持使能
//	SPI4_Handler.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;//定义波特率预分频的值:波特率预分频值为256
//	SPI4_Handler.Init.FirstBit = SPI_FIRSTBIT_MSB;     //指定数据传输从MSB位还是LSB位开始:数据传输从MSB位开始
//	SPI4_Handler.Init.TIMode = SPI_TIMODE_DISABLE;     //关闭TI模式
//	SPI4_Handler.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;//关闭硬件CRC校验
//	SPI4_Handler.Init.CRCPolynomial = 7;               //CRC值计算的多项式
//	HAL_SPI_Init(&SPI4_Handler);
//	
//	__HAL_SPI_ENABLE(&SPI4_Handler);                 //使能SPI1
//	spi4_read_write_byte(0Xff);                        //启动传输
//}

//SPI速度设置函数
//SPI速度=PLL1Q/分频系数
//@ref SPI_BaudRate_Prescaler:SPI_BAUDRATEPRESCALER_2~SPI_BAUDRATEPRESCALER_256
//PLL1Q时钟一般为200Mhz：
//void spi3_set_speed(uint32_t SPI_BaudRatePrescaler)
//{
//    assert_param(IS_SPI_BAUDRATE_PRESCALER(SPI_BaudRatePrescaler));//判断有效性
//    __HAL_SPI_DISABLE(&SPI3_Handler);            //关闭SPI
//    SPI3_Handler.Instance->CFG1 &= ~(0X7<<28);     //位30-28清零，用来设置波特率
//    SPI3_Handler.Instance->CFG1 |= SPI_BaudRatePrescaler;//设置SPI速度
//    __HAL_SPI_ENABLE(&SPI3_Handler);             //使能SPI
//}


//SPI2速度设置函数
//SpeedSet:0~7
//SPI速度=spi_ker_ck/2^(SpeedSet+1)
//spi_ker_ck我们选择来自pll1_q_ck,为200Mhz
//void spi4_set_speed(uint8_t SpeedSet)
//{
//	SpeedSet&=0X07;					//限制范围
//	SPI4->CR1&=~(1<<0); 			//SPE=0,SPI设备失能
//	SPI4->CFG1&=~(7<<28); 			//MBR[2:0]=0,清除原来的分频设置
//	SPI4->CFG1|=(uint32_t)SpeedSet<<28;	//MBR[2:0]=SpeedSet,设置SPI2速度  
//	SPI4->CR1|=1<<0; 				//SPE=1,SPI设备使能	 	  
//} 


//SPI2 读写一个字节
//TxData:要写入的字节
//返回值:读取到的字节
uint8_t spi4_read_write_byte(uint8_t TxData)
{
   // uint8_t Rxdata;

//    HAL_SPI_TransmitReceive(&SPI3_Handler, &TxData, &Rxdata, 1, 1000);

// 	return Rxdata;          		    //返回收到的数据
	
	uint8_t RxData=0;	
	SPI4->CR1|=1<<0;				//SPE=1,使能SPI4
	SPI4->CR1|=1<<9;  				//CSTART=1,启动传输
	
	while((SPI4->SR&1<<1)==0);		//等待发送区空 
	*(vuint8_t *)&SPI4->TXDR=TxData;		//发送一个byte,以传输长度访问TXDR寄存器   
	while((SPI4->SR&1<<0)==0);		//等待接收完一个byte  
	RxData=*(vuint8_t *)&SPI4->RXDR;		//接收一个byte,以传输长度访问RXDR寄存器	
	
	SPI4->IFCR|=3<<3;				//EOTC和TXTFC置1,清除EOT和TXTFC位 
	SPI4->CR1&=~(1<<0);				//SPE=0,关闭SPI2,会执行状态机复位/FIFO重置等操作
	return RxData;					//返回收到的数据
	
	
}




