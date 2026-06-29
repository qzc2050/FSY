#include "lv_port_indev.h"

#include "key.h"
#include "lvgl.h"
#include "uart_diag.h"

static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

static lv_indev_t *s_indev_keypad;
static lv_group_t *s_indev_group;

/* 单键锁存：同一物理按键在一个按下→释放周期内只产生一次 LVGL 事件 */
static KEY_ID_t s_latched_key = KEY_ID_MAX;

static uint32_t key_to_lv(KEY_ID_t id)
{
    switch (id) {
    case KEY_ID_UP:
        return LV_KEY_UP;
    case KEY_ID_DOWN:
        return LV_KEY_DOWN;
    case KEY_ID_LEFT:
        return LV_KEY_LEFT;
    case KEY_ID_RIGHT:
        return LV_KEY_RIGHT;
    case KEY_ID_EN:
        return LV_KEY_ENTER;
    case KEY_ID_SET:
        return LV_KEY_ESC;
    default:
        return 0U;
    }
}

lv_group_t *lv_port_indev_get_group(void)
{
    return s_indev_group;
}

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    s_indev_group = lv_group_create();
    lv_group_set_default(s_indev_group);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;
    s_indev_keypad = lv_indev_drv_register(&indev_drv);
    lv_indev_set_group(s_indev_keypad, s_indev_group);
}

static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static uint32_t last_key = 0U;
    KEY_ID_t id;
    uint32_t lv_key;

    (void)indev_drv;

    id = KEY_GetActiveKey();
    if (id == KEY_ID_MAX) {
        if (s_latched_key != KEY_ID_MAX) {
            /* Convert navigation keys for the release event */
            uint32_t rel_key = key_to_lv(s_latched_key);
            if (!lv_group_get_editing(s_indev_group)) {
                if (rel_key == LV_KEY_UP)   rel_key = LV_KEY_PREV;
                if (rel_key == LV_KEY_DOWN) rel_key = LV_KEY_NEXT;
            }
            data->state = LV_INDEV_STATE_RELEASED;
            data->key   = rel_key;
            last_key    = rel_key;
            s_latched_key = KEY_ID_MAX;
            return;
        }
        data->state = LV_INDEV_STATE_RELEASED;
        data->key   = last_key;
        return;
    }

    if (id == s_latched_key) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key   = last_key;
        return;
    }

    /* New key press */
    s_latched_key = id;
    lv_key = key_to_lv(id);

    /* In navigation mode, remap UP/DOWN → PREV/NEXT for LVGL group switching */
    if (!lv_group_get_editing(s_indev_group)) {
        if (lv_key == LV_KEY_UP)   lv_key = LV_KEY_PREV;
        if (lv_key == LV_KEY_DOWN) lv_key = LV_KEY_NEXT;
    }

    if (lv_key != 0U) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key   = lv_key;
        last_key    = lv_key;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key   = last_key;
    }
}