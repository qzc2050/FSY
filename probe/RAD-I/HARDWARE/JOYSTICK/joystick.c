#include "joystick.h"
#include "main.h"
#include "adc.h"
#include "key.h"

#include "lvgl.h"
#include "ui.h"

//#define KEY_LEFT_ADC_TH      -1500
//#define KEY_RIGHT_ADC_TH     5000
//#define KEY_PREV_ADC_TH      3500
//#define KEY_NEXT_ADC_TH      -5000
//#define KEY_PRESS_ADC_TH     60000

//#define KEY_ADC_NUM          3


//uint16_t init_val[KEY_ADC_NUM] = {0};



//void button_adc_val_init(void)
//{
//    int32_t adc_val = 0;
//    
//    for(uint8_t i = 0;i < KEY_ADC_NUM;i++)
//    {
//        adc_val = 0;
//        for(uint8_t j = 0;j < 20;j++)
//        {
//            adc_val += get_adc_data(i);
//            // HAL_Delay(5);
//            vTaskDelay(5);
//        }
//        init_val[i] = adc_val / 20;
//        printf("Button %s 初始值: %d\r\n", (i < 1) ? "上下": (i < 2) ? "左右" : "按压", init_val[i]);
//    }
//}



uint8_t get_button_state(void)
{
    lv_group_t *def_g;

    switch(KEY_GetPressedKey())
    {
        case KEY_ID_UP: 
            // printf("上!!\r\n");
            def_g = lv_group_get_default();
            if(def_g == volume_set_g || def_g == hth_set_g \
                || def_g == lth_set_g || def_g == bright_set_g \
                || def_g == datetime_set_g || def_g == about_set_g)
                return KEY_STATE_UP;
            else
                return KEY_STATE_PREV;
        case KEY_ID_DOWN: 
            // printf("下!!\r\n");
            def_g = lv_group_get_default();
            if(def_g == volume_set_g || def_g == hth_set_g \
                || def_g == lth_set_g || def_g == bright_set_g \
                || def_g == datetime_set_g || def_g == about_set_g)
                return KEY_STATE_DOWN;
            else
                return KEY_STATE_NEXT;
        case KEY_ID_RETURN: 
            // printf("左!!\r\n");
            // return KEY_STATE_LEFT;
            return KEY_STATE_ESC;
        case KEY_ID_OK: 
            // printf("右!!\r\n");
            def_g = lv_group_get_default();
            if(def_g == hth_set_g || def_g == lth_set_g || def_g == datetime_set_g)
                return KEY_STATE_NEXT;
            else
                return KEY_STATE_ENTER;
            // break;
        default: break;
    }
    return KEY_STATE_NULL;




//    int diff_val = 0;
    
//    for(int8_t i = KEY_ADC_NUM;i >= 0;i--)
//    {
//        uint16_t data = get_adc_data(i);
//        
//        diff_val = data - init_val[i];
//        switch(i)
//        {
//            case 0: 
//                if(diff_val > KEY_PREV_ADC_TH)
//                {
////                    printf("上!!\r\n");
//                    lv_group_t *def_g;
//                    def_g = lv_group_get_default();
//                    if(def_g == volume_set_g || def_g == hth_set_g \
//                        || def_g == lth_set_g || def_g == bright_set_g \
//                        || def_g == datetime_set_g || def_g == about_set_g)
//                        return KEY_STATE_UP;
//                    else
//                        return KEY_STATE_PREV;
//                }
//                else if(diff_val < KEY_NEXT_ADC_TH)
//                {
////                    printf("下!!\r\n");
//                    lv_group_t *def_g;
//                    def_g = lv_group_get_default();
//                    if(def_g == volume_set_g || def_g == hth_set_g \
//                        || def_g == lth_set_g || def_g == bright_set_g \
//                        || def_g == datetime_set_g || def_g == about_set_g)
//                        return KEY_STATE_DOWN;
//                    else
//                        return KEY_STATE_NEXT;
//                }
////                printf("上下: %d(上+ 下-)\r\n", data);
//                break;
//            case 1: 
//                if(diff_val < KEY_LEFT_ADC_TH)
//                {
////                    printf("左!!\r\n");
////                    return KEY_STATE_LEFT;
//                    return KEY_STATE_ESC;
//                }
//                else if(diff_val > KEY_RIGHT_ADC_TH)
//                {
////                    printf("右!!\r\n");
//                    lv_group_t *def_g;
//                    def_g = lv_group_get_default();
//                    if(def_g == hth_set_g || def_g == lth_set_g || def_g == datetime_set_g)
//                        return KEY_STATE_NEXT;
//                    else
//                        return KEY_STATE_ENTER;
//                }
////                printf("左右: %d(左- 右+)\r\n", data);
//                break;
//            case 2: 
//                if(diff_val > KEY_PRESS_ADC_TH)
//                {
////                    printf("按压!!\r\n");
//                    return KEY_STATE_ENTER;
//                }
//                break;
//            default: break;
//        }
//    }
    // return KEY_STATE_NULL;
}












