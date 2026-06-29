#ifndef NEIJI_UI_H
#define NEIJI_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#include "ui_helpers.h"
#include "language.h"
#include "components/ui_comp.h"
#include "components/ui_comp_hook.h"

LV_IMG_DECLARE(ui_img_532493655);
LV_IMG_DECLARE(ui_img_545021016);
LV_IMG_DECLARE(ui_img_1523901040);
LV_IMG_DECLARE(ui_img_1464373361);
LV_IMG_DECLARE(ui_img_pm2_5_png);
LV_IMG_DECLARE(ui_img_1985833602);
LV_IMG_DECLARE(ui_img_1779444627);
LV_IMG_DECLARE(ui_img_602667123);
LV_IMG_DECLARE(ui_img_834997458);
LV_IMG_DECLARE(ui_img_1957260177);
LV_IMG_DECLARE(ui_img_1777030953);
LV_IMG_DECLARE(ui_img_display_png);
LV_IMG_DECLARE(ui_img_2138131786);
LV_IMG_DECLARE(ui_img_182239421);
LV_IMG_DECLARE(ui_img_1640302447);
LV_IMG_DECLARE(ui_img_820325126);
LV_IMG_DECLARE(ui_img_966723152);
LV_IMG_DECLARE(ui_img_1261263291);
LV_IMG_DECLARE(ui_img_373985928);

LV_FONT_DECLARE(ui_font_hansanbold128);
LV_FONT_DECLARE(ui_font_hansanbold24);
LV_FONT_DECLARE(ui_font_hansanbold28);
LV_FONT_DECLARE(ui_font_hansanbold32);
LV_FONT_DECLARE(ui_font_hansanbold36);
LV_FONT_DECLARE(ui_font_hansanbold48);
LV_FONT_DECLARE(ui_font_hansanbold64);

lv_anim_t *opaon_Animation(lv_obj_t *TargetObject, int delay);

void ui_init(void);

#ifdef __cplusplus
}
#endif

#endif
