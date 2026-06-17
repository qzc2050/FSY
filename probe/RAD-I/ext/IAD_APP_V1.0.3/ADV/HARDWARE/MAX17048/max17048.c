#define _MAX17048_C_

#include "gpio.h"
#include "control.h"

#define MAX17048_ATHD_VAL  0x16     //设置低电量报警阈值

//// /**
////  * @brief  读取剩余电量
////  * @note   
////  * @retval 
////  */
//uint8_t max17048_get_percent(void)
//{
//    uint16_t p_value;

//	// memoryWrite[0] = REGISTER_SOC;
//	
//    // if (write_with_address(MAX17048_ADDRESS, memoryWrite, 1) != 1)
//    //     return 0;
//    // if (read_with_address(MAX17048_ADDRESS, memoryRead, 2) != 2)
//    //     return 0;

//    // uint16_t value = ((uint16_t)memoryRead[0] << 8) | memoryRead[1];
//    // // remove rounding error when converting percent to per mille
//    // if (value > 100 * 256)
//    // {
//    //     value = 100 * 256;
//    // }
//    // *per = ((float)value / 256.0f);
//    // return 1;

//    uint8_t buf[2];

////	  max17048_write_rag(REGISTER_SOC, buf, 1);
//		
//    max17048_read_rag(REGISTER_SOC, buf, 2);

//    p_value = ((uint16_t)buf[0] << 8) | buf[1];

//    if (p_value > (100 * 256))
//    {
//        p_value = 100 * 256;
//    }
//		Get_vol();
//    printf("初始值：%.2f\r\n\r\n",(float)p_value / 256.0f);
//    printf("电量值：%.2f\r\n\r\n",(float)p_value / 256.0f * 1.12);   //90％灯灭   91%灯灭
//		
////		MAX17048_POR();
////		MAX17048_SET_HIBRT();
//    return (int)(p_value / 256 * 1.12);

//}

// /**
//  * @brief  读取剩余电量
//  * @note   
//  * @retval 
//  */
void max17048_get_percent(uint8_t *perc)
{
    uint8_t p_value,buf[2];

    max17048_read_rag(REGISTER_SOC, buf, 2);

    p_value = (((uint16_t)buf[0] << 8) | buf[1]) / 256.0f;

	*perc = (p_value>100 ? 100:p_value);
}



// /**
//  * @brief  读取电池电压。单位MV
//  * @note   
//  * @param  callback: 
//  * @retval None
//  */
void max17048_get_millivolt(float *milv)
{
    uint8_t buf[2];

    max17048_read_rag(REGISTER_VCELL, buf, 2);

    uint16_t value = ((uint16_t)buf[0] << 8) | buf[1];

    *milv = (value * 78.125f / 1000.0f);
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

  IIC_Start();
	IIC_Send_Byte(MAX17048_WRITE_ADDRESS);
	IIC_Send_Byte(reg);
	
  for(i = 0; i < len; i++)
	{	   
    ret = IIC_Send_Byte(p_data[i]);  	//发数据
		if(ret)break;  
	}
	IIC_Stop();
  return ret; 
}


void max17048_read_rag(max_register_t reg, uint8_t *p_data, uint8_t len)
{
  uint8_t i; 

	IIC_Start();
	IIC_Send_Byte(MAX17048_WRITE_ADDRESS);  //函数结束时放回应答信号，再次等待应答信号会出错
	IIC_Send_Byte(reg);
	
	IIC_Start();
	IIC_Send_Byte(MAX17048_READ_ADDRESS);

  for(i = 0; i < len; i++)
	{	   
    p_data[i] = IIC_Read_Byte(i==(len-1)?0:1); //发数据	  
	} 
	IIC_Stop();
}


