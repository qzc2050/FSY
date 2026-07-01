package com.raydose.netshield.data

import com.raydose.netshield.model.NeijiEnvSensorKind
import com.raydose.netshield.model.neijiEnvSensorsOffline

/**
 * 内机 0x23 每秒上报传感器离线 bit；下拉日志 **仅在状态变化** 时写入。
 *
 * - TCP 已连接后，首次 0x23 若全部正常 →「传感器正常」
 * - 某传感器 正常→离线 → 输出一次「XX传感器离线」
 * - 全部恢复 离线→正常 →「传感器正常」
 */
class ProbeSensorOfflineLogAggregator(
    private val onSensorsNormal: (timestampMillis: Long, probeName: String) -> Unit,
    private val onSensorsOffline: (timestampMillis: Long, probeName: String, sensors: Set<NeijiEnvSensorKind>) -> Unit,
) {
    /** null = 尚未收到本连接周期内的 0x23，不做边沿比较 */
    private val lastOffline = mutableMapOf<String, Set<NeijiEnvSensorKind>?>()

    fun onProbeConnected(probeId: String) {
        lastOffline[probeId] = null
    }

    fun onProbeDisconnected(probeId: String) {
        lastOffline.remove(probeId)
    }

    /** 每个含 alarm_bit 的 0x23 实时包调用一次 */
    fun onTelemetrySample(
        probeId: String,
        probeName: String,
        alarmBit: Long?,
        nowMillis: Long,
    ) {
        if (alarmBit == null) return

        val offline = neijiEnvSensorsOffline(alarmBit)
        when (val prev = lastOffline[probeId]) {
            null -> {
                if (offline.isEmpty()) {
                    onSensorsNormal(nowMillis, probeName)
                } else {
                    onSensorsOffline(nowMillis, probeName, offline)
                }
            }
            else -> {
                val newlyOffline = offline - prev
                if (newlyOffline.isNotEmpty()) {
                    onSensorsOffline(nowMillis, probeName, newlyOffline)
                }
                if (prev.isNotEmpty() && offline.isEmpty()) {
                    onSensorsNormal(nowMillis, probeName)
                }
            }
        }
        lastOffline[probeId] = offline
    }
}
