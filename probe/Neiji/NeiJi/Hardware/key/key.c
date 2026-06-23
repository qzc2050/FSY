#include "key.h"

#include "main.h"
#include "stm32h7xx_hal_adc.h"
#include "stm32h7xx_hal_adc_ex.h"

#include <stdio.h>

#define KEY_PRESS_GPIO_Port      GPIOA
#define KEY_PRESS_Pin            GPIO_PIN_4
#define KEY_UP_DOWN_GPIO_Port    GPIOC
#define KEY_UP_DOWN_Pin          GPIO_PIN_4
#define KEY_LEFT_RIGHT_GPIO_Port GPIOC
#define KEY_LEFT_RIGHT_Pin       GPIO_PIN_5
#define KEY_SET_GPIO_Port        GPIOH
#define KEY_SET_Pin              GPIO_PIN_7

#define KEY_ADC_NUM              3U
#define KEY_ADC_AVG_SAMPLES      4U
#define KEY_DEBOUNCE_SAMPLES     3U

/* 按下阈值（RAD-I joystick.c） */
#define KEY_UP_PRESS_TH          3500
#define KEY_DOWN_PRESS_TH        (-5000)
#define KEY_LEFT_PRESS_TH        (-1500)
#define KEY_RIGHT_PRESS_TH       5000
/* EN 中心按压：与方向键同量级；原 60000 超出 16bit ADC 有效范围 */
#define KEY_EN_ABS_PRESS_TH      5000
#define KEY_EN_ABS_RELEASE_TH    2500

/* 释放滞回：回到中性区才松开，避免阈值附近抖动 */
#define KEY_UP_RELEASE_TH        2000
#define KEY_DOWN_RELEASE_TH      (-3000)
#define KEY_LEFT_RELEASE_TH      (-800)
#define KEY_RIGHT_RELEASE_TH     3500

#define KEY_ADC_IDX_UPDOWN       0U
#define KEY_ADC_IDX_LEFTRIGHT    1U
#define KEY_ADC_IDX_PRESS        2U

static ADC_HandleTypeDef s_hadc2;
static uint16_t s_adc_baseline[KEY_ADC_NUM];

static bool s_raw_active[KEY_ID_MAX];
static bool s_stable_active[KEY_ID_MAX];
static bool s_prev_stable[KEY_ID_MAX];
static uint8_t s_debounce_cnt[KEY_ID_MAX];
static bool s_press_pending[KEY_ID_MAX];

static bool s_latch_up;
static bool s_latch_down;
static bool s_latch_left;
static bool s_latch_right;
static bool s_latch_en;
static int32_t s_last_diff_press;

static void key_adc_clock_init(void);
static void key_adc_gpio_init(void);
static void key_set_gpio_init(void);
static void key_adc_periph_init(void);
static uint16_t key_adc_read_channel(uint32_t channel);
static uint16_t key_adc_read_avg(uint32_t channel);
static void key_adc_sample_all(uint16_t out[KEY_ADC_NUM]);
static void key_calibrate_baseline(void);
static int32_t key_adc_diff(uint8_t idx, uint16_t val);
static void key_update_axis_latch(int32_t diff_ud, int32_t diff_lr, int32_t diff_press);
static void key_update_raw_gpio(void);
static void key_debounce_all(void);

void HAL_ADC_MspInit(ADC_HandleTypeDef *adcHandle)
{
    if (adcHandle->Instance != ADC2) {
        return;
    }

    key_adc_gpio_init();
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *adcHandle)
{
    if (adcHandle->Instance != ADC2) {
        return;
    }

    __HAL_RCC_ADC12_CLK_DISABLE();
    HAL_GPIO_DeInit(KEY_PRESS_GPIO_Port, KEY_PRESS_Pin);
    HAL_GPIO_DeInit(KEY_UP_DOWN_GPIO_Port, KEY_UP_DOWN_Pin | KEY_LEFT_RIGHT_Pin);
}

static void key_adc_clock_init(void)
{
    RCC_PeriphCLKInitTypeDef clk = {0};

    clk.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    clk.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
    if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
        Error_Handler();
    }
}

static void key_adc_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin = KEY_PRESS_Pin;
    HAL_GPIO_Init(KEY_PRESS_GPIO_Port, &gpio);

    gpio.Pin = KEY_UP_DOWN_Pin | KEY_LEFT_RIGHT_Pin;
    HAL_GPIO_Init(KEY_UP_DOWN_GPIO_Port, &gpio);
}

static void key_set_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOH_CLK_ENABLE();

    gpio.Pin = KEY_SET_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY_SET_GPIO_Port, &gpio);
}

