#include "BME280_app.h"
#include "bme280.h"
#include "main.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"

// 全局设备结构体
struct bme280_dev bme280;

// I2C读写函数实现
static int8_t bme280_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
    HAL_StatusTypeDef status;
    
    // 使用HAL库的I2C读取函数
    status = I2C4_Mem_Read(dev_id << 1, reg_addr, reg_data, len, I2C_BUS_TIMEOUT_MS);
    
    return (status == HAL_OK) ? BME280_OK : BME280_E_COMM_FAIL;
}

static int8_t bme280_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
    HAL_StatusTypeDef status;
    
    // 使用HAL库的I2C写入函数
    status = I2C4_Mem_Write(dev_id << 1, reg_addr, reg_data, len, I2C_BUS_TIMEOUT_MS);
    
    return (status == HAL_OK) ? BME280_OK : BME280_E_COMM_FAIL;
}

// 延时函数
static void bme280_delay_ms(uint32_t period)
{
    // HAL_Delay(period);
    vTaskDelay(period);
}

// BME280初始化函数
int8_t bme280_stm32_init(void)
{
    int8_t rslt;
    
    // 初始化设备结构体
    bme280.dev_id = BME280_I2C_ADDR_PRIM;  // 使用主地址0x76
    bme280.intf = BME280_I2C_INTF;        // I2C接口
    bme280.read = bme280_i2c_read;        // 读函数指针
    bme280.write = bme280_i2c_write;      // 写函数指针
    bme280.delay_ms = bme280_delay_ms;    // 延时函数指针
    
    // 初始化BME280传感器
    rslt = bme280_init(&bme280);
    if (rslt != BME280_OK) {
        return rslt;
    }
    
    // 配置传感器参数
    bme280.settings.osr_p = BME280_OVERSAMPLING_8X;    // 压力8倍过采样
    bme280.settings.osr_t = BME280_OVERSAMPLING_1X;    // 温度1倍过采样  
    bme280.settings.osr_h = BME280_OVERSAMPLING_1X;    // 湿度1倍过采样
    bme280.settings.filter = BME280_FILTER_COEFF_4;     // 滤波器系数4
    bme280.settings.standby_time = BME280_STANDBY_TIME_62_5_MS; // 待机时间62.5ms
    
    // 应用设置
    uint8_t settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | 
                          BME280_OSR_HUM_SEL | BME280_FILTER_SEL | 
                          BME280_STANDBY_SEL;
    
    rslt = bme280_set_sensor_settings(settings_sel, &bme280);
    if (rslt != BME280_OK) {
        return rslt;
    }
    
    // 设置为正常模式
    rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, &bme280);
    
    return rslt;
}

// BME280初始化函数
void bme280_app_init(void)
{
    int8_t rslt;
    
    rslt = bme280_stm32_init();
    if (rslt != BME280_OK)
    {
        // 初始化失败处理
        printf("BME280初始化失败: %d\r\n", rslt);
        return;
    }
    printf("BME280初始化成功！\r\n");
}

// 读取传感器数据函数
int8_t bme280_read_data(struct bme280_data *comp_data)
{
    int8_t rslt;
    
    // 读取所有传感器数据（压力、温度、湿度）
    rslt = bme280_get_sensor_data(BME280_ALL, comp_data, &bme280);
    
    return rslt;
}

// 获取温度数据（简化版本）
float bme280_get_temperature(void)
{
    struct bme280_data comp_data;
    
    if (bme280_read_data(&comp_data) == BME280_OK) {
#ifdef BME280_FLOAT_ENABLE
        return (float)comp_data.temperature;  // 浮点版本
#else
        return comp_data.temperature / 100.0f; // 整数版本，转换为摄氏度
#endif
    }
    
    return -273.15f; // 错误时返回绝对零度
}

