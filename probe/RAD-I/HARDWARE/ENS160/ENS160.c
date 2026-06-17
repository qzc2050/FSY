#include "ens160.h"

#include "main.h"
#include "i2c.h"

#include "FreeRTOS.h"
#include "task.h"

ENS160_HandleTypeDef ens160;

ENS160_I2C_Funcs i2c_funcs = {
    .i2c_write = ens160_i2c_write,
    .i2c_read = ens160_i2c_read,
    .delay_ms = ens160_delay_ms,
    .debug_print = ens160_debug_print
};

// 内部函数声明
static bool ENS160_CheckPartID(ENS160_HandleTypeDef *hens160);
static bool ENS160_ClearCommand(ENS160_HandleTypeDef *hens160);
static bool ENS160_GetFirmware(ENS160_HandleTypeDef *hens160);

static uint8_t ENS160_Read8(ENS160_HandleTypeDef *hens160, uint8_t reg);
static uint8_t ENS160_Read(ENS160_HandleTypeDef *hens160, uint8_t reg, uint8_t *buf, uint8_t num);
static uint8_t ENS160_Write8(ENS160_HandleTypeDef *hens160, uint8_t reg, uint8_t value);
static uint8_t ENS160_Write(ENS160_HandleTypeDef *hens160, uint8_t reg, uint8_t *buf, uint8_t num);


// 用户需要实现的I2C函数
int ens160_i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status = I2C4_Mem_Write(dev_addr << 1, reg_addr, data, len, I2C_BUS_TIMEOUT_MS);
    return (status == HAL_OK) ? 0 : 1;
}

int ens160_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status = I2C4_Mem_Read(dev_addr << 1, reg_addr, data, len, I2C_BUS_TIMEOUT_MS);
    return (status == HAL_OK) ? 0 : 1;
}

void ens160_delay_ms(uint32_t ms)
{
    // HAL_Delay(ms);
    vTaskDelay(ms);
}

void ens160_debug_print(const char *msg)
{
    printf("%s\r\n", msg);
    // 通过串口输出调试信息
    // HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 1000);
}




bool ENS160_Init(ENS160_HandleTypeDef *hens160, uint8_t slave_addr, ENS160_I2C_Funcs *i2c_funcs, bool debug)
{
    if (hens160 == NULL || i2c_funcs == NULL)
        return false;
    
    // 初始化结构体
    hens160->slave_addr = slave_addr;
    hens160->i2c_funcs = *i2c_funcs;
    hens160->debug_enabled = debug;
    hens160->available = false;
    
    // 等待传感器启动
    if(hens160->i2c_funcs.delay_ms)
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    
    hens160->available = false;
    hens160->available = ENS160_Reset(hens160);
    hens160->available = ENS160_CheckPartID(hens160);
    
    if (hens160->available)
    {
        hens160->available = ENS160_SetMode(hens160, ENS160_OPMODE_IDLE);
        hens160->available = ENS160_ClearCommand(hens160);
        hens160->available = ENS160_GetFirmware(hens160);
    }
    return hens160->available;
}

void ENS160_Setting(void)
{
    // 初始化ENS160
    if(ENS160_Init(&ens160, ENS160_I2CADDR_0, &i2c_funcs, true))
    {
        printf("ENS160: 初始化成功!\r\n");

        // 设置环境数据（温度和湿度）
        float temperature = 25.0f;  // 假设温度25℃
        float humidity = 50.0f;     // 假设湿度50%
        ENS160_SetEnvData(&ens160, temperature, humidity);
        
        // 设置为标准测量模式
        ENS160_SetMode(&ens160, ENS160_OPMODE_STD);
        printf("ENS160: 设置标准测量模式!\r\n\r\n");
    }
    else
        printf("ENS160: 初始化失败! 请检查接线和地址。\r\n");
}

