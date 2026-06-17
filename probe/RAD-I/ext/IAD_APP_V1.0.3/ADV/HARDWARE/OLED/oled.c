#include "control.h"

extern SPI_HandleTypeDef hspi1;

HAL_StatusTypeDef HAL_USER_SPI_Transmit(SPI_HandleTypeDef *hspi, uint8_t *pData)
{
  /* Process Locked */
  __HAL_LOCK(hspi);

	while (hspi->State != HAL_SPI_STATE_READY);   //等待进行发送

  /* Set the transaction information */
  hspi->State       = HAL_SPI_STATE_BUSY_TX;
  hspi->ErrorCode   = HAL_SPI_ERROR_NONE;
  hspi->pTxBuffPtr  = (uint8_t *)pData;

  /* Configure communication direction : 1Line */
	__HAL_SPI_DISABLE(hspi);
	SPI_1LINE_TX(hspi);

  /* Check if the SPI is already enabled */
  if ((hspi->Instance->CR1 & SPI_CR1_SPE) != SPI_CR1_SPE)
  {
    /* Enable SPI peripheral */
    __HAL_SPI_ENABLE(hspi);
  }
	
  /* Transmit data in 8 Bit mode */
	*((__IO uint8_t *)&hspi->Instance->DR) = (*hspi->pTxBuffPtr);

  if (hspi->ErrorCode != HAL_SPI_ERROR_NONE);
  else  hspi->State = HAL_SPI_STATE_READY;

  /* Process Unlocked */
  __HAL_UNLOCK(hspi);
	return HAL_OK;
}

//******************************************************************************
//    函数说明：OLED写入一个指令
//    入口数据：dat 数据
//    返回值：  无
//******************************************************************************
void OLED_WR_Bus(uint8_t dat)
{
	OLED_CS_Clr();
		
    HAL_USER_SPI_Transmit(&hspi1,&dat); //通过外设SPIx发送一个数据
	OLED_CS_Set();
}

//******************************************************************************
//    函数说明：OLED写入一个指令
//    入口数据：reg 指令
//    返回值：  无
//******************************************************************************
void OLED_WR_REG(uint8_t reg)
{	  
	OLED_DC_Clr();		  
    OLED_WR_Bus(reg);
    OLED_DC_Set();	
}

//******************************************************************************
//    函数说明：OLED写入一个数据
//    入口数据：dat 数据
//    返回值：  无
//******************************************************************************
void OLED_WR_Byte(uint8_t dat)
{	  
    OLED_WR_Bus(dat);
}

//******************************************************************************
//    函数说明：OLED显示列的起始终止地址
//    入口数据：a  列的起始地址
//              b  列的终止地址
//    返回值：  无
//******************************************************************************
void OLED_AddressSet(uint8_t x,uint8_t y) 
{
	OLED_WR_REG(0xB0);
	OLED_WR_REG(y);
	OLED_WR_REG(((x&0xf0)>>4)|0x10);
	OLED_WR_REG((x&0x0f));
}

//******************************************************************************
//    函数说明：OLED清屏显示
//    入口数据：无
//    返回值：  无
//******************************************************************************
void OLED_Clear(void)
{
	OLED_Draw_Fill(0,0,128,64,0x00);
}

//******************************************************************************
//    函数说明：OLED显示汉字
//    入口数据：x,y :起点坐标
//              *s  :要显示的汉字串
//              sizey 字符高度 
//              mode  0:正常显示；1：反色显示
//    返回值：  无
//******************************************************************************
//void OLED_ShowChinese(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode)
void OLED_ShowChinese(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey)
{
	uint8_t temp = 0;
	while(*s!=0)
	{
//		if(sizey==12) OLED_ShowChinese12x12(x+temp,y,s,sizey,mode);
//		else if(sizey==16) OLED_ShowChinese16x16(x,y,s,sizey,mode);
////		else if(sizey==32) OLED_ShowChinese32x32(x,y,s,sizey,mode);
//		else return;
//		s+=2;
//		temp+=2;
//		x+=sizey;
		if(sizey==12) OLED_ShowChinese12x12(x+temp,y,s,sizey);
		else if(sizey==16) OLED_ShowChinese16x16(x,y,s,sizey);
//		else if(sizey==32) OLED_ShowChinese32x32(x,y,s,sizey);
		else return;
		s+=2;
		temp+=2;
		x+=sizey;
	}
}

