#include "lv_port_indev.h"

#include "key.h"
#include "lvgl.h"

static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

static lv_indev_t *s_indev_keypad;
static lv_group_t *s_indev_group;

static KEY_ID_t key_poll_active(void)
{
    if (KEY_IsActive(KEY_ID_UP)) {
        return KEY_ID_UP;
    }
    if (KEY_IsActive(KEY_ID_DOWN)) {
        return KEY_ID_DOWN;
    }
    if (KEY_IsActive(KEY_ID_LEFT)) {
        return KEY_ID_LEFT;
    }
    if (KEY_IsActive(KEY_ID_RIGHT)) {
        return KEY_ID_RIGHT;
    }
    if (KEY_IsActive(KEY_ID_EN)) {
        return KEY_ID_EN;
    }
    if (KEY_IsActive(KEY_ID_SET)) {
        return KEY_ID_SET;
    }

    return KEY_ID_MAX;
}

static uint32_t key_to_lv(KEY_ID_t id)
{
    switch (id) {
    case KEY_ID_UP:
        return LV_KEY_PREV;
    case KEY_ID_DOWN:
        return LV_KEY_NEXT;
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

    id = key_poll_active();
    if (id != KEY_ID_MAX) {
        lv_key = key_to_lv(id);
        if (lv_key != 0U) {
            data->state = LV_INDEV_STATE_PRESSED;
            last_key = lv_key;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->key = last_key;
}