bool ENS160_Reset(ENS160_HandleTypeDef *hens160)
{
    uint8_t result = ENS160_Write8(hens160, ENS160_REG_OPMODE, ENS160_OPMODE_RESET);
    
    if (hens160->debug_enabled && hens160->i2c_funcs.debug_print) {
        char msg[50];
        sprintf(msg, "ENS160_Reset() result: %s", result == 0 ? "ok" : "nok");
        hens160->i2c_funcs.debug_print(msg);
    }
    
    if (hens160->i2c_funcs.delay_ms) {
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    }
    
    return result == 0;
}

static bool ENS160_CheckPartID(ENS160_HandleTypeDef *hens160)
{
    uint8_t i2cbuf[2];
    uint16_t part_id;
    bool result = false;
    
    ENS160_Read(hens160, ENS160_REG_PART_ID, i2cbuf, 2);
    part_id = i2cbuf[0] | ((uint16_t)i2cbuf[1] << 8);
    
    if (hens160->debug_enabled && hens160->i2c_funcs.debug_print) {
        char msg[50];
        sprintf(msg, "ENS160_CheckPartID() result: 0x%04X", part_id);
        hens160->i2c_funcs.debug_print(msg);
    }
    
    if (hens160->i2c_funcs.delay_ms) {
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    }
    
    if (part_id == ENS160_PARTID) {
        hens160->rev_ens16x = 0;
        result = true;
    } else if (part_id == ENS161_PARTID) {
        hens160->rev_ens16x = 1;
        result = true;
    }
    
    return result;
}

static bool ENS160_ClearCommand(ENS160_HandleTypeDef *hens160)
{
    uint8_t status;
    uint8_t result;
    
    result = ENS160_Write8(hens160, ENS160_REG_COMMAND, ENS160_COMMAND_NOP);
    result = ENS160_Write8(hens160, ENS160_REG_COMMAND, ENS160_COMMAND_CLRGPR);
    
    if (hens160->debug_enabled && hens160->i2c_funcs.debug_print) {
        char msg[50];
        sprintf(msg, "ENS160_ClearCommand() result: %s", result == 0 ? "ok" : "nok");
        hens160->i2c_funcs.debug_print(msg);
    }
    
    if (hens160->i2c_funcs.delay_ms) {
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    }
    
    status = ENS160_Read8(hens160, ENS160_REG_DATA_STATUS);
    
    if (hens160->debug_enabled && hens160->i2c_funcs.debug_print) {
        char msg[50];
        sprintf(msg, "ENS160_ClearCommand() status: 0x%02X", status);
        hens160->i2c_funcs.debug_print(msg);
    }
    
    if (hens160->i2c_funcs.delay_ms) {
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    }
    
    return result == 0;
}

static bool ENS160_GetFirmware(ENS160_HandleTypeDef *hens160)
{
    uint8_t i2cbuf[3];
    uint8_t result;
    
    ENS160_ClearCommand(hens160);
    
    if (hens160->i2c_funcs.delay_ms) {
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    }
    
    result = ENS160_Write8(hens160, ENS160_REG_COMMAND, ENS160_COMMAND_GET_APPVER);
    result = ENS160_Read(hens160, ENS160_REG_GPR_READ_4, i2cbuf, 3);
    
    hens160->fw_ver_major = i2cbuf[0];
    hens160->fw_ver_minor = i2cbuf[1];
    hens160->fw_ver_build = i2cbuf[2];
    
    if (hens160->fw_ver_major > 6)
        hens160->rev_ens16x = 1;
    else
        hens160->rev_ens16x = 0;
    
    if (hens160->debug_enabled && hens160->i2c_funcs.debug_print) {
        char msg[100];
        sprintf(msg, "ENS160_GetFirmware() FW: %d.%d.%d, result: %s", 
                hens160->fw_ver_major, hens160->fw_ver_minor, hens160->fw_ver_build,
                result == 0 ? "ok" : "nok");
        hens160->i2c_funcs.debug_print(msg);
    }
    
    if (hens160->i2c_funcs.delay_ms) {
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    }
    
    return result == 0;
}