//static lv_obj_t * btn1;
//static lv_obj_t * btn2;
//static lv_obj_t * info_label;

//static void btn_event_handler(lv_event_t * e)
//{
//    lv_event_code_t code = lv_event_get_code(e);
//    lv_obj_t * obj = lv_event_get_target(e);
//    
//    if(code == LV_EVENT_CLICKED)
//    {
//        if(obj == btn1)
//            lv_label_set_text(info_label, "Button 1 clicked!");
//        else if(obj == btn2)
//            lv_label_set_text(info_label, "Button 2 clicked!");
//    }
//    else if(code == LV_EVENT_FOCUSED)
//    {
//        // 焦点样式变化
//        lv_obj_set_style_bg_color(obj, lv_color_hex(0x4A90E2), 0);
//        lv_obj_set_style_border_width(obj, 3, 0);
//    }
//    else if(code == LV_EVENT_DEFOCUSED)
//    {
//        // 失去焦点恢复样式
//        lv_obj_set_style_bg_color(obj, lv_color_hex(0x2196F3), 0);
//        lv_obj_set_style_border_width(obj, 2, 0);
//    }
//}

//void create_test_ui(void)
//{
//    // 创建第一个按钮
//    btn1 = lv_btn_create(lv_scr_act());
//    lv_obj_set_size(btn1, 100, 50);
//    lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -60);
//    lv_obj_add_event_cb(btn1, btn_event_handler, LV_EVENT_ALL, NULL);
//    
//    // 确保按钮可点击和可聚焦
//    lv_obj_add_flag(btn1, LV_OBJ_FLAG_CLICKABLE);
//    lv_obj_add_flag(btn1, LV_OBJ_FLAG_CLICK_FOCUSABLE);
//    
//    lv_obj_t * label1 = lv_label_create(btn1);
//    lv_label_set_text(label1, "Button 1");
//    lv_obj_center(label1);
//    
//    // 创建第二个按钮
//    btn2 = lv_btn_create(lv_scr_act());
//    lv_obj_set_size(btn2, 100, 50);
//    lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 60);
//    lv_obj_add_event_cb(btn2, btn_event_handler, LV_EVENT_ALL, NULL);
//    
//    // 确保按钮可点击和可聚焦
//    lv_obj_add_flag(btn2, LV_OBJ_FLAG_CLICKABLE);
//    lv_obj_add_flag(btn2, LV_OBJ_FLAG_CLICK_FOCUSABLE);
//    
//    lv_obj_t * label2 = lv_label_create(btn2);
//    lv_label_set_text(label2, "Button 2");
//    lv_obj_center(label2);
//    
//    // 信息标签
//    info_label = lv_label_create(lv_scr_act());
//    lv_label_set_long_mode(info_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
//    lv_obj_set_width(info_label, 200);
//    lv_label_set_text(info_label, "等待按键输入...");
//    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -20);
//    
//    // 设置初始样式
//    lv_obj_set_style_bg_color(btn1, lv_color_hex(0x2196F3), 0);
//    lv_obj_set_style_bg_color(btn2, lv_color_hex(0x2196F3), 0);
//    
//    
//    static lv_group_t *g;	//将组绑定到输入设备
//    g = lv_group_get_default();
//    
//    // 关键：使用全局组变量g，而不是重新创建
//    // 将按钮添加到输入设备组
//    lv_group_add_obj(g, btn1);
//    lv_group_add_obj(g, btn2);
//    
//    // 设置第一个按钮为默认焦点
//    lv_group_focus_obj(btn1);
//    
//    // 添加焦点样式变化
//    lv_obj_set_style_bg_color(btn1, lv_color_hex(0x4A90E2), 0); // 初始焦点按钮高亮
//}







