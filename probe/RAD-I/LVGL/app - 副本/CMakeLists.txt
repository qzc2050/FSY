SET(SOURCES components/ui_comp_mainmenu.c
    components/ui_comp.c
    screens/ui_Start_interface.c
    screens/ui_Main_Interface.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_532493655.c
    images/ui_img_545021016.c
    images/ui_img_pm2_5_png.c
    images/ui_img_1464373361.c
    images/ui_img_1523901040.c
    images/ui_img_1985833602.c
    images/ui_img_1779444627.c
    images/ui_img_602667123.c
    images/ui_img_834997458.c
    images/ui_img_1957260177.c
    images/ui_img_1261263291.c
    images/ui_img_1777030953.c
    images/ui_img_display_png.c
    images/ui_img_2138131786.c
    images/ui_img_182239421.c
    images/ui_img_373985928.c
    images/ui_img_1640302447.c
    images/ui_img_820325126.c
    images/ui_img_966723152.c
    fonts/ui_font_hansanbold128.c
    fonts/ui_font_hansanbold28.c
    fonts/ui_font_hansanbold32.c
    fonts/ui_font_hansanbold48.c
    fonts/ui_font_hansanbold64.c)

add_library(ui ${SOURCES})
