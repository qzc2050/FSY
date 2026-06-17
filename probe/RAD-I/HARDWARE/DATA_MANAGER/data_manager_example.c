/**
 * @file     data_manager_example.c
 * @brief    数据管理模块使用示例
 * @author   ALIENTEK
 * @version  V1.0
 * @date     2026/03/25
 * 
 * @note     演示如何使用数据管理模块保存和读取辐射剂量数据
 */

#include "hist_record_app.h"
#include "stdio.h"
#include "string.h"

/**
 * @brief  读取全部数据示例
 * @note   演示如何读取所有数据（从最新到最旧）
 */
void Data_Manager_Example_ReadAll(void)
{
    printf("\r\n========== Read All Data Example ==========\r\n");
    
    printf("5-min data:\r\n");
    HistRecord_ReadAll();
    
    printf("========================================\r\n\r\n");
}

//============================== 实际应用示例 ==============================//
/**
 * @brief  实际应用中保存 5 分钟平均剂量数据
 * @param  year: 年 (2000-2099)
 * @param  month: 月 (1-12)
 * @param  day: 日 (1-31)
 * @param  hour: 时 (0-23)
 * @param  minute: 分 (0-59)
 * @param  dose_uSv: 剂量值（单位：微希沃特 uSv）
 * @retval 执行结果：0-成功，其他 - 失败
 */
int Save_Dose_Data_5Min(uint8_t year, uint8_t month, uint8_t day,
                        uint8_t hour, uint8_t minute,
                        uint32_t dose_uSv)
{
    char datetime[20];
    
    // 格式化日期时间：YYYYMMDD,HHMMSS (秒设为 0)
    sprintf(datetime, "20%02u%02u%02u,%02u%02u00", 
            year, month, day, hour, minute);
    
    // 直接传入剂量值（微西弗）
    return HistRecord_Write(datetime, dose_uSv);
}
