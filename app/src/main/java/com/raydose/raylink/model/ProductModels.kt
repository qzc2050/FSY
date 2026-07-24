package com.raydose.raylink.model

/**
 * Raylink 产品型号体系。
 *
 * - 主机：RKM10 / RKM13
 * - 内机：RK100P / RK100D / RK100N（固件默认 RK100P；可「添加探头」后 TCP 采数）
 * - 其它本公司联网设备（如 RK-N）：仅下拉「在线设备」展示，不入库为探头
 *
 * 添加探头：
 * - **网络组播**：广播型号为内机三型时才可添加
 * - **CAN / LoRa**：无型号字段，默认 RK100P，可添加
 */
object ProductModels {
    const val HOST_RKM10 = "RKM10"
    const val HOST_RKM13 = "RKM13"

    /** 内机 · 全功能有屏（固件/串口默认） */
    const val PROBE_RK100P = "RK100P"

    /** 内机 · 无屏有空气成分 */
    const val PROBE_RK100D = "RK100D"

    /** 内机 · 仅网络链路 */
    const val PROBE_RK100N = "RK100N"

    /** 物联网盒子 · 仅网络，不下探头管理 */
    const val BOX_RK_N = "RK-N"

    /** 旧固件内机型号（过渡期仍可添加） */
    const val LEGACY_PROBE_FSY_I = "FSY-I"

    /** 关于本机等展示用型号（不加尺寸后缀） */
    const val HOST_RKM10_LABEL = HOST_RKM10
    const val HOST_RKM13_LABEL = HOST_RKM13

    /** 可「添加探头」入库并走 TCP/遥测的内机型号 */
    private val manageableProbeModels: Set<String> = setOf(
        PROBE_RK100P,
        PROBE_RK100D,
        PROBE_RK100N,
        LEGACY_PROBE_FSY_I,
    )

    fun isManageableProbeModel(model: String): Boolean {
        val key = model.trim()
        if (key.isEmpty()) return false
        return manageableProbeModels.any { it.equals(key, ignoreCase = true) }
    }

    /** RK100N 无空气成分传感器，不做环境传感器离线提示/报警。 */
    fun hasEnvAirSensors(model: String): Boolean {
        val key = model.trim()
        if (key.isEmpty()) return true /* 未知型号按有传感器处理，避免漏报 */
        if (key.equals(PROBE_RK100N, ignoreCase = true)) return false
        return true
    }

    @Deprecated("使用 isManageableProbeModel", ReplaceWith("isManageableProbeModel(model)"))
    fun isDiscoverableProbeModel(model: String): Boolean = isManageableProbeModel(model)
}
