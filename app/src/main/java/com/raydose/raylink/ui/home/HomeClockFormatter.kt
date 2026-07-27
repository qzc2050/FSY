package com.raydose.raylink.ui.home

import android.content.res.Resources
import android.icu.util.ChineseCalendar
import com.raydose.raylink.R
import com.raydose.raylink.model.TimeSettings
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale

/** 主页第二行日期/时间展示（受设置 · 时间 中的显示开关控制，不写系统时钟）。 */
object HomeClockFormatter {
    private val time24Fmt = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
    private val time12Fmt = SimpleDateFormat("a hh:mm:ss", Locale.US)

    fun format(now: Date, prefs: TimeSettings, resources: Resources): Pair<String, String> {
        val cal = Calendar.getInstance().apply { time = now }
        val dateLine = buildDateLine(cal, now, prefs, resources)
        val timeLine = if (prefs.use24Hour) {
            time24Fmt.format(now)
        } else {
            time12Fmt.format(now)
        }
        return dateLine to timeLine
    }

    /** 待机页：第二行日期+农历，第三行时间（不含节假日单独占行）。 */
    fun formatStandbyLines(
        now: Date,
        prefs: TimeSettings,
        resources: Resources,
        fallbackDateText: String,
        fallbackTimeText: String,
    ): StandbyClockLines {
        val cal = Calendar.getInstance().apply { time = now }
        val dateParts = mutableListOf<String>()
        if (prefs.showGregorian) {
            dateParts += gregorianDateFmt(resources).format(now)
        }
        if (prefs.showLunar) {
            formatLunar(cal, resources)?.let { dateParts += it }
        }
        val dateLine = dateParts.joinToString("  ").ifBlank { fallbackDateText }
        val timeLine = if (prefs.use24Hour) {
            time24Fmt.format(now)
        } else {
            time12Fmt.format(now)
        }.ifBlank { fallbackTimeText }
        return StandbyClockLines(dateLine = dateLine, timeLine = timeLine)
    }

    data class StandbyClockLines(
        val dateLine: String,
        val timeLine: String,
    )

    private fun buildDateLine(cal: Calendar, now: Date, prefs: TimeSettings, resources: Resources): String {
        val parts = mutableListOf<String>()
        if (prefs.showGregorian) {
            parts += gregorianDateFmt(resources).format(now)
        }
        if (prefs.showLunar) {
            formatLunar(cal, resources)?.let { parts += it }
        }
        if (prefs.showHoliday) {
            ChinaHolidayNames.nameFor(cal, resources)?.let { parts += it }
        }
        return parts.joinToString("  ")
    }

    private fun gregorianDateFmt(resources: Resources): SimpleDateFormat {
        val locale = resources.configuration.locales[0] ?: Locale.getDefault()
        return SimpleDateFormat(resources.getString(R.string.date_gregorian_format), locale)
    }

    private fun formatLunar(cal: Calendar, resources: Resources): String? {
        return try {
            val cc = ChineseCalendar(Locale.CHINA)
            cc.timeInMillis = cal.timeInMillis
            val leap = cc.get(ChineseCalendar.IS_LEAP_MONTH) == ChineseCalendar.IS_LEAP_MONTH
            val month = cc.get(ChineseCalendar.MONTH) + 1
            val day = cc.get(ChineseCalendar.DAY_OF_MONTH)
            val locale = resources.configuration.locales[0] ?: Locale.getDefault()
            if (locale.language.startsWith("zh")) {
                val monthName = lunarMonthName(month, leap)
                val dayName = lunarDayName(day)
                resources.getString(R.string.date_lunar_prefix, "$monthName$dayName")
            } else {
                /* 英文不用「六月十四」混排，改为 Lunar 6/14 */
                if (leap) {
                    resources.getString(R.string.date_lunar_leap_md, month, day)
                } else {
                    resources.getString(R.string.date_lunar_md, month, day)
                }
            }
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
fun formatHomeClock(now: Date = Date(), resources: Resources): Pair<String, String> =
    HomeClockFormatter.format(now, TimeSettings(), resources)
