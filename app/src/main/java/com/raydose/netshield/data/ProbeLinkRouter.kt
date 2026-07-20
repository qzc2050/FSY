package com.raydose.netshield.data

import com.raydose.netshield.model.ProbeCommandLink
import java.util.concurrent.ConcurrentHashMap

/**
 * 按探头记录**实时 0x23** 的来源（网口/串口），用于下发选路与切换日志。
 * 五分钟等其它帧不写入此处。
 */
class ProbeLinkRouter {
    private val lastRoute = ConcurrentHashMap<String, ProbeCommandLink>()
    private val lastRx23Ms = ConcurrentHashMap<String, Long>()
    /** 网口组播发现刷新，用于 0x23 超时判定 */
    private val lastMulticastMs = ConcurrentHashMap<String, Long>()

    /**
     * 记录 0x23 来源。
     * @return 若相对上次发生变化则返回上一通道（首次建立时上一通道为 null）；未变化返回 [Unchanged]。
     */
    fun recordRx23(probeId: String, link: ProbeCommandLink): RouteChange? {
        val prev = lastRoute.put(probeId, link)
        lastRx23Ms[probeId] = System.currentTimeMillis()
        if (prev == link) return null
        return RouteChange(previous = prev, current = link)
    }

    data class RouteChange(
        val previous: ProbeCommandLink?,
        val current: ProbeCommandLink,
    )

    /** 已保存网口探头收到组播时刷新，延长 UI 层 0x23 超时窗口 */
    fun recordMulticastKeepalive(probeId: String) {
        lastMulticastMs[probeId] = System.currentTimeMillis()
    }

    fun routeFor(probeId: String): ProbeCommandLink? = lastRoute[probeId]

    fun lastRx23Millis(probeId: String): Long? = lastRx23Ms[probeId]

    fun lastMulticastKeepaliveMillis(probeId: String): Long? = lastMulticastMs[probeId]

    fun clear(probeId: String) {
        lastRoute.remove(probeId)
        lastRx23Ms.remove(probeId)
        lastMulticastMs.remove(probeId)
    }
}
