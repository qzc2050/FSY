#include "ui.h"

#include "language.h"
#include "screens/ui_Main_Interface.h"
#include "ui_alarm_status.h"

lv_anim_t *opaon_Animation(lv_obj_t *TargetObject, int delay)
{
    (void)delay;

    lv_obj_set_style_opa(TargetObject, LV_OPA_COVER, 0);
    return NULL;
}

void ui_init(void)
{
    LV_EVENT_GET_COMP_CHILD = lv_event_register_id();

    lv_disp_t * dispp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                               true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    ui_Main_Interface_screen_init();

    /* Bind hardware config values to UI labels */
    ui_main_bind_settings();

    language_init();
    update_all_ui_texts();
    Ui_AlarmStatus_Init();
}
