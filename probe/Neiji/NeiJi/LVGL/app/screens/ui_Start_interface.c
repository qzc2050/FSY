#include "ui_Start_interface.h"

#include "ui_start.h"

lv_obj_t *ui_Start_interface = NULL;

void ui_Start_interface_screen_init(void)
{
    ui_start_create();
    ui_Start_interface = lv_scr_act();
}

void ui_Start_interface_screen_destroy(void)
{
    if (ui_Start_interface != NULL) {
        lv_obj_del(ui_Start_interface);
        ui_Start_interface = NULL;
    }
}

void ui_Start_interface_screen_relocalize(void)
{
    /* placeholder: language switching can be wired here in later steps */
}
