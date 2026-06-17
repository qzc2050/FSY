#include "AHT20.h"

#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"



// AHT20 I2C 初始化
void AHT20_I2C_Init(void)
{
    
}


bool AHT20_I2C_Write(uint8_t *data, uint8_t size)
{
    return I2C4_Master_Transmit(AHT20_I2C_ADDR, data, size, I2C_BUS_TIMEOUT_MS);
}

bool AHT20_I2C_Read(uint8_t *data, uint8_t size)
{
    return I2C4_Master_Receive(AHT20_I2C_ADDR, data, size, I2C_BUS_TIMEOUT_MS);
}

void AHT20_Delay_ms(uint16_t time)
{
    // HAL_Delay(time);
    vTaskDelay(time);
}


// AHT20 复位
bool AHT20_Reset(void)
{
    uint8_t cmd = AHT20_CMD_RESET;
    
    if (AHT20_I2C_Write(&cmd, 1))
        return false;
    
    AHT20_Delay_ms(20);  // 复位后等待
    return true;
}

// 检查AHT20是否已校准
uint8_t AHT20_CheckCalibration(void)
{
    uint8_t status_byte;
    
    if(!AHT20_I2C_Read(&status_byte, 1))
        return (status_byte & AHT20_STATUS_CALIBRATED);
    
    return false;
}

// AHT20 初始化
void AHT20_Init(void)
{
    uint8_t init_cmd[3] = {AHT20_CMD_INIT, 0x08, 0x00};
    
    // 先尝试复位
    AHT20_Reset();
    AHT20_Delay_ms(10);
    
    // 检查是否已校准
    if (!AHT20_CheckCalibration())
    {
        // 发送初始化命令
        if (AHT20_I2C_Write(init_cmd, 3))
            printf("AHT20 初始化失败！\r\n");
        AHT20_Delay_ms(10);  // 初始化后等待
    }
    printf("AHT20 初始化成功！\r\n");
}

// AHT20 开始测量
bool AHT20_StartMeasurement(void)
{
    uint8_t measure_cmd[3] = {AHT20_CMD_MEASURE, 0x33, 0x00};
    
    return AHT20_I2C_Write(measure_cmd, 3);
}

// 读取AHT20原始数据
bool AHT20_ReadRawData(uint8_t *data)
{
    uint8_t status_byte;
    
    // 等待测量完成
    uint32_t timeout = 100;  // 最大等待100ms
    do {
        AHT20_Delay_ms(2);
        if (AHT20_I2C_Read(&status_byte, 1))
            return 0x01;    // 读取失败

        timeout--;
    } while ((status_byte & AHT20_STATUS_BUSY) && timeout > 0);
    
    if (!timeout)
        return 0x03;    // 超时
    
    // 读取6字节数据
    return AHT20_I2C_Read(data, 6);
}

// 读取AHT20温湿度数据
bool AHT20_ReadData(uint32_t *humidity, uint32_t *temperature)
{
    uint8_t raw_data[6];
    
    if (AHT20_ReadRawData(raw_data))
        return 0x01;
    
    // 检查数据有效性
    if ((raw_data[0] & AHT20_STATUS_CALIBRATED) == 0)
        return 0x01;
    
    // 解析湿度数据 (20位)
    *humidity = ((uint32_t)raw_data[1] << 12) | ((uint32_t)raw_data[2] << 4) | (raw_data[3] >> 4);
    // 转换为百分比 (0-100000 表示 0-100%)
    *humidity = (*humidity * 1000) / 1048576;
    
    // 解析温度数据 (20位)
    *temperature = (((uint32_t)raw_data[3] & 0x0F) << 16) | ((uint32_t)raw_data[4] << 8) | raw_data[5];
    // 转换为摄氏度 (0-200000 表示 -50~150°C)
    *temperature = (*temperature * 2000) / 1048576 - 500;
    
    return false;
}

// 单次读取AHT20数据
void AHT20_ReadOnce(uint32_t *humidity, uint32_t *temperature)
{
    // 开始测量
    if(AHT20_StartMeasurement())
    {
        printf("AHT20 Start Measure Failed!\r\n");
        return;
    }

    // 等待测量完成并读取数据
    AHT20_Delay_ms(80);
    if(AHT20_ReadData(humidity, temperature))
    {
        printf("AHT20 Read Data Failed!\r\n");
        return;
    }
    // printf("Raw -> AHT20 -> Temperature: %d, Humidity: %d%%\r\n", 
    //         *temperature, *humidity);

    // 显示温湿度数据
    printf("AHT20 -> Temperature: %u.%u ℃, Humidity: %u.%u%%\r\n", 
        *temperature / 10, *temperature % 10,
        *humidity / 10, *humidity % 10);
}






