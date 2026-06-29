#include "key.h"

#include "main.h"
#include <stdio.h>
#include <string.h>

/* ---- 4-button GPIO pins ---- */
#define KEY_PIN_HOME  GPIO_PIN_4
#define KEY_PORT_HOME GPIOA

#define KEY_PIN_UP    GPIO_PIN_7
#define KEY_PORT_UP   GPIOH

#define KEY_PIN_DOWN  GPIO_PIN_5
#define KEY_PORT_DOWN GPIOC

#define KEY_PIN_OK    GPIO_PIN_4
#define KEY_PORT_OK   GPIOC

/* Active-low with internal pull-up */
#define KEY_ACTIVE_LEVEL  GPIO_PIN_RESET

#define KEY_DEBOUNCE_MS  30U

typedef struct {
    GPIO_TypeDef *port;
    uint16_t       pin;
    bool           raw;
    bool           stable;
    bool           prev_stable;
    uint32_t       debounce_tick;
    bool           press_pending;
} key_slot_t;

static key_slot_t s_keys[KEY_ID_MAX];

void KEY_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    memset(s_keys, 0, sizeof(s_keys));

    /* --- HOME = PA4 --- */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin  = KEY_PIN_HOME;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY_PORT_HOME, &gpio);

    s_keys[KEY_ID_SET].port = KEY_PORT_HOME;
    s_keys[KEY_ID_SET].pin  = KEY_PIN_HOME;

    /* --- UP = PH7 --- */
    __HAL_RCC_GPIOH_CLK_ENABLE();
    gpio.Pin  = KEY_PIN_UP;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY_PORT_UP, &gpio);

    s_keys[KEY_ID_UP].port = KEY_PORT_UP;
    s_keys[KEY_ID_UP].pin  = KEY_PIN_UP;

    /* --- DOWN = PC5 --- */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Pin  = KEY_PIN_DOWN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY_PORT_DOWN, &gpio);

    s_keys[KEY_ID_DOWN].port = KEY_PORT_DOWN;
    s_keys[KEY_ID_DOWN].pin  = KEY_PIN_DOWN;

    /* --- OK = PC4 --- */
    gpio.Pin  = KEY_PIN_OK;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY_PORT_OK, &gpio);

    s_keys[KEY_ID_EN].port = KEY_PORT_OK;
    s_keys[KEY_ID_EN].pin  = KEY_PIN_OK;

    printf("[KEY] init OK (HOME=PA4 UP=PH7 DOWN=PC5 OK=PC4)\r\n");
}

void KEY_Scan(void)
{
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < KEY_ID_MAX; i++) {
        if (s_keys[i].port == NULL) {
            continue;  /* unused slot */
        }

        bool raw = (HAL_GPIO_ReadPin(s_keys[i].port, s_keys[i].pin) == KEY_ACTIVE_LEVEL);
        s_keys[i].raw = raw;

        if (raw != s_keys[i].stable) {
            if (s_keys[i].debounce_tick == 0) {
                s_keys[i].debounce_tick = now;
            } else if (now - s_keys[i].debounce_tick >= KEY_DEBOUNCE_MS) {
                s_keys[i].prev_stable = s_keys[i].stable;
                s_keys[i].stable = raw;
                s_keys[i].debounce_tick = 0;
                if (raw && !s_keys[i].prev_stable) {
                    s_keys[i].press_pending = true;
                }
            }
        } else {
            s_keys[i].debounce_tick = 0;
        }
    }
}

bool KEY_IsActive(KEY_ID_t id)
{
    if (id >= KEY_ID_MAX) return false;
    if (s_keys[id].port == NULL) return false;
    return s_keys[id].stable;
}

bool KEY_GetPressEvent(KEY_ID_t *id)
{
    if (id == NULL) return false;

    for (int i = 0; i < KEY_ID_MAX; i++) {
        if (s_keys[i].press_pending) {
            s_keys[i].press_pending = false;
            *id = (KEY_ID_t)i;
            return true;
        }
    }
    return false;
}

int32_t KEY_GetPressAdcDiff(void)
{
    return 0;
}

KEY_ID_t KEY_GetActiveKey(void)
{
    for (int i = 0; i < KEY_ID_MAX; i++) {
        if (s_keys[i].stable) {
            return (KEY_ID_t)i;
        }
    }
    return KEY_ID_MAX;
}

const char *KEY_Name(KEY_ID_t id)
{
    switch (id) {
    case KEY_ID_EN:    return "OK";
    case KEY_ID_UP:    return "UP";
    case KEY_ID_DOWN:  return "DOWN";
    case KEY_ID_LEFT:  return "LEFT";
    case KEY_ID_RIGHT: return "RIGHT";
    case KEY_ID_SET:   return "HOME";
    default:           return "?";
    }
}
