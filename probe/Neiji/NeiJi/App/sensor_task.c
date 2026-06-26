#include "sensor_task.h"
#include "aht20.h"
#include "bmp280.h"
#include "ens160.h"
#include "pcf85063.h"
#include "pm25.h"
#include "fsy_regmap.h"
#include "cmsis_os.h"

#define SENSOR_POLL_MS  1000U

Environment_Data_t env_data = {0};

static void SensorTask(void *argument);

static osThreadId_t sensorTaskHandle;
static const osThreadAttr_t sensorTaskAttributes = {
    .name = "sensorTask",
    .stack_size = 384 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

void Sensor_TaskInit(void)
{
    sensorTaskHandle = osThreadNew(SensorTask, NULL, &sensorTaskAttributes);
}

static void SensorTask(void *argument)
{
    AHT20_Data_t aht;
    BMP280_Data_t bmp;
    ENS160_Data_t ens;
    PM25_Data_t pm25;
    Pcf85063_DateTime_t rtc;

    (void)argument;

    osDelay(300);
    Pcf85063_Init();
    PM25_Rx_Start();
    ENS160_Init();
    AHT20_Init();
    BMP280_Init();

    for (;;) {
        (void)Pcf85063_GetTime(&rtc);

        AHT20_Update();
        BMP280_Update();
        AHT20_GetData(&aht);
        if (aht.online != 0U) {
            ENS160_SetCompensation(aht.temperature_c, aht.humidity_rh);
        }
        ENS160_Update();

        AHT20_GetData(&aht);
        BMP280_GetData(&bmp);
        ENS160_GetData(&ens);
        PM25_GetData(&pm25);

        if (Pcf85063_GetTime(&rtc) == 0) {
            env_data.dt = rtc;
        }

        // 不依赖 online 标志，只要值在合理范围就更新
        if (aht.temperature_c >= -40.0f && aht.temperature_c <= 85.0f) {
            env_data.temperature = aht.temperature_c;
        }
        if (aht.humidity_rh >= 0.0f && aht.humidity_rh <= 100.0f) {
            env_data.humidity = aht.humidity_rh;
        }

        if (bmp.online != 0U) {
            env_data.baro = bmp.pressure_pa;
        }
        if (ens.online != 0U) {
            env_data.CO2 = ens.eco2;
        }
        if (pm25.online != 0U) {
            env_data.PM2_5 = pm25.pm2_5;
        } else {
            env_data.PM2_5 = 0U;
        }

        Fsy_Regmap_UpdateEnv(&aht, &bmp, &ens, &pm25);

        osDelay(SENSOR_POLL_MS);
    }
}
