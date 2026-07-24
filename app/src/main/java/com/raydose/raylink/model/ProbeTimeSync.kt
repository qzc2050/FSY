package com.raydose.raylink.model

import java.util.Calendar
import java.util.concurrent.TimeUnit

/** 主机时间写入探头前：不早于该日期（本地时区）。 */
const val PROBE_TIME_SYNC_MIN_YEAR = 2026
const val PROBE_TIME_SYNC_MIN_DAY = 8

/** 第一版：同一探头两次自动同步最小间隔 */
val PROBE_AUTO_SYNC_INTERVAL_MS = TimeUnit.HOURS.toMillis(6)

/** 收到 5min 帧时，|设备时间−主机| 超过该阈值则立即写 reg94 */
val PROBE_TIME_SYNC_ON_5MIN_SKEW_MS = TimeUnit.MINUTES.toMillis(5)

/** 短暂断线重连后暂不再次自动同步 */
val PROBE_AUTO_SYNC_RECONNECT_DEBOUNCE_MS = TimeUnit.MINUTES.toMillis(15)

/** App 启动后等待连接稳定再尝试自动同步 */
const val PROBE_AUTO_SYNC_STARTUP_DELAY_MS = 60_000L

/** 允许写入的最大未来偏移（防止误设远期时间） */
val PROBE_TIME_SYNC_MAX_FUTURE_SKEW_MS = TimeUnit.DAYS.toMillis(1)

fun isHostTimeValidForProbeSync(nowMillis: Long = System.currentTimeMillis()): Boolean {
    val cal = Calendar.getInstance().apply { timeInMillis = nowMillis }
    val min = Calendar.getInstance().apply {
        set(Calendar.YEAR, PROBE_TIME_SYNC_MIN_YEAR)
        set(Calendar.MONTH, Calendar.JULY)
        set(Calendar.DAY_OF_MONTH, PROBE_TIME_SYNC_MIN_DAY)
        set(Calendar.HOUR_OF_DAY, 0)
        set(Calendar.MINUTE, 0)
        set(Calendar.SECOND, 0)
        set(Calendar.MILLISECOND, 0)
    }
    if (nowMillis < min.timeInMillis) return false
    val upperBound = System.currentTimeMillis() + PROBE_TIME_SYNC_MAX_FUTURE_SKEW_MS
    if (nowMillis > upperBound) return false
    return true
}

fun hostTimeInvalidForProbeSyncHint(): String =
    "主机时间无效（需不早于 2026-07-08），请先校正系统时间"

fun shouldAutoSyncProbeTime(
    lastAutoSyncMillis: Long,
    lastOfflineMillis: Long,
    nowMillis: Long = System.currentTimeMillis(),
    intervalMs: Long = PROBE_AUTO_SYNC_INTERVAL_MS,
    reconnectDebounceMs: Long = PROBE_AUTO_SYNC_RECONNECT_DEBOUNCE_MS,
): Boolean {
    if (lastAutoSyncMillis <= 0L) return true
    if (nowMillis - lastAutoSyncMillis < intervalMs) return false
    if (lastOfflineMillis > 0L && nowMillis - lastOfflineMillis < reconnectDebounceMs) {
        return false
    }
    return true
}