static void key_adc_periph_init(void)
{
    s_hadc2.Instance = ADC2;
    s_hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV16;
    s_hadc2.Init.Resolution = ADC_RESOLUTION_16B;
    s_hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
    s_hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    s_hadc2.Init.LowPowerAutoWait = DISABLE;
    s_hadc2.Init.ContinuousConvMode = DISABLE;
    s_hadc2.Init.NbrOfConversion = 1;
    s_hadc2.Init.DiscontinuousConvMode = DISABLE;
    s_hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    s_hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    s_hadc2.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    s_hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    s_hadc2.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    s_hadc2.Init.OversamplingMode = DISABLE;

    if (HAL_ADC_Init(&s_hadc2) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_ADCEx_Calibration_Start(&s_hadc2, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        Error_Handler();
    }
}

static uint16_t key_adc_read_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef ch = {0};

    ch.Channel = channel;
    ch.Rank = ADC_REGULAR_RANK_1;
    ch.SamplingTime = ADC_SAMPLETIME_64CYCLES_5;
    ch.SingleDiff = ADC_SINGLE_ENDED;
    ch.OffsetNumber = ADC_OFFSET_NONE;
    ch.Offset = 0;
    ch.OffsetSignedSaturation = DISABLE;

    if (HAL_ADC_ConfigChannel(&s_hadc2, &ch) != HAL_OK) {
        return 0U;
    }

    if (HAL_ADC_Start(&s_hadc2) != HAL_OK) {
        return 0U;
    }
    if (HAL_ADC_PollForConversion(&s_hadc2, 10U) != HAL_OK) {
        (void)HAL_ADC_Stop(&s_hadc2);
        return 0U;
    }

    uint16_t val = (uint16_t)HAL_ADC_GetValue(&s_hadc2);
    (void)HAL_ADC_Stop(&s_hadc2);
    return val;
}

static uint16_t key_adc_read_avg(uint32_t channel)
{
    uint32_t sum = 0U;

    for (uint8_t i = 0U; i < KEY_ADC_AVG_SAMPLES; i++) {
        sum += key_adc_read_channel(channel);
    }

    return (uint16_t)(sum / KEY_ADC_AVG_SAMPLES);
}

static void key_adc_sample_all(uint16_t out[KEY_ADC_NUM])
{
    out[KEY_ADC_IDX_UPDOWN] = key_adc_read_avg(ADC_CHANNEL_4);
    out[KEY_ADC_IDX_LEFTRIGHT] = key_adc_read_avg(ADC_CHANNEL_8);
    out[KEY_ADC_IDX_PRESS] = key_adc_read_avg(ADC_CHANNEL_18);
}

static int32_t key_adc_diff(uint8_t idx, uint16_t val)
{
    return (int32_t)val - (int32_t)s_adc_baseline[idx];
}

static void key_calibrate_baseline(void)
{
    uint32_t sum[KEY_ADC_NUM] = {0U};
    uint16_t sample[KEY_ADC_NUM];

    for (uint8_t n = 0U; n < 20U; n++) {
        key_adc_sample_all(sample);
        for (uint8_t i = 0U; i < KEY_ADC_NUM; i++) {
            sum[i] += sample[i];
        }
        HAL_Delay(5U);
    }

    for (uint8_t i = 0U; i < KEY_ADC_NUM; i++) {
        s_adc_baseline[i] = (uint16_t)(sum[i] / 20U);
    }

    printf("[KEY] baseline PC4=%u PC5=%u PA4=%u\r\n",
           (unsigned)s_adc_baseline[KEY_ADC_IDX_UPDOWN],
           (unsigned)s_adc_baseline[KEY_ADC_IDX_LEFTRIGHT],
           (unsigned)s_adc_baseline[KEY_ADC_IDX_PRESS]);
}

static int32_t key_abs32(int32_t v)
{
    return (v < 0) ? -v : v;
}

static void key_update_axis_latch(int32_t diff_ud, int32_t diff_lr, int32_t diff_press)
{
    if (s_latch_up) {
        if (diff_ud < KEY_UP_RELEASE_TH) {
            s_latch_up = false;
        }
    } else if (s_latch_down) {
        if (diff_ud > KEY_DOWN_RELEASE_TH) {
            s_latch_down = false;
        }
    } else if (diff_ud > KEY_UP_PRESS_TH) {
        s_latch_up = true;
    } else if (diff_ud < KEY_DOWN_PRESS_TH) {
        s_latch_down = true;
    }

    if (s_latch_left) {
        if (diff_lr > KEY_LEFT_RELEASE_TH) {
            s_latch_left = false;
        }
    } else if (s_latch_right) {
        if (diff_lr < KEY_RIGHT_RELEASE_TH) {
            s_latch_right = false;
        }
    } else if (diff_lr < KEY_LEFT_PRESS_TH) {
        s_latch_left = true;
    } else if (diff_lr > KEY_RIGHT_PRESS_TH) {
        s_latch_right = true;
    }

    if (s_latch_en) {
        if (key_abs32(diff_press) < KEY_EN_ABS_RELEASE_TH) {
            s_latch_en = false;
        }
    } else if (key_abs32(diff_press) > KEY_EN_ABS_PRESS_TH) {
        s_latch_en = true;
    }
}