//******************************************************************************
//    函数说明：OLED显示汉字
//    入口数据：x,y :起点坐标
//              *s  :要显示的汉字
//              sizey 字符高度 
//              mode  0:正常显示；1：反色显示
//    返回值：  无
//******************************************************************************
//void OLED_ShowChinese12x12(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode)
void OLED_ShowChinese12x12(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey)
{
	uint8_t HZnum;
	
	HZnum=sizeof(tfont12)/sizeof(typFNT_GB12);	//统计汉字库数目
	
	for(uint8_t k=0;k<HZnum;k++)
	{
		if ((tfont12[k].Index[0]==*(s))&&(tfont12[k].Index[1]==*(s+1)))
		{ 	
//			OLED_DrawSingleBMP(x,y,12,12,(uint8_t *)&tfont12[k].Msk[0],mode);
			OLED_DrawSingleBMP(x,y,12,12,(uint8_t *)&tfont12[k].Msk[0]);
		}
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}

//******************************************************************************
//    函数说明：OLED显示汉字
//    入口数据：x,y :起点坐标
//              *s  :要显示的汉字
//              sizey 字符高度 
//              mode  0:正常显示；1：反色显示
//    返回值：  无
//******************************************************************************
//void OLED_ShowChinese16x16(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode)
void OLED_ShowChinese16x16(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey)
{
	uint8_t i,j,k,t,DATA = 0,HZnum;
	uint16_t TypefaceNum;
	x/=2;
	TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;//字符所占字节数
	HZnum=sizeof(tfont16)/sizeof(typFNT_GB16);	//统计汉字库数目
	t=sizey/8;
	for(k=0;k<HZnum;k++)
	{
		if((tfont16[k].Index[0]==*(s))&&(tfont16[k].Index[1]==*(s+1)))
		{
			for(i=0;i<TypefaceNum;i++)
			{
				if(i%t==0)
				{
					OLED_AddressSet(x,y);
					y++;
				}
				for(j=0;j<4;j++)
				{
					if(tfont16[k].Msk[i]&(0x01<<(j*2+0)))
					{
						DATA=0xf0;
					}
					if(tfont16[k].Msk[i]&(0x01<<(j*2+1)))
					{
						DATA|=0x0f;
					}
//					if(mode)
//					{
//						OLED_WR_Byte(~DATA);
//					}else
//					{
						OLED_WR_Byte(DATA);
//					}
					DATA=0;
				}
			}
		}				  	
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}

//******************************************************************************
//    函数说明：OLED显示汉字
//    入口数据：x,y :起点坐标
//              *s  :要显示的汉字
//              sizey 字符高度 
//              mode  0:正常显示；1：反色显示
//    返回值：  无
//******************************************************************************
//void OLED_ShowChinese32x32(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode)
//{
//	uint8_t i,j,k,t,DATA=0,HZnum;
//	uint16_t TypefaceNum;
//	x/=2;
//	TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;//字符所占字节数
//	HZnum=sizeof(tfont32)/sizeof(typFNT_GB32);	//统计汉字库数目
//	t=sizey/8;
//	for(k=0;k<HZnum;k++)
//	{
//		if ((tfont32[k].Index[0]==*(s))&&(tfont32[k].Index[1]==*(s+1)))
//		{ 	
//			for(i=0;i<TypefaceNum;i++)
//			{
//				if(i%t==0)
//				{
//					OLED_AddressSet(x,y);
//					y++;
//				}
//				for(j=0;j<4;j++)
//				{
//					if(tfont32[k].Msk[i]&(0x01<<(j*2+0)))
//					{
//						DATA=0xf0;
//					}
//					if(tfont32[k].Msk[i]&(0x01<<(j*2+1)))
//					{
//						DATA|=0x0f;
//					}
//					if(mode)
//					{
//						OLED_WR_Byte(~DATA);
//					}else
//					{
//						OLED_WR_Byte(DATA);
//					}
//					DATA=0;
//				}
//			}
//		}				  	
//		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
//	}
//}


//******************************************************************************
//    函数说明：OLED显示字符函数 
//    此函数适用范围：字符宽度是2的倍数  字符高度是宽度的2倍
//    入口数据：x,y   起始坐标
//              chr   要写入的字符
//              sizey 字符高度 
//    返回值：  无
//******************************************************************************
//void OLED_ShowChar12x12(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode)
void OLED_ShowChar12x12(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey)
{
	uint8_t k,HZnum;
	
	HZnum=sizeof(tchar12)/sizeof(typASC_CHAR12);	//统计汉字库数目
	
	for(k=0;k<HZnum;k++)
	{
		if (tchar12[k].Index[0]== *(s))
		{ 	
			OLED_DrawSingleBMP(x,y,sizey/2,sizey,(uint8_t *)&tchar12[k].Msk[0]);
		}
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}

//******************************************************************************
//    函数说明：OLED显示字符函数 
//    此函数适用范围：字符宽度是2的倍数  字符高度是宽度的2倍
//    入口数据：x,y   起始坐标
//              chr   要写入的字符
//              sizey 字符高度 
//    返回值：  无
//******************************************************************************
//void OLED_ShowChar16x16(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode)
void OLED_ShowChar16x16(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey)
{
	uint8_t k,HZnum;
	
	HZnum=sizeof(tchar16)/sizeof(typASC_CHAR16);	//统计汉字库数目
	
	for(k=0;k<HZnum;k++)
	{
		if (tchar16[k].Index[0]== *(s))
		{ 	
//			OLED_DrawSingleBMP(x,y,sizey/2,sizey,(uint8_t *)&tchar16[k].Msk[0],mode);
			OLED_DrawSingleBMP(x,y,sizey/2,sizey,(uint8_t *)&tchar16[k].Msk[0]);
		}
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}


//******************************************************************************
//    函数说明：OLED显示字符函数 
//    此函数适用范围：字符宽度是2的倍数  字符高度是宽度的2倍
//    入口数据：x,y   起始坐标
//              chr   要写入的字符
//              sizey 字符高度 
//    返回值：  无
//******************************************************************************
//void OLED_ShowChar32x32(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode)
void OLED_ShowChar32x32(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey)
{
	uint8_t k,HZnum;
	
	HZnum=sizeof(tchar32)/sizeof(typASC_CHAR32);	//统计汉字库数目
	
	for(k=0;k<HZnum;k++)
	{
		if (tchar32[k].Index[0]== *(s))
		{ 	
//			OLED_DrawSingleBMP(x,y,sizey/2,sizey,(uint8_t *)&tchar32[k].Msk[0],mode);
			OLED_DrawSingleBMP(x,y,sizey/2,sizey,(uint8_t *)&tchar32[k].Msk[0]);
		}
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}

//******************************************************************************
//    函数说明：OLED显示字符函数 
//    此函数适用范围：字符宽度是2的倍数  字符高度是宽度的2倍
//    入口数据：x,y   起始坐标
//              chr   要写入的字符
//              sizey 字符高度 
//    返回值：  无
//******************************************************************************
//void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey,uint8_t mode)
void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *s,uint8_t sizey)
{
	while(*s!=0)
	{
//		if(sizey==12) OLED_ShowChar12x12(x,y,s,sizey,mode);
//		else if(sizey==16) OLED_ShowChar16x16(x,y,s,sizey,mode);
//		else if(sizey==32) OLED_ShowChar32x32(x,y,s,sizey,mode);
//		else return;
//		s+=1;
//		x+=sizey/2;
        if(sizey==12) OLED_ShowChar12x12(x,y,s,sizey);
		else if(sizey==16) OLED_ShowChar16x16(x,y,s,sizey);
		else if(sizey==32) OLED_ShowChar32x32(x,y,s,sizey);
		else return;
		s+=1;
		x+=sizey/2;
	}
}

//******************************************************************************
//    函数说明：OLED显示字符函数 
//    此函数适用范围：字符宽度是2的倍数  字符高度是宽度的2倍
//    入口数据：x,y   起始坐标
//              chr   要写入的字符
//              sizey 字符高度 
//    返回值：  无
//******************************************************************************
//void OLED_ShowChar(uint8_t x,uint8_t y,uint8_t chr,uint8_t sizey,uint8_t mode)
void OLED_ShowChar(uint8_t x,uint8_t y,uint8_t chr,uint8_t sizey)
{
//	OLED_ShowString(x,y,&chr,sizey,mode);
	OLED_ShowString(x,y,&chr,sizey);
}


//******************************************************************************
//    函数说明：OLED显示字符函数 
//    此函数适用范围：字符宽度是2的倍数  字符高度是宽度的2倍
//    入口数据：x,y   起始坐标
//              chr   要写入的字符
//              sizey 字符高度 
//    返回值：  无
//******************************************************************************
//void OLED_ShowChar(uint8_t x,uint8_t y,uint8_t chr,uint8_t sizey,uint8_t mode)
//{
//	uint8_t i,j,c,k,t=4,temp,DATA=0;
//	uint16_t num;
//	x/=2;
//	c=chr-' ';
//	num=(sizey/16+((sizey%16)?1:0))*sizey;
//	k=sizey/16;
//	for(i=0;i<num;i++)
//	{
//		if(sizey==12)temp=ascii_1206[c][i];//调用6x12字符
//		else if(sizey==16)temp=ascii_1608[c][i];//调用8x16字符
////		else if(sizey==24)temp=ascii_2412[c][i];//调用12x24字符
//		else if(sizey==32)temp=ascii_3216[c][i];//调用16x32字符
////		else if(sizey==40)temp=ascii_4020[c][i];//调用20x40字符(有问题)
////		else if(sizey==48)temp=ascii_4824[c][i];//调用24x48字符
//		else return;
//		if(sizey%16)
//		{
//			k=sizey/16+1;
//			if(i%k) t=2;
//			else t=4;
//		}
//		if(i%k==0)
//		{
//			OLED_AddressSet(x,y);
//			y++;
//		}
//		for(j=0;j<t;j++)
//		{
//			if(temp&(0x01<<(j*2+0)))
//			{
//				DATA=0xf0;
//			}
//			if(temp&(0x01<<(j*2+1)))
//			{
//				DATA|=0x0f;
//			}
//			if(mode)
//			{
//				OLED_WR_Byte(~DATA);
//			}else
//			{
//				OLED_WR_Byte(DATA);
//			}
//			DATA=0;
//		}
//	}
//}

//******************************************************************************
//    函数说明：OLED显示字符串
//    入口数据：x,y  起始坐标
//              *dp   要写入的字符
//              sizey 字符高度 
//              mode  0:正常显示；1：反色显示
//    返回值：  无
//******************************************************************************
//void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *dp,uint8_t sizey,uint8_t mode)
//{
//	while(*dp!='\0')
//	{
//	  OLED_ShowChar(x,y,*dp,sizey,mode);
//		dp++;
//		x+=sizey/2;
//	}
//}


//******************************************************************************
//    函数说明：m^n
//    入口数据：m:底数 n:指数
//    返回值：  result
//******************************************************************************
uint32_t oled_pow(uint16_t m,uint16_t n)
{
	uint32_t result=1;
	while(n--)result*=m;
	return result;
}


//******************************************************************************
//    函数说明：OLED显示变量
//    入口数据：x,y :起点坐标	 
//              num :要显示的变量
//              len :数字的位数
//              sizey 字符高度 
//              mode  0:正常显示；1：反色显示
//    返回值：  无
//******************************************************************************
//void OLED_ShowNum(uint8_t x,uint8_t y,uint32_t num,uint16_t len,uint8_t sizey,uint8_t mode)
void OLED_ShowNum(uint8_t x,uint8_t y,uint32_t num,uint16_t len,uint8_t sizey)
{         	
	uint8_t t,temp;
	uint8_t enshow=0;
	for(t=0;t<len;t++)
	{
		temp=(num/oled_pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
//				OLED_ShowChar(x+(sizey/2)*t,y,' ',sizey,mode);
				OLED_ShowChar(x+(sizey/2)*t,y,' ',sizey);
				continue;
			}
            else
                enshow=1; 
		}
//	 	OLED_ShowChar(x+(sizey/2)*t,y,temp+'0',sizey,mode); 
	 	OLED_ShowChar(x+(sizey/2)*t,y,temp+'0',sizey); 
	}
}

//******************************************************************************
//    函数说明：显示灰度图片
//    入口数据：x,y :起点坐标
//              length 图片长度
//              width  图片宽度
//              BMP[] :要显示图片
//              mode  0:正常显示；1：反色显示
//    返回值：  无
//******************************************************************************
//void OLED_DrawBMP(uint8_t x,uint8_t y,uint16_t length,uint8_t width,const uint8_t BMP[],uint8_t mode)
void OLED_DrawBMP(uint8_t x,uint8_t y,uint16_t length,uint8_t width,const uint8_t BMP[])
{
	uint8_t i,j;
	uint16_t k=0;
	x/=2;
	length=length/2+((length%2)?1:0);
	for(i=0;i<width;i++)
	{
		OLED_AddressSet(x,y+i);
		for(j=0;j<length;j++)
		{
//			if(mode)
//			{
//				OLED_WR_Byte(~BMP[k++]);
//			}else
//			{
				OLED_WR_Byte(BMP[k++]);
//			}
        }
	}
}


//******************************************************************************
//    函数说明：显示灰度图片
//    入口数据：x,y :起点坐标
//              length 图片长度
//              width  图片宽度
//              BMP[] :要显示图片
//              mode  0:正常显示；1：反色显示
//    返回值：  无
//******************************************************************************
//void OLED_DrawSingleBMP(uint8_t x,uint8_t y,uint16_t length,uint8_t width,const uint8_t BMP[],uint8_t mode)
void OLED_DrawSingleBMP(uint8_t x,uint8_t y,uint16_t length,uint8_t width,const uint8_t BMP[])
{
	uint8_t i,j,k,data=0;
	uint16_t m=0;
	length=length/8+((length%8)?1:0);
	x/=2;
    
	for(i=0;i<width;i++)
	{
		OLED_AddressSet(x,y+i);
		for(j=0;j<length;j++)
		{
			for(k=0;k<4;k++)
			{
				if(BMP[m]&(0x01<<(k*2+0)))
				{
					data=0xf0;
				}
				if(BMP[m]&(0x01<<(k*2+1)))
				{
					data|=0x0f;
				}
                
//                mode = 0;
//				if(mode)
//					OLED_WR_Byte(~DATA);
//                else
				OLED_WR_Byte(data);
				data=0;
			}
			m++;
		}
	}
}
/********************************************************************************************
* 函数名：OLED_GPIO_Init
* 描述  ：初始化IO口
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void OLED_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

    __HAL_RCC_GPIOB_CLK_ENABLE();
	
	GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_7;//GPIO_PIN_3|
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_7,GPIO_PIN_SET);
	
	OLED_WR_REG(0xAF);   //正常显示
}
/********************************************************************************************
* 函数名：OLED_Init
* 描述  ：初始化OLED
* 输入  : 无
* 输出  ：无
* 调用  ：外部调用
********************************************************************************************/
void OLED_Init(void)
{
    OLED_GPIO_Init();
	
	OLED_RES_Clr(); //复位
	HAL_Delay(10);
	OLED_RES_Set();
	
	OLED_WR_REG(0xAE); //Set display off
	OLED_WR_REG(0xB0); //Row address Mode Setting
	OLED_WR_REG(0x00);
	OLED_WR_REG(0x10); //Set Higher Column Address of display RAM
	OLED_WR_REG(0x00); //Set Lower Column Address of display RAM
	OLED_WR_REG(0xD5); //Set Display Clock Divide Ratio/Oscillator Frequency
	OLED_WR_REG(0x50); //50 125hz
	OLED_WR_REG(0xD9); //Set Discharge/Precharge Period
	OLED_WR_REG(0x22);
	OLED_WR_REG(0x40); //Set Display Start Line
	OLED_WR_REG(0x81); //对比度设置
	OLED_WR_REG(0xFF); /*    <--------------------------------------------改变亮度    */
	if(USE_HORIZONTAL)
	{
		OLED_WR_REG(0xA1); //Set Segment Re-map
		OLED_WR_REG(0xC8); //Set Common Output Scan Direction
        OLED_WR_REG(0xD3); //Set Display Offset
        OLED_WR_REG(0x20);
	}else
	{
		OLED_WR_REG(0xA0); //Set Segment Re-map
		OLED_WR_REG(0xC0); //Set Common Output Scan Direction
        OLED_WR_REG(0xD3); //Set Display Offset
        OLED_WR_REG(0x00);
	}
	OLED_WR_REG(0xA4); //Set Entire Display OFF/ON
	OLED_WR_REG(0xA6); //Set Normal/Reverse Display
	OLED_WR_REG(0xA8); //Set Multiplex Ration
	OLED_WR_REG(0x3F);
	OLED_WR_REG(0xAD); //DC-DC Setting
	OLED_WR_REG(0x80); //DC-DC is disable
	OLED_WR_REG(0xDB); //Set VCOM Deselect Level
	OLED_WR_REG(0x30);
	OLED_WR_REG(0xDC); //Set VSEGM Level
	OLED_WR_REG(0x30);
	OLED_WR_REG(0x33); //Set Discharge VSL Level 1.8V
	OLED_Clear();
	OLED_WR_REG(0xAF); //Set Display On
}

//******************************************************************************
//    函数说明：填充区域
//    返回值：  无
//******************************************************************************
//void OLED_Draw_Fill(uint8_t x1,uint8_t y1,uint8_t x_length,uint8_t y_length,uint8_t data,uint8_t mode)
void OLED_Draw_Fill(uint8_t x1,uint8_t y1,uint8_t x_length,uint8_t y_length,uint8_t data)
{
	uint16_t j,i;
	
	x1 /= 2; 
	for(i=y1;i<y1+y_length;i++)
	{
		OLED_AddressSet(x1,i);
		for(j=0;j<x_length;j++)
		{
//			if(!mode)
				OLED_WR_Byte(data);
//			else 
//				OLED_WR_Byte(~data);
		}
	}
}

//******************************************************************************
//    函数说明：画像素点
//    入口数据：像素点位置
//    返回值：  无
//******************************************************************************
//void OLED_Draw_Pixel(uint8_t x, uint8_t y)
//{
//    uint8_t page = y / 8;          // SH1122的页地址
//    uint8_t bit_mask = 1 << (y % 8); // 位掩码

//    // 读取当前显存数据
//    uint8_t data = SH1122_ReadData(x, page);

////    if (color)
//        data |= bit_mask;  // 设置像素点
////    else
////        data &= ~bit_mask; // 清除像素点

//    // 写回显存
//    SH1122_WriteData(x, page, data);
//}
