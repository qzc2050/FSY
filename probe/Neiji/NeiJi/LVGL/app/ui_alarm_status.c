#include "ui_alarm_status.h"



#include "ui.h"

#include "ui_helpers.h"

#include "screens/ui_Main_Interface.h"

#include "alarm_output.h"

#include "geiger.h"

#include "language.h"

#include "main.h"

#include "lvgl.h"



#define RADIATION_COLOR_NORMAL   0xFDBB5EU

#define RADIATION_COLOR_HI       0xFF3333U

#define RADIATION_COLOR_LO       0xFF9900U

#define BLINK_MS_HI              400U

#define BLINK_MS_LO              800U



static lv_anim_t s_spin_anim;

static uint8_t s_alarm_blink_on;

static uint8_t s_radiation_alarm_ui;

static Alarm_Visual_State_t s_radiation_ui_vis;



static void radiation_spin_exec(void *obj, int32_t v)

{

    lv_img_set_angle((lv_obj_t *)obj, (int16_t)v);

}



static uint8_t radiation_spin_running(void)

{

    return (lv_anim_get(ui_radiation, radiation_spin_exec) != NULL) ? 1U : 0U;

}



static void radiation_spin_stop(void)

{

    if (ui_radiation == NULL) {

        return;

    }

    lv_anim_del(ui_radiation, NULL);

    lv_anim_del(ui_radiation, (lv_anim_exec_xcb_t)_ui_anim_callback_set_image_angle);

    lv_anim_del(ui_radiation, radiation_spin_exec);

    lv_img_set_angle(ui_radiation, 0);

}



static void radiation_show_static_normal(void)

{

    if (ui_radiation == NULL) {

        return;

    }

    lv_img_set_angle(ui_radiation, 0);

    lv_obj_set_style_img_recolor(ui_radiation, lv_color_hex(RADIATION_COLOR_NORMAL),

                                 LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_img_recolor_opa(ui_radiation, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_clear_flag(ui_radiation, LV_OBJ_FLAG_HIDDEN);

}



static void radiation_spin_start(void)

{

    if (ui_radiation == NULL) {

        return;

    }

    if (NEIJI_UI_RADIATION_SPIN == 0) {

        radiation_spin_stop();

        radiation_show_static_normal();

        return;

    }

    if (radiation_spin_running()) {

        return;

    }



    lv_anim_init(&s_spin_anim);

    lv_anim_set_var(&s_spin_anim, ui_radiation);

    lv_anim_set_exec_cb(&s_spin_anim, radiation_spin_exec);

    lv_anim_set_values(&s_spin_anim, 0, 3600);

    lv_anim_set_time(&s_spin_anim, 3000);

    lv_anim_set_repeat_count(&s_spin_anim, LV_ANIM_REPEAT_INFINITE);

    lv_anim_start(&s_spin_anim);

    radiation_show_static_normal();

}



static void radiation_dose_alarm_show(Alarm_Visual_State_t vis)

{

    uint32_t color;



    if (vis == ALARM_VIS_HI) {

        color = RADIATION_COLOR_HI;

    } else {

        color = RADIATION_COLOR_LO;

    }



    if (!s_radiation_alarm_ui || (s_radiation_ui_vis != vis)) {

        lv_anim_del(ui_radiation, radiation_spin_exec);

        lv_img_set_angle(ui_radiation, 0);

        s_radiation_alarm_ui = 1U;

        s_radiation_ui_vis = vis;

    }



    lv_obj_set_style_img_recolor(ui_radiation, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_img_recolor_opa(ui_radiation, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (s_alarm_blink_on) {

        lv_obj_clear_flag(ui_radiation, LV_OBJ_FLAG_HIDDEN);

    } else {

        lv_obj_add_flag(ui_radiation, LV_OBJ_FLAG_HIDDEN);

    }

}



static void sync_corner_icons(void)

{

    uint8_t th_zero = (sys_cfg.th_rh_rate == 0.0f) && (sys_cfg.th_rl_rate == 0.0f);

    uint8_t sound_on = sys_cfg.alarm_sound && (sys_cfg.alarm_volume > 0U) && !th_zero;

    uint8_t light_on = sys_cfg.alarm_light && !th_zero;



    if (sound_on) {

        _ui_flag_modify(ui_alarm_on, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);

        _ui_flag_modify(ui_alarm_off, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

    } else {

        _ui_flag_modify(ui_alarm_on, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

        _ui_flag_modify(ui_alarm_off, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);

    }



    if (light_on) {

        _ui_flag_modify(ui_led_on, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);

        _ui_flag_modify(ui_led_off, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

    } else {

        _ui_flag_modify(ui_led_on, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

        _ui_flag_modify(ui_led_off, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);

    }

}



static void alarm_status_timer_cb(lv_timer_t *timer)

{

    (void)timer;

    Ui_AlarmStatus_Refresh();

}



void Ui_AlarmStatus_Init(void)

{

    if (ui_Panel_doserate == NULL) {

        return;

    }



    s_radiation_ui_vis = ALARM_VIS_NORMAL;

    radiation_spin_stop();

    radiation_spin_start();

    sync_corner_icons();

#if NEIJI_UI_RADIATION_SPIN
    lv_timer_create(alarm_status_timer_cb, 200, NULL);
#endif

}



void Ui_AlarmStatus_Refresh(void)

{

    static uint32_t blink_tk;

    Alarm_Visual_State_t vis;



    if (ui_radiation == NULL) {

        return;

    }

    if (NEIJI_UI_RADIATION_SPIN == 0) {

        radiation_spin_stop();

        return;

    }



    sync_corner_icons();

    vis = Alarm_Output_GetVisualState();



    if (vis == ALARM_VIS_HI || vis == ALARM_VIS_LO) {

        uint32_t blink_ms = (vis == ALARM_VIS_HI) ? BLINK_MS_HI : BLINK_MS_LO;



        if ((lv_tick_get() - blink_tk) >= blink_ms) {

            blink_tk = lv_tick_get();

            s_alarm_blink_on = (uint8_t)!s_alarm_blink_on;

        }

        radiation_dose_alarm_show(vis);

    } else {

        if (s_radiation_alarm_ui) {

            s_radiation_alarm_ui = 0U;

            s_radiation_ui_vis = ALARM_VIS_NORMAL;

            s_alarm_blink_on = 1U;

            lv_img_set_angle(ui_radiation, 0);

        }

        radiation_spin_start();

    }

}