bool ENS160_SetMode(ENS160_HandleTypeDef *hens160, uint8_t mode)
{
    uint8_t result;
    
    // LP only valid for rev>0
    if ((mode == ENS160_OPMODE_LP) && (hens160->rev_ens16x == 0)) {
        result = 1;
    } else {
        result = ENS160_Write8(hens160, ENS160_REG_OPMODE, mode);
    }
    
    if (hens160->debug_enabled && hens160->i2c_funcs.debug_print) {
        char msg[50];
        sprintf(msg, "ENS160_SetMode() result: %s", result == 0 ? "ok" : "nok");
        hens160->i2c_funcs.debug_print(msg);
    }
    
    if (hens160->i2c_funcs.delay_ms) {
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    }
    
    return result == 0;
}

bool ENS160_InitCustomMode(ENS160_HandleTypeDef *hens160, uint16_t step_num)
{
    uint8_t result;
    
    if (step_num > 0) {
        hens160->step_count = step_num;
        
        result = ENS160_SetMode(hens160, ENS160_OPMODE_IDLE);
        result = ENS160_ClearCommand(hens160);
        result = ENS160_Write8(hens160, ENS160_REG_COMMAND, ENS160_COMMAND_SETSEQ);
    } else {
        result = 1;
    }
    
    if (hens160->i2c_funcs.delay_ms) {
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    }
    
    return result == 0;
}

bool ENS160_AddCustomStep(ENS160_HandleTypeDef *hens160, uint16_t time, 
                         bool measure_hp0, bool measure_hp1, bool measure_hp2, bool measure_hp3,
                         uint16_t temp_hp0, uint16_t temp_hp1, uint16_t temp_hp2, uint16_t temp_hp3)
{
    uint8_t seq_ack;
    uint8_t temp;
    
    if (hens160->debug_enabled && hens160->i2c_funcs.debug_print) {
        char msg[50];
        sprintf(msg, "ENS160_AddCustomStep() step: %d", hens160->step_count);
        hens160->i2c_funcs.debug_print(msg);
    }
    
    if (hens160->i2c_funcs.delay_ms) {
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    }
    
    temp = (uint8_t)(((time / 24) - 1) << 6);
    if (measure_hp0) temp |= 0x20;
    if (measure_hp1) temp |= 0x10;
    if (measure_hp2) temp |= 0x08;
    if (measure_hp3) temp |= 0x04;
    ENS160_Write8(hens160, ENS160_REG_GPR_WRITE_0, temp);
    
    temp = (uint8_t)(((time / 24) - 1) >> 2);
    ENS160_Write8(hens160, ENS160_REG_GPR_WRITE_1, temp);
    
    ENS160_Write8(hens160, ENS160_REG_GPR_WRITE_2, (uint8_t)(temp_hp0 / 2));
    ENS160_Write8(hens160, ENS160_REG_GPR_WRITE_3, (uint8_t)(temp_hp1 / 2));
    ENS160_Write8(hens160, ENS160_REG_GPR_WRITE_4, (uint8_t)(temp_hp2 / 2));
    ENS160_Write8(hens160, ENS160_REG_GPR_WRITE_5, (uint8_t)(temp_hp3 / 2));
    ENS160_Write8(hens160, ENS160_REG_GPR_WRITE_6, (uint8_t)(hens160->step_count - 1));
    
    if (hens160->step_count == 1) {
        ENS160_Write8(hens160, ENS160_REG_GPR_WRITE_7, 128);
    } else {
        ENS160_Write8(hens160, ENS160_REG_GPR_WRITE_7, 0);
    }
    
    if (hens160->i2c_funcs.delay_ms) {
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    }
    
    seq_ack = ENS160_Read8(hens160, ENS160_REG_GPR_READ_7);
    
    if (hens160->i2c_funcs.delay_ms) {
        hens160->i2c_funcs.delay_ms(ENS160_BOOTING);
    }
    
    if ((ENS160_SEQ_ACK_COMPLETE | hens160->step_count) != seq_ack) {
        hens160->step_count--;
        return false;
    } else {
        return true;
    }
}

