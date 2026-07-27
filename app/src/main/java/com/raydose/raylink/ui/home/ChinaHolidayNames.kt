package com.raydose.raylink.ui.home

import android.content.res.Resources
import android.icu.util.ChineseCalendar
import androidx.annotation.StringRes
import com.raydose.raylink.R
import java.util.Calendar
import java.util.Locale

/**
 * 中国大陆常见节假日名称。
 * - 清明：按节气（太阳黄经 15°）
 * - 除夕：农历腊月最后一天（次日为正月初一）
 * - 其余农历/公历固定节日按历法当日匹配
 */
internal object ChinaHolidayNames {
    fun nameFor(cal: Calendar, resources: Resources): String? {
        if (isLunarNewYearEve(cal)) return resources.getString(R.string.holiday_new_year_eve)
        if (SolarTermCalculator.isQingmingDay(cal)) return resources.getString(R.string.holiday_qingming)
        lunarName(cal)?.let { return resources.getString(it) }
        return solarName(cal)?.let { resources.getString(it) }
    }

    /** 次日为正月初一 ⇒ 当日为除夕（含腊月小月廿九） */
    private fun isLunarNewYearEve(cal: Calendar): Boolean {
        return try {
            val tomorrow = (cal.clone() as Calendar).apply {
                add(Calendar.DAY_OF_MONTH, 1)
            }
            val cc = ChineseCalendar(Locale.CHINA)
            cc.timeInMillis = tomorrow.timeInMillis
            cc.get(ChineseCalendar.MONTH) == ChineseCalendar.JANUARY &&
                cc.get(ChineseCalendar.DAY_OF_MONTH) == 1
        } catch (_: Exception) {
            false
        }
    }

    private fun solarName(cal: Calendar): Int? {
        val key = String.format(
            Locale.US,
            "%02d-%02d",
            cal.get(Calendar.MONTH) + 1,
            cal.get(Calendar.DAY_OF_MONTH),
        )
        return SOLAR[key]
    }

    private fun lunarName(cal: Calendar): Int? {
        return try {
            val cc = ChineseCalendar(Locale.CHINA)
            cc.timeInMillis = cal.timeInMillis
            val month = cc.get(ChineseCalendar.MONTH) + 1
            val day = cc.get(ChineseCalendar.DAY_OF_MONTH)
            LUNAR["$month-$day"]
        } catch (_: Exception) {
            null
        }
    }

    /** 公历固定节日（不含清明） */
    private val SOLAR = mapOf(
        "01-01" to R.string.holiday_new_year,
        "02-14" to R.string.holiday_valentine,
        "03-08" to R.string.holiday_womens_day,
        "03-12" to R.string.holiday_arbor_day,
        "04-01" to R.string.holiday_april_fools,
        "05-01" to R.string.holiday_labor_day,
        "05-04" to R.string.holiday_youth_day,
        "06-01" to R.string.holiday_children_day,
        "07-01" to R.string.holiday_party_day,
        "08-01" to R.string.holiday_army_day,
        "09-10" to R.string.holiday_teachers_day,
        "10-01" to R.string.holiday_national_day,
        "12-24" to R.string.holiday_christmas_eve,
        "12-25" to R.string.holiday_christmas,
    )

    /** 农历节日（不含除夕） */
    private val LUNAR = mapOf(
        "1-1" to R.string.holiday_spring_festival,
        "1-15" to R.string.holiday_lantern,
        "2-2" to R.string.holiday_dragon_head,
        "5-5" to R.string.holiday_dragon_boat,
        "7-7" to R.string.holiday_qixi,
        "7-15" to R.string.holiday_ghost,
        "8-15" to R.string.holiday_mid_autumn,
        "9-9" to R.string.holiday_double_ninth,
        "12-8" to R.string.holiday_laba,
        "12-23" to R.string.holiday_north_minor_year,
        "12-24" to R.string.holiday_south_minor_year,
    )
}
