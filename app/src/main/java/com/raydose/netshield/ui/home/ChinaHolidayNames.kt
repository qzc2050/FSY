package com.raydose.netshield.ui.home

import android.icu.util.ChineseCalendar
import java.util.Calendar
import java.util.Locale

/**
 * 中国大陆常见节假日名称。
 * - 清明：按节气（太阳黄经 15°）
 * - 除夕：农历腊月最后一天（次日为正月初一）
 * - 其余农历/公历固定节日按历法当日匹配
 */
internal object ChinaHolidayNames {
    fun nameFor(cal: Calendar): String? {
        if (isLunarNewYearEve(cal)) return "除夕"
        if (SolarTermCalculator.isQingmingDay(cal)) return "清明节"
        lunarName(cal)?.let { return it }
        return solarName(cal)
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

    private fun solarName(cal: Calendar): String? {
        val key = String.format(
            Locale.US,
            "%02d-%02d",
            cal.get(Calendar.MONTH) + 1,
            cal.get(Calendar.DAY_OF_MONTH),
        )
        return SOLAR[key]
    }

    private fun lunarName(cal: Calendar): String? {
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
        "01-01" to "元旦",
        "02-14" to "情人节",
        "03-08" to "妇女节",
        "03-12" to "植树节",
        "04-01" to "愚人节",
        "05-01" to "劳动节",
        "05-04" to "青年节",
        "06-01" to "儿童节",
        "07-01" to "建党节",
        "08-01" to "建军节",
        "09-10" to "教师节",
        "10-01" to "国庆节",
        "12-24" to "平安夜",
        "12-25" to "圣诞节",
    )

    /** 农历节日（不含除夕） */
    private val LUNAR = mapOf(
        "1-1" to "春节",
        "1-15" to "元宵节",
        "2-2" to "龙抬头",
        "5-5" to "端午节",
        "7-7" to "七夕节",
        "7-15" to "中元节",
        "8-15" to "中秋节",
        "9-9" to "重阳节",
        "12-8" to "腊八节",
        "12-23" to "北方小年",
        "12-24" to "南方小年",
    )
}
