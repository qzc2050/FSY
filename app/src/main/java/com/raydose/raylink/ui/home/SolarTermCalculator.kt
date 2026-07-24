package com.raydose.raylink.ui.home

import java.util.Calendar
import kotlin.math.floor
import kotlin.math.sin

/**
 * 二十四节气中的「清明」：太阳黄经达 15° 的公历日。
 * 采用简化天文算法，适用于工控屏节日展示（约 ±1 日精度）。
 */
internal object SolarTermCalculator {
    private const val RAD = Math.PI / 180.0
    private const val QINGMING_LONGITUDE = 15.0

    private val qingmingDayCache = mutableMapOf<Int, Int>()

    fun isQingmingDay(cal: Calendar): Boolean {
        val year = cal.get(Calendar.YEAR)
        if (cal.get(Calendar.MONTH) != Calendar.APRIL) return false
        return cal.get(Calendar.DAY_OF_MONTH) == qingmingDayOfApril(year)
    }

    /** 某年清明所在公历 4 月的日期（3～7 日之间搜索） */
    fun qingmingDayOfApril(year: Int): Int =
        qingmingDayCache.getOrPut(year) { computeQingmingDay(year) }

    private fun computeQingmingDay(year: Int): Int {
        var prevLon = sunLongitude(julianDayNoon(year, 4, 3))
        for (day in 4..7) {
            val lon = sunLongitude(julianDayNoon(year, 4, day))
            if (crossesLongitude(prevLon, lon, QINGMING_LONGITUDE)) return day
            prevLon = lon
        }
        return 5
    }

    private fun crossesLongitude(prev: Double, curr: Double, target: Double): Boolean {
        if (prev <= curr) return prev < target && curr >= target
        return prev < target || curr >= target
    }

    private fun julianDayNoon(year: Int, month: Int, day: Int): Double {
        var y = year
        var m = month
        if (m <= 2) {
            y--
            m += 12
        }
        val a = y / 100
        val b = 2 - a + a / 4
        return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + day + b - 1524.5 + 0.5
    }

    /** 太阳视黄经 [0, 360) */
    fun sunLongitude(julianDay: Double): Double {
        val t = (julianDay - 2451545.0) / 36525.0
        var l0 = 280.46646 + 36000.76983 * t + 0.0003032 * t * t
        val m = 357.52911 + 35999.05029 * t - 0.0001537 * t * t
        val mr = m * RAD
        val c = (1.914602 - 0.004817 * t - 0.000014 * t * t) * sin(mr) +
            (0.019993 - 0.000101 * t) * sin(2 * mr) +
            0.000289 * sin(3 * mr)
        l0 += c
        var lon = l0 % 360.0
        if (lon < 0) lon += 360.0
        return lon
    }
}