static void key_update_raw_gpio(void)
{
    s_raw_active[KEY_ID_SET] =
        (HAL_GPIO_ReadPin(KEY_SET_GPIO_Port, KEY_SET_Pin) == GPIO_PIN_RESET);
}

static void key_debounce_all(void)
{
    for (uint8_t i = 0U; i < (uint8_t)KEY_ID_MAX; i++) {
        if (s_raw_active[i] == s_stable_active[i]) {
            s_debounce_cnt[i] = 0U;
            continue;
        }

        if (s_debounce_cnt[i] < KEY_DEBOUNCE_SAMPLES) {
            s_debounce_cnt[i]++;
        }

        if (s_debounce_cnt[i] >= KEY_DEBOUNCE_SAMPLES) {
            s_prev_stable[i] = s_stable_active[i];
            s_stable_active[i] = s_raw_active[i];
            s_debounce_cnt[i] = 0U;

            if (s_stable_active[i] && !s_prev_stable[i]) {
                s_press_pending[i] = true;
            }
        }
    }
}

void KEY_Init(void)
{
    for (uint8_t i = 0U; i < (uint8_t)KEY_ID_MAX; i++) {
        s_raw_active[i] = false;
        s_stable_active[i] = false;
        s_prev_stable[i] = false;
        s_debounce_cnt[i] = 0U;
        s_press_pending[i] = false;
    }

    s_latch_up = false;
    s_latch_down = false;
    s_latch_left = false;
    s_latch_right = false;
    s_latch_en = false;

    key_adc_clock_init();
    key_set_gpio_init();
    key_adc_periph_init();
    key_calibrate_baseline();

    printf("[KEY] init OK (EN=PA4 UP/DOWN=PC4 LEFT/RIGHT=PC5 SET=PH7)\r\n");
}

void KEY_Scan(void)
{
    uint16_t adc[KEY_ADC_NUM];
    int32_t diff_ud;
    int32_t diff_lr;
    int32_t diff_press;

    for (uint8_t i = 0U; i < (uint8_t)KEY_ID_MAX; i++) {
        s_raw_active[i] = false;
    }

    key_adc_sample_all(adc);
    diff_ud = key_adc_diff(KEY_ADC_IDX_UPDOWN, adc[KEY_ADC_IDX_UPDOWN]);
    diff_lr = key_adc_diff(KEY_ADC_IDX_LEFTRIGHT, adc[KEY_ADC_IDX_LEFTRIGHT]);
    diff_press = key_adc_diff(KEY_ADC_IDX_PRESS, adc[KEY_ADC_IDX_PRESS]);
    s_last_diff_press = diff_press;

    key_update_axis_latch(diff_ud, diff_lr, diff_press);

    s_raw_active[KEY_ID_UP] = s_latch_up;
    s_raw_active[KEY_ID_DOWN] = s_latch_down;
    s_raw_active[KEY_ID_LEFT] = s_latch_left;
    s_raw_active[KEY_ID_RIGHT] = s_latch_right;
    s_raw_active[KEY_ID_EN] = s_latch_en;

    key_update_raw_gpio();
    key_debounce_all();
}

bool KEY_IsActive(KEY_ID_t id)
{
    if (id >= KEY_ID_MAX) {
        return false;
    }
    return s_stable_active[id];
}

bool KEY_GetPressEvent(KEY_ID_t *id)
{
    if (id == NULL) {
        return false;
    }

    for (uint8_t i = 0U; i < (uint8_t)KEY_ID_MAX; i++) {
        if (s_press_pending[i]) {
            s_press_pending[i] = false;
            *id = (KEY_ID_t)i;
            return true;
        }
    }

    return false;
}

int32_t KEY_GetPressAdcDiff(void)
{
    return s_last_diff_press;
}

KEY_ID_t KEY_GetActiveKey(void)
{
    for (uint8_t i = 0U; i < (uint8_t)KEY_ID_MAX; i++) {
        if (s_stable_active[i]) {
            return (KEY_ID_t)i;
        }
    }
    return KEY_ID_MAX;
}

const char *KEY_Name(KEY_ID_t id)
{
    switch (id) {
    case KEY_ID_EN:    return "EN";
    case KEY_ID_UP:    return "UP";
    case KEY_ID_DOWN:  return "DOWN";
    case KEY_ID_LEFT:  return "LEFT";
    case KEY_ID_RIGHT: return "RIGHT";
    case KEY_ID_SET:   return "SET";
    default:           return "?";
    }
}
