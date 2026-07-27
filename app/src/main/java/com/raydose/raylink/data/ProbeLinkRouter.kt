package com.raydose.raylink.data

import com.raydose.raylink.model.ProbeCommandLink
import java.util.concurrent.ConcurrentHashMap

/**
 * 按探头记录**实时 0x23** 的来源（网口/串口），用于下发选路与切换日志。
 * 五分钟等其它帧不写入此处。
 *
 * 网线重插窗口内 TCP 与 LoRa 可能短暂并存：已走网口时，短时内串口 0x23 不抢回，避免选路抖动。
 */
class ProbeLinkRouter {
    private val lastRoute = ConcurrentHashMap<String, ProbeCommandLink>()
    private val lastRx23Ms = ConcurrentHashMap<String, Long>()
    /** 网口组播发现刷新，用于 0x23 超时判定 */
    private val lastMulticastMs = ConcurrentHashMap<String, Long>()

    /**
     * 记录 0x23 来源。
     * @return 若相对上次发生变化则返回上一通道（首次建立时上一通道为 null）；未变化返回 null。
     */
    fun recordRx23(probeId: String, link: ProbeCommandLink): RouteChange? {
        val now = System.currentTimeMillis()
        val prev = lastRoute[probeId]
        if (prev == ProbeCommandLink.NETWORK && link == ProbeCommandLink.SERIAL) {
            val lastNet = lastRx23Ms[probeId]
            val mcast = lastMulticastMs[probeId]
            val netFresh = lastNet != null && now - lastNet < NETWORK_HOLD_MS
            val mcastFresh = mcast != null && now - mcast < NETWORK_HOLD_MS
            if (netFresh || mcastFresh) {
                return null
            }
        }
        lastRoute[probeId] = link
        lastRx23Ms[probeId] = now
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

    companion object {
        /** 网口活跃时，串口 0x23 在此窗口内不抢占选路 */
        private const val NETWORK_HOLD_MS = 5_000L
    }
}