bool ENS160_Measure(ENS160_HandleTypeDef *hens160, bool wait_for_new)
{
    uint8_t i2cbuf[8];
    uint8_t status;
    bool new_data = false;
    
//    if (hens160->debug_enabled && hens160->i2c_funcs.debug_print)
//        hens160->i2c_funcs.debug_print("ENS160_Measure() - Start measurement");
    
    if (wait_for_new)
    {
        uint32_t waited = 0;

        do
        {
            if (hens160->i2c_funcs.delay_ms)
                hens160->i2c_funcs.delay_ms(1);

            status = ENS160_Read8(hens160, ENS160_REG_DATA_STATUS);
            waited++;
        } while (!IS_NEWDAT(status) && waited < ENS160_DATA_WAIT_MS);
    }
    else
        status = ENS160_Read8(hens160, ENS160_REG_DATA_STATUS);
    
    if (IS_NEWDAT(status)) {
        new_data = true;
        ENS160_Read(hens160, ENS160_REG_DATA_AQI, i2cbuf, 7);
        hens160->data_aqi = i2cbuf[0];
        hens160->data_tvoc = i2cbuf[1] | ((uint16_t)i2cbuf[2] << 8);
        hens160->data_eco2 = i2cbuf[3] | ((uint16_t)i2cbuf[4] << 8);
        
        if (hens160->rev_ens16x > 0) {
            hens160->data_aqi500 = i2cbuf[5] | ((uint16_t)i2cbuf[6] << 8);
        } else {
            hens160->data_aqi500 = 0;
        }
    }
    
    return new_data;
}

bool ENS160_MeasureRaw(ENS160_HandleTypeDef *hens160, bool wait_for_new)
{
    uint8_t i2cbuf[8];
    uint8_t status;
    bool new_data = false;
    
//    if (hens160->debug_enabled && hens160->i2c_funcs.debug_print) {
//        hens160->i2c_funcs.debug_print("ENS160_MeasureRaw() - Start raw measurement");
//    }
    
    if (wait_for_new) {
        uint32_t waited = 0;

        do {
            if (hens160->i2c_funcs.delay_ms) {
                hens160->i2c_funcs.delay_ms(1);
            }

            status = ENS160_Read8(hens160, ENS160_REG_DATA_STATUS);
            waited++;
        } while (!IS_NEWGPR(status) && waited < ENS160_DATA_WAIT_MS);
    } else {
        status = ENS160_Read8(hens160, ENS160_REG_DATA_STATUS);
    }
    
    if (IS_NEWGPR(status)) {
        new_data = true;
        
        // Read raw resistance values
        ENS160_Read(hens160, ENS160_REG_GPR_READ_0, i2cbuf, 8);
        hens160->hp0_rs = CONVERT_RS_RAW2OHMS_F(i2cbuf[0] | ((uint32_t)i2cbuf[1] << 8));
        hens160->hp1_rs = CONVERT_RS_RAW2OHMS_F(i2cbuf[2] | ((uint32_t)i2cbuf[3] << 8));
        hens160->hp2_rs = CONVERT_RS_RAW2OHMS_F(i2cbuf[4] | ((uint32_t)i2cbuf[5] << 8));
        hens160->hp3_rs = CONVERT_RS_RAW2OHMS_F(i2cbuf[6] | ((uint32_t)i2cbuf[7] << 8));
        
        // Read baselines
        ENS160_Read(hens160, ENS160_REG_DATA_BL, i2cbuf, 8);
        hens160->hp0_bl = CONVERT_RS_RAW2OHMS_F(i2cbuf[0] | ((uint32_t)i2cbuf[1] << 8));
        hens160->hp1_bl = CONVERT_RS_RAW2OHMS_F(i2cbuf[2] | ((uint32_t)i2cbuf[3] << 8));
        hens160->hp2_bl = CONVERT_RS_RAW2OHMS_F(i2cbuf[4] | ((uint32_t)i2cbuf[5] << 8));
        hens160->hp3_bl = CONVERT_RS_RAW2OHMS_F(i2cbuf[6] | ((uint32_t)i2cbuf[7] << 8));
        
        hens160->misr = ENS160_Read8(hens160, ENS160_REG_DATA_MISR);
    }
    
    return new_data;
}

