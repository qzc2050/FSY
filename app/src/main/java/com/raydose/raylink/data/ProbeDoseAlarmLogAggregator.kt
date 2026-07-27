package com.raydose.raylink.data

/**
 * 内机 0x23 每秒上报辐射上/下阈值报警 bit；下拉日志 **仅在状态变化** 时写入。
 *
 * - 某阈值 正常→报警 →「辐射上限/下限报警」
 * - 全部报警解除 →「辐射报警解除」
 */
enum class DoseAlarmLimit { Upper, Lower }

class ProbeDoseAlarmLogAggregator(
    private val onAlarmStarted: (timestampMillis: Long, probeName: String, limits: List<DoseAlarmLimit>) -> Unit,
    private val onAlarmCleared: (timestampMillis: Long, probeName: String) -> Unit,
) {
    /** null = 尚未收到本连接周期内的 0x23 */
    private val lastActive = mutableMapOf<String, Set<DoseAlarmLimit>?>()

    fun onProbeConnected(probeId: String) {
        lastActive[probeId] = null
    }

    fun onProbeDisconnected(probeId: String) {
        lastActive.remove(probeId)
    }

    /** 每个含 alarm_bit 的 0x23 实时包调用一次 */
    fun onTelemetrySample(
        probeId: String,
        probeName: String,
        alarmBit: Long?,
        nowMillis: Long,
    ) {
        if (alarmBit == null) return

        val active = activeDoseAlarmLimits(alarmBit)
        when (val prev = lastActive[probeId]) {
            null -> {
                if (active.isNotEmpty()) {
                    onAlarmStarted(nowMillis, probeName, active.toList())
                }
            }
            else -> {
                val newly = active - prev
                if (newly.isNotEmpty()) {
                    onAlarmStarted(nowMillis, probeName, newly.toList())
                }
                if (prev.isNotEmpty() && active.isEmpty()) {
                    onAlarmCleared(nowMillis, probeName)
                }
            }
        }
        lastActive[probeId] = active
    }

    private fun activeDoseAlarmLimits(alarmBit: Long): Set<DoseAlarmLimit> = buildSet {
        if ((alarmBit and 1L) != 0L) add(DoseAlarmLimit.Upper)
        if ((alarmBit and 2L) != 0L) add(DoseAlarmLimit.Lower)
    }
}