// 获取压力数据（简化版本）
float bme280_get_pressure(void)
{
    struct bme280_data comp_data;
    
    if (bme280_read_data(&comp_data) == BME280_OK) {
#ifdef BME280_FLOAT_ENABLE
        return (float)comp_data.pressure;  // 浮点版本，单位为Pa
#else
        return comp_data.pressure / 100.0f; // 整数版本，转换为hPa
#endif
    }
    
    return 0.0f;
}

// 获取湿度数据（简化版本）
float bme280_get_humidity(void)
{
    struct bme280_data comp_data;
    
    if (bme280_read_data(&comp_data) == BME280_OK) {
#ifdef BME280_FLOAT_ENABLE
        return (float)comp_data.humidity;  // 浮点版本
#else
        return comp_data.humidity / 1024.0f; // 整数版本，转换为百分比
#endif
    }
    
    return -1.0f;
}

// 主应用示例
void bme280_get_real_data(float* temperature_out, float* humidity_out, float* baro_out)
{
    int8_t rslt;
    struct bme280_data comp_data;

    static bool first_read = true;
    static uint8_t index = 0;
    static float temperature_buf[FILTER_BUFFER_SIZE];
    static float humidity_buf[FILTER_BUFFER_SIZE];
    static float baro_buf[FILTER_BUFFER_SIZE];
    float temp = 0;
    
    // 读取传感器数据
    rslt = bme280_read_data(&comp_data);
    if (rslt == BME280_OK)
    {
        // 处理传感器数据
#ifdef BME280_FLOAT_ENABLE
        temperature_buf[index] = comp_data.temperature;
        humidity_buf[index] = comp_data.humidity;
        baro_buf[index] = comp_data.pressure;
        if(++index >= FILTER_BUFFER_SIZE)
            index = 0;

        if(first_read)
        {
            for(uint8_t i = 1;i < FILTER_BUFFER_SIZE;i++)
            {
                temperature_buf[i] = temperature_buf[0];
                humidity_buf[i] = humidity_buf[0];
                baro_buf[i] = baro_buf[0];
            }
            first_read = false;
        }

        for(uint8_t i = 0;i < FILTER_BUFFER_SIZE;i++)
            temp += temperature_buf[i];
        *temperature_out = temp / FILTER_BUFFER_SIZE;

        temp = 0;
        for(uint8_t i = 0;i < FILTER_BUFFER_SIZE;i++)
            temp += humidity_buf[i];
        *humidity_out = temp / FILTER_BUFFER_SIZE;
        if(*humidity_out > 100.0f)
            *humidity_out = 100;
            
        // *humidity_out = comp_data.humidity;
        
        temp = 0;
        for(uint8_t i = 0;i < FILTER_BUFFER_SIZE;i++)
            temp += baro_buf[i];
        *baro_out = temp / FILTER_BUFFER_SIZE;

        // printf("BME280 -> 温度: %.2f ℃, 湿度: %.2f %%, 压力: %.2f Pa\r\n", \
                *temperature_out, *humidity_out, *baro_out);
#else
        float temperature = comp_data.temperature / 100.0f;
        float humidity = comp_data.humidity / 1024.0f;
        float pressure = comp_data.pressure / 10000.0f;
        
        // printf("BME280 -> 温度: %.2f °C, 压力: %.2f Pa, 湿度: %.2f %%\r\n", 
        //         temperature, pressure, humidity);
#endif
    }
    else
        printf("BME280 -> 读取传感器数据失败: %d\r\n", rslt);

    // 延时2秒
//    bme280.delay_ms(2000);
}

// 中断模式读取示例（如果需要）
void bme280_interrupt_example(void)
{
    // 配置为强制模式，按需读取
    bme280_set_sensor_mode(BME280_FORCED_MODE, &bme280);
    
    // 等待转换完成（根据过采样设置调整延时）
    bme280.delay_ms(10);
    
    // 读取数据
    struct bme280_data comp_data;
    if (bme280_read_data(&comp_data) == BME280_OK) {
        // 处理数据
    }
}