/*============================================================================*/
void MAX17048_Compensation(uint8_t tem)
{
    uint8_t buf[2],RComp;
    uint16_t data = max17048_get_config();
	  uint8_t temp = 0x97; 
	
	  if(tem >= 20)
			RComp = temp + (tem - 20) * (-0.5);
		else 
			RComp = temp + (tem - 20) * (-5);
		
    data &= 0x00FF;
    data |= RComp << 8;
    //max17048_write_rag(MAX17048_CONFIG, data, 2);

    buf[0] = data >> 8;
    buf[1] = data;
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
    
    buf[0] = data >> 8;
    buf[1] = data;
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

    buf[0] = value >> 8;
    buf[1] = value;
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

//void MAX17048_SET_HIBRT(void)     
//{
//    uint8_t buf[2];
//    uint16_t data;

//	  max17048_read_rag(REGISTER_HIBRT,buf,2);
//	  data = ((uint16_t)buf[0] << 8) + buf[1];
////	  printf("\r\ndata1: %d\r\n",data);

//	  data = 0x007F;
//    buf[1] = data;
//    buf[0] = data >> 8;
////    max17048_write_rag(REGISTER_CONFIG, buf, 2);
//    max17048_write_rag(REGISTER_HIBRT, buf, 2);
////	  printf("data2: %d\r\n",data);
//	
//	  max17048_read_rag(REGISTER_HIBRT,buf,2);
//	  data = ((uint16_t)buf[0] << 8) + buf[1];
////	  printf("data3: %d\r\n\r\n",data);
//}


//void MAX17048_STATUS_INIT(void)
//{
//	uint8_t buf[2];
//	uint16_t data = max17048_get_status();
//	uint16_t temp = 0;
//	data &= 0x80FF;
//	//max17048_write_rag(MAX17048_CONFIG, data, 2);

//	buf[0] = data;
//	buf[1] = data >> 8;
//	max17048_write_rag(REGISTER_STATUS, buf, 2);
//	
//	temp = max17048_get_status();
////	printf("状态寄存器数值：%#x\r\n",temp);
//}

//void MAX17048_CLR_ALERT(uint16_t bit)
//{
//	uint8_t buf[2];
//	uint16_t data = max17048_get_status();
//	uint16_t temp = 0;
//	data &= ~bit;
//	//max17048_write_rag(MAX17048_CONFIG, data, 2);

//	buf[0] = data;
//	buf[1] = data >> 8;
//	max17048_write_rag(REGISTER_STATUS, buf, 2);
//	
//	temp = max17048_get_status();
//	printf("状态寄存器数值：%#x\r\n",temp);
//	temp &= bit;
//	printf("清除操作后该位数值：%d\r\n",temp);
//}

uint16_t status_index = 0;

//uint16_t MAX17048_DEAL_STATUS(void)
//{
//	uint16_t status_val = 0;
//	
//	status_val = max17048_get_status();
//	status_val = status_val >> 8;
//	
//	if((status_val &= 0x20) == 1)
//		status_index = MAX17048_CHARGE_STA;   //充电状态
////	else if((status_val &= 0x10) == 1)
////		status_index = MAX17048_ATHD_STA;   //电压电量小于CONFIG.ATHD设置的百分比
////	if((status_val &= 0x02) == 1)
////		status_index = 3;   //电压大于ALRT.VALRTMAX
////	if((status_val &= 0x04) == 1)
////		status_index = 4;   //电压小于ALRT.VALRTMIN
//	printf("触发指令：%#x\r\n",status_index);
//	return status_index;
//}

/*
 * Compensate the battery capacity reading for voltage and initial capacity in Ah, assuming actual cell voltage of 3.7V 
 
 * @param cap The uncompensated battery capacity reading in ampere hours (Ah).
 * @param v_meas The measured battery voltage (including voltage divider) in volts (V).
 * @param v_divider The gain of the voltage divider circuit (unitless).
  @param nominal_voltage The nominal voltage of the battery in volts (V).
 * @param initial_capacity The initial battery capacity in ampere hours (Ah).
 * 
 * @return The compensated battery capacity reading based on voltage in ampere hours (Ah).
 */
/*
 * cap: 电池测得的实际电量，单位为Ah
 * v_meas: 测量得到的电池电压（包括电压分压器），单位为V
 * v_divider: 电压分压器实际的分压比，例如 2 表示将电压减半
 * nominal_voltage：电池的标称电压，单位为V
 * initial_capacity：电池的初始容量，单位为Ah
 * 该函数首先先计算出通过电压读取得到的实际电池电压 v_batt。接着根据 v_batt 和标称电压 nominal_voltage，计算出预期电量cap_expected。
 * 最后，乘上一个校正系数cap_factor，将测得的电量转换为补偿电量并返回。
 */
//float max17048G_T10_compensate_capacity(float cap, float v_meas, uint16_t v_divider, float nominal_voltage, float initial_capacity)
//{
//    // Calculate the actual battery voltage. The voltage divider reduces the voltage by a factor of 2.
//    const float v_cell = 65535 * v_meas * v_divider / nominal_voltage;

//    // Calculate the expected capacity based on the actual cell voltage and the nominal capacity.
//    const float expected_capacity = initial_capacity * (v_cell / 3700.0f);

//    // Apply the correction factor to the measured capacity.
//    const float compensated_capacity = cap * (expected_capacity / initial_capacity);

//    return compensated_capacity;
//}

/* 
 * Compensate the battery capacity reading for voltage and initial capacity in Ah, and return the remaining battery 
 * capacity percentage assuming actual cell voltage of 3.7V.
 * 
 * @param soc_pct The remaining battery capacity percentage reported by MAX17048G_T10.
 * @param v_meas The measured battery voltage (including voltage divider) in volts (V).
 * @param v_divider The gain of the voltage divider circuit (unitless).
 * @param nominal_voltage The nominal voltage of the battery in volts (V).
 * @param initial_capacity The initial battery capacity in Ah.
 * 
 * @return The compensated battery capacity remaining percentage based on voltage.
 */
//float max17048G_T10_compensate_capacity(float soc_pct, float v_meas, uint16_t v_divider, float nominal_voltage, float initial_capacity)
//{
//		float vol = 0;
//	
//	  max17048_get_millivolt(&vol);
//    // Read the last voltage measurement from the MAX17048G_T10 register.
//     float v_cell = vol * 0.00125f / 1000;

//    // Calculate the expected capacity based on the actual cell voltage and the nominal capacity.
//     float cap_expected = initial_capacity * (v_cell / nominal_voltage);

//    // Calculate the correction factor to account for any deviation from nominal capacity.
//     float pct_factor = ++soc_pct / 100.0f;
//     float cap_factor = cap_expected / (initial_capacity * pct_factor);

//    // Apply the correction factor to the measured capacity and convert to a percentage.
//     float cap_compensated = initial_capacity * pct_factor * cap_factor;
//     float soc_compensated = 100.0f * cap_compensated / initial_capacity;

//    return soc_compensated;
//}