bool ENS160_SetEnvData(ENS160_HandleTypeDef *hens160, float temperature, float humidity)
{
    uint16_t t_data = (uint16_t)((temperature + 273.15f) * 64.0f);
    uint16_t rh_data = (uint16_t)(humidity * 512.0f);
    
    return ENS160_SetEnvData210(hens160, t_data, rh_data);
}

bool ENS160_SetEnvData210(ENS160_HandleTypeDef *hens160, uint16_t temp_raw, uint16_t rh_raw)
{
    uint8_t trh_in[4];
    
    trh_in[0] = temp_raw & 0xFF;
    trh_in[1] = (temp_raw >> 8) & 0xFF;
    trh_in[2] = rh_raw & 0xFF;
    trh_in[3] = (rh_raw >> 8) & 0xFF;
    
    uint8_t result = ENS160_Write(hens160, ENS160_REG_TEMP_IN, trh_in, 4);
    return result == 0;
}

// 数据获取函数
uint8_t ENS160_GetAQI(ENS160_HandleTypeDef *hens160)
{
    return hens160->data_aqi;
}

uint16_t ENS160_GetTVOC(ENS160_HandleTypeDef *hens160)
{
    return hens160->data_tvoc;
}
uint16_t ENS160_GetECO2(ENS160_HandleTypeDef *hens160)
{
    return hens160->data_eco2;
}

uint16_t ENS160_GetAQI500(ENS160_HandleTypeDef *hens160)
{
    return hens160->data_aqi500;
}

uint32_t ENS160_GetHP0Resistance(ENS160_HandleTypeDef *hens160)
{
    return hens160->hp0_rs;
}

uint32_t ENS160_GetHP1Resistance(ENS160_HandleTypeDef *hens160)
{
    return hens160->hp1_rs;
}

uint32_t ENS160_GetHP2Resistance(ENS160_HandleTypeDef *hens160)
{
    return hens160->hp2_rs;
}

uint32_t ENS160_GetHP3Resistance(ENS160_HandleTypeDef *hens160)
{
    return hens160->hp3_rs;
}

uint32_t ENS160_GetHP0Baseline(ENS160_HandleTypeDef *hens160)
{
    return hens160->hp0_bl;
}

uint32_t ENS160_GetHP1Baseline(ENS160_HandleTypeDef *hens160)
{
    return hens160->hp1_bl;
}

uint32_t ENS160_GetHP2Baseline(ENS160_HandleTypeDef *hens160)
{
    return hens160->hp2_bl;
}

uint32_t ENS160_GetHP3Baseline(ENS160_HandleTypeDef *hens160)
{
    return hens160->hp3_bl;
}

uint8_t ENS160_GetMISR(ENS160_HandleTypeDef *hens160)
{
    return hens160->misr;
}

uint8_t ENS160_GetRevision(ENS160_HandleTypeDef *hens160)
{
    return hens160->rev_ens16x;
}

bool ENS160_IsAvailable(ENS160_HandleTypeDef *hens160)
{
    return hens160->available;
}

// 内部I2C读写函数
static uint8_t ENS160_Read8(ENS160_HandleTypeDef *hens160, uint8_t reg)
{
    uint8_t ret;
    ENS160_Read(hens160, reg, &ret, 1);
    
    return ret;
}

