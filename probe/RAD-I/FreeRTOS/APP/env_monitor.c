#include "env_monitor.h"
#include "geiger.h"
#include "ui.h"
#include <string.h>

Environment_Data_t env_data = {0};

extern lv_obj_t * label_geiger;
extern char ccnt[48];

bool update_sys_cfg = false;

static void env_label_set_if_changed(lv_obj_t * label, const char * text)
{
    if(label == NULL || text == NULL) {
        return;
    }

    if(strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

/******************************** 日期时间、传感器数据更新 ********************************/
void refresh_env_data(void)
{
    char buf[24];
    Environment_Data_t env_old = {0};
    static uint8_t tk = 0;
    
    tk++;
    if(!(tk % 5))
    {
        if(env_data.dt.year != env_old.dt.year || \
            env_data.dt.month != env_old.dt.month || \
            env_data.dt.day != env_old.dt.day || \
            env_data.dt.hour != env_old.dt.hour || \
            env_data.dt.minute != env_old.dt.minute)
        {
            sprintf(buf, "20%02d/%02d/%02d  %02d:%02d", env_data.dt.year, \
                                                        env_data.dt.month, \
                                                        env_data.dt.day,\
                                                        env_data.dt.hour, \
                                                        env_data.dt.minute);
            env_label_set_if_changed(ui_date, buf);
            memcpy(&env_old.dt, &env_data.dt, sizeof(DateTime_t));
        }
    }
    
    if(!(tk % 2))
    {
        sprintf(buf, "%.1f ℃", env_data.temperature);
        env_label_set_if_changed(ui_data_temperature, buf);

        sprintf(buf, "%3d %%", (uint16_t)env_data.humidity);
        env_label_set_if_changed(ui_data_humidity, buf);
    }

    if(!(tk % 3))
    {
        sprintf(buf, "%4d hPa", (uint16_t)(env_data.baro / 100.0f));
        env_label_set_if_changed(ui_data_baro, buf);

        sprintf(buf, "%4d ppm", env_data.CO2);
        env_label_set_if_changed(ui_data_CO2, buf);

        sprintf(buf, "%3d ug/m³", env_data.PM2_5);
        env_label_set_if_changed(ui_data_PM2_5, buf);
        if(env_data.PM2_5 >= 100)
            lv_obj_set_style_text_font(ui_data_PM2_5, &ui_font_hansanbold24, LV_PART_MAIN | LV_STATE_DEFAULT);
        else
            lv_obj_set_style_text_font(ui_data_PM2_5, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if(data_var.real_rate < 1000.0f)
    {
        sprintf(buf, "%3.2f", data_var.real_rate);
        env_label_set_if_changed(ui_data_unit, "μSv/h");
    }
    else
    {
        sprintf(buf, "%.2f", (float)RATE_LIMIT / 1000.0f);
        env_label_set_if_changed(ui_data_unit, "mSv/h");
    }
    env_label_set_if_changed(ui_data_doserate, buf);
    
    // lv_label_set_text(label_geiger, ccnt);
    


    if(update_sys_cfg)
    {
        update_sys_cfg = false;

        // 先同步语言设置
        language_set_current((Language_t)sys_cfg.language);
        
        if(sys_cfg.alarm_light)
        {
            _ui_flag_modify(ui_led_off, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
            _ui_flag_modify(ui_led_on, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
            lv_label_set_text(ui_light_alarm_state, language_get_string(LANG_ALARM_STATE_ON));
        }
        else
        {
            _ui_flag_modify(ui_led_on, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
            _ui_flag_modify(ui_led_off, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
            lv_label_set_text(ui_light_alarm_state, language_get_string(LANG_ALARM_STATE_OFF));
        }

        /* 声报警主界面图标（与光报警一致，随 sys_cfg 同步） */
        if(sys_cfg.alarm_sound && sys_cfg.alarm_volume)
        {
            _ui_flag_modify(ui_alarm_on, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
            _ui_flag_modify(ui_alarm_off, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        }
        else
        {
            _ui_flag_modify(ui_alarm_on, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
            _ui_flag_modify(ui_alarm_off, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        }

        // 亮度设置
        lv_label_set_text_fmt(ui_sys_bright, "%d %%", (int)sys_cfg.bright_sz);
        lv_label_set_text_fmt(ui_set_bright, "%d %%", (int)sys_cfg.bright_sz);
        lv_slider_set_value(ui_Slider_bright, sys_cfg.bright_sz, LV_ANIM_OFF);

        // 报警音量设置
        if(sys_cfg.alarm_sound && sys_cfg.alarm_volume)
        {
            lv_label_set_text_fmt(ui_alarm_volume, "%d %%", (int)sys_cfg.alarm_volume);
            lv_label_set_text_fmt(ui_set_alarm_volume, "%d %%", (int)sys_cfg.alarm_volume);
            lv_slider_set_value(ui_Slider_volume, sys_cfg.alarm_volume, LV_ANIM_OFF);
        }
        else
        {
            lv_label_set_text_fmt(ui_alarm_volume, "0 %%");
            lv_label_set_text_fmt(ui_set_alarm_volume, "0 %%");
            lv_slider_set_value(ui_Slider_volume, 0, LV_ANIM_OFF);
        }


        // 读取剂量率上、下阈值，更新显示和滚筒位置
        // 处理高位阈值
        uint8_t i = 0;
        char *p;
        char temp[16] = {0}, gstr[2][8] = {0};
        int integer_part = 0, decimal_part = 0;

        if(sys_cfg.th_rh_rate >= 1000.0f)
            snprintf(temp, 16, "%.2f mSv/h", (float)((uint32_t)(sys_cfg.th_rh_rate / 10.0f)) / 100.0f);
        else
            snprintf(temp, 16, "%.2f μSv/h", sys_cfg.th_rh_rate);
        lv_label_set_text(ui_high_threshold, temp);

        p = strtok((char*)temp, " ");
        while(p)
        {
            strcpy(gstr[i], p);
            i++;
            p = strtok(NULL, " ");
        }

        // 解析输入字符串
        if(sscanf(gstr[0], "%d.%d", &integer_part, &decimal_part) == 2)
            snprintf(gstr[0], 7, "%03d.%02d", integer_part, decimal_part);
        // printf("hth val: %s %s\r\n", gstr[0], gstr[1]);
        lv_roller_set_selected(ui_HthLeft3, 9 - (gstr[0][0] - '0'), LV_ANIM_OFF);
        lv_roller_set_selected(ui_HthLeft2, 9 - (gstr[0][1] - '0'), LV_ANIM_OFF);
        lv_roller_set_selected(ui_HthLeft1, 9 - (gstr[0][2] - '0'), LV_ANIM_OFF);
        lv_roller_set_selected(ui_HthRight1, 9 - (gstr[0][4] - '0'), LV_ANIM_OFF);
        lv_roller_set_selected(ui_HthRight2, 9 - (gstr[0][5] - '0'), LV_ANIM_OFF);

        if(!strcasecmp(gstr[1], "mSv/h"))
            lv_roller_set_selected(ui_HthUnit, 0, LV_ANIM_OFF);
        else
            lv_roller_set_selected(ui_HthUnit, 1, LV_ANIM_OFF);


        if(sys_cfg.th_rl_rate >= 1000.0f)
            snprintf(temp, 16, "%.2f mSv/h", (float)((uint32_t)(sys_cfg.th_rl_rate / 10.0f)) / 100.0f);
        else
            snprintf(temp, 16, "%.2f μSv/h", sys_cfg.th_rl_rate);
        lv_label_set_text(ui_low_threshold, temp);
        
        i = 0;
        p = strtok((char*)temp, " ");
        while(p)
        {
            strcpy(gstr[i], p);
            i++;
            p = strtok(NULL, " ");
        }

        // 解析输入字符串
        if(sscanf(gstr[0], "%d.%d", &integer_part, &decimal_part) == 2)
            snprintf(gstr[0], 7, "%03d.%02d", integer_part, decimal_part);
        // printf("lth val: %s %s\r\n", gstr[0], gstr[1]);
        lv_roller_set_selected(ui_LthLeft3, 9 - (gstr[0][0] - '0'), LV_ANIM_OFF);
        lv_roller_set_selected(ui_LthLeft2, 9 - (gstr[0][1] - '0'), LV_ANIM_OFF);
        lv_roller_set_selected(ui_LthLeft1, 9 - (gstr[0][2] - '0'), LV_ANIM_OFF);
        lv_roller_set_selected(ui_LthRight1, 9 - (gstr[0][4] - '0'), LV_ANIM_OFF);
        lv_roller_set_selected(ui_LthRight2, 9 - (gstr[0][5] - '0'), LV_ANIM_OFF);

        if(!strcasecmp(gstr[1], "mSv/h"))
            lv_roller_set_selected(ui_LthUnit, 0, LV_ANIM_OFF);
        else
            lv_roller_set_selected(ui_LthUnit, 1, LV_ANIM_OFF);
    }
}

// 定时器回调函数
static void refresh_timer_cb(lv_timer_t * timer)
{
    refresh_env_data();
}

void create_refresh_timer(void)
{
    // 创建定时器，每1000ms（1秒）执行一次
    lv_timer_t * timer = lv_timer_create(refresh_timer_cb, 1000, NULL);
    
    // 可选：设置定时器只执行一次
    // lv_timer_set_repeat_count(timer, 1);
}
/******************************** 日期时间、传感器数据更新 ********************************/


/************************************ 日期时间滚轮调整 ************************************/
// 当前选择的年月日
uint8_t current_year = 0;
uint8_t current_month = 0;
uint8_t current_day = 0;

/**
 * @brief 更新日的可选范围
 * @details 根据当前年月计算最大天数，并更新日的roller选项
 */
void update_day_roller_options(void)
{
    char buf[3] = {0};
    uint8_t max_days = 31;
    char day_options[100] = {0};
    
    lv_roller_get_selected_str(ui_day,buf, sizeof(buf));
    current_day = atoi(buf);

    if(current_month == 2)
    {
        // 闰年判断：能被4整除但不能被100整除，或者能被400整除
        if((((current_year % 4) == 0) && ((current_year % 100) != 0)) || ((current_year % 400) == 0))
            max_days = 29;
        else
            max_days = 28;
    }
    else if(current_month == 4 || current_month == 6 || current_month == 9 || current_month == 11)
        max_days = 30;
    else
        max_days = 31;
    
    // 构建日的选项字符串
    char temp[3] = {0};
    for(uint8_t i = max_days; i >= 1; i--) {
        snprintf(temp, sizeof(temp), "%02d", i);
        strcat(day_options, temp);
        if(i > 1) {
            strcat(day_options, "\n");
        }
    }
    
    // 保存当前选择的日（在更新选项前）
    uint8_t prev_day = current_day;
    
    // 更新日的roller选项
    lv_roller_set_options(ui_day, day_options, LV_ROLLER_MODE_INFINITE);
    
    // 如果之前的日大于最大天数，调整为最大天数
    if(prev_day > max_days)
    {
        current_day = max_days;
        lv_roller_set_selected(ui_day, 0, LV_ANIM_OFF);
    }
    else
    {
        // 保持之前选择的日（调整索引，因为选项是倒序的）
        uint8_t day_index = max_days - current_day;
        lv_roller_set_selected(ui_day, day_index, LV_ANIM_OFF);
    }
}


/**
 * @brief 日期时间设置初始化
 * @details 在创建完所有roller组件后调用，设置初始值和事件监听
 */
void datetime_setup_init(void)
{
    // 获取初始值
    char buf[3];
    
    lv_roller_get_selected_str(ui_Year, buf, sizeof(buf));
    current_year = atoi(buf);
    
    lv_roller_get_selected_str(ui_month, buf, sizeof(buf));
    current_month = atoi(buf);
    
    lv_roller_get_selected_str(ui_day, buf, sizeof(buf));
    current_day = atoi(buf);
}
/************************************ 日期时间滚轮调整 ************************************/



















