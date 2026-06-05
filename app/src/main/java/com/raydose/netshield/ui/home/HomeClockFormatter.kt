package com.raydose.netshield.ui.home

import android.icu.util.ChineseCalendar
import com.raydose.netshield.model.TimeSettings
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale

/** 主页第二行日期/时间展示（受设置 · 时间 中的显示开关控制，不写系统时钟）。 */
object HomeClockFormatter {
    private val gregorianDateFmt = SimpleDateFormat("yyyy年MM月dd日", Locale.CHINA)
    private val time24Fmt = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
    private val time12Fmt = SimpleDateFormat("a hh:mm:ss", Locale.US)

    fun format(now: Date, prefs: TimeSettings): Pair<String, String> {
        val cal = Calendar.getInstance().apply { time = now }
        val dateLine = buildDateLine(cal, now, prefs)
        val timeLine = if (prefs.use24Hour) {
            time24Fmt.format(now)
        } else {
            time12Fmt.format(now)
        }
        return dateLine to timeLine
    }

    private fun buildDateLine(cal: Calendar, now: Date, prefs: TimeSettings): String {
        val parts = mutableListOf<String>()
        if (prefs.showGregorian) {
            parts += gregorianDateFmt.format(now)
        }
        if (prefs.showLunar) {
            formatLunar(cal)?.let { parts += it }
        }
        if (prefs.showHoliday) {
            ChinaHolidayNames.nameFor(cal)?.let { parts += it }
        }
        return parts.joinToString("  ")
    }

    private fun formatLunar(cal: Calendar): String? {
        return try {
            val cc = ChineseCalendar(Locale.CHINA)
            cc.timeInMillis = cal.timeInMillis
            val leap = cc.get(ChineseCalendar.IS_LEAP_MONTH) == ChineseCalendar.IS_LEAP_MONTH
            val month = cc.get(ChineseCalendar.MONTH) + 1
            val day = cc.get(ChineseCalendar.DAY_OF_MONTH)
            val monthName = lunarMonthName(month, leap)
            val dayName = lunarDayName(day)
            "农历$monthName$dayName"
        } catch (_: Exception) {
            null
        }
    }

    private fun lunarMonthName(month: Int, leap: Boolean): String {
        val names = arrayOf(
            "正", "二", "三", "四", "五", "六",
            "七", "八", "九", "十", "冬", "腊",
        )
        val base = names.getOrElse(month - 1) { month.toString() }
        return (if (leap) "闰" else "") + base + "月"
    }

    private fun lunarDayName(day: Int): String = when (day) {
        in 1..9 -> "初${chineseDigit(day)}"
        10 -> "初十"
        in 11..19 -> "十${chineseDigit(day - 10)}"
        20 -> "二十"
        in 21..29 -> "廿${chineseDigit(day - 20)}"
        30 -> "三十"
        else -> "${day}日"
    }

    private fun chineseDigit(n: Int): String = when (n) {
        0 -> ""
        1 -> "一"
        2 -> "二"
        3 -> "三"
        4 -> "四"
        5 -> "五"
        6 -> "六"
        7 -> "七"
        8 -> "八"
        9 -> "九"
        else -> n.toString()
    }

}

/** @deprecated 预览用；运行时使用 [HomeClockFormatter.format] */
fun formatHomeClock(now: Date = Date()): Pair<String, String> =
    HomeClockFormatter.format(now, TimeSettings())