static uint8_t ENS160_Read(ENS160_HandleTypeDef *hens160, uint8_t reg, uint8_t *buf, uint8_t num)
{
    if (hens160->i2c_funcs.i2c_read)
        return hens160->i2c_funcs.i2c_read(hens160->slave_addr, reg, buf, num);

    return 1; // Error
}

static uint8_t ENS160_Write8(ENS160_HandleTypeDef *hens160, uint8_t reg, uint8_t value)
{
    return ENS160_Write(hens160, reg, &value, 1);
}

static uint8_t ENS160_Write(ENS160_HandleTypeDef *hens160, uint8_t reg, uint8_t *buf, uint8_t num)
{
    if (hens160->i2c_funcs.i2c_write)
        return hens160->i2c_funcs.i2c_write(hens160->slave_addr, reg, buf, num);

    return 1; // Error
}

// 通过串口输出ENS160数据
void print_ens160_data(ENS160_HandleTypeDef *ens160)
{
    // AQI数据
    uint8_t aqi = ENS160_GetAQI(ens160);
    const char *aqi_desc = "";
    switch(aqi) {
        case 1: aqi_desc = "Excellent"; break;
        case 2: aqi_desc = "Good"; break;
        case 3: aqi_desc = "Moderate"; break;
        case 4: aqi_desc = "Poor"; break;
        case 5: aqi_desc = "Unhealthy"; break;
        default: aqi_desc = "Invalid"; break;
    }
    
    // char buffer[100];
    // snprintf(buffer, sizeof(buffer), 
    //         "ENS160 -> AQI: %d (%s), TVOC: %d ppb, eCO2: %d ppm, AQI500: %d",
    //         aqi, aqi_desc, 
    //         ENS160_GetTVOC(ens160),
    //         ENS160_GetECO2(ens160),
    //         ENS160_GetAQI500(ens160));
    // ens160_debug_print(buffer);
}

// 打印原始传感器数据
void print_raw_data(ENS160_HandleTypeDef *ens160)
{
    (void)ENS160_MeasureRaw(ens160, false);
}

// 空气质量评估函数
void evaluate_air_quality(ENS160_HandleTypeDef *ens160)
{
    uint16_t tvoc = ENS160_GetTVOC(ens160);
    uint16_t eco2 = ENS160_GetECO2(ens160);
    uint8_t aqi = ENS160_GetAQI(ens160);
    
    char buffer[100];
    char recommendation[100];
    
    // TVOC评估
    if (tvoc < 220)
        strcpy(recommendation, "空气质量良好");
    else if (tvoc < 660)
        strcpy(recommendation, "建议适度通风");
    else if (tvoc < 2200)
        strcpy(recommendation, "建议加强通风");
    else
        strcpy(recommendation, "需要立即通风");
    
    snprintf(buffer, sizeof(buffer), "ENS160 -> TVOC: %d ppb, eCO2: %d ppm - %s",
                                                    tvoc, eco2, recommendation);
    ens160_debug_print(buffer);
}

// 传感器状态监控
void check_sensor_status(ENS160_HandleTypeDef *ens160)
{
    char buffer[100];
    
    if (!ENS160_IsAvailable(ens160)) {
        snprintf(buffer, sizeof(buffer), "ENS160 -> 传感器未就绪!");
        ens160_debug_print(buffer);
        return;
    }
    
    // uint8_t revision = ENS160_GetRevision(ens160);
    // snprintf(buffer, sizeof(buffer), 
    //         "ENS160 -> 状态: %s, 版本: %s",
    //         ENS160_IsAvailable(ens160) ? "正常" : "异常",
    //         (revision == 0) ? "ENS160" : "ENS161");
    // ens160_debug_print(buffer);
}

void ENS160_Measure_Task(void)
{
    if (ENS160_Measure(&ens160, false)) {
        print_ens160_data(&ens160);
    }
}




