package com.raydose.netshield.data

import com.raydose.netshield.model.ProbeCommandLink
import java.util.concurrent.ConcurrentHashMap

/**
 * 按序列号（probeId）记录该设备**最后一条实时 0x23** 的来源，用于决定下发走串口还是 TCP。
 */
class ProbeLinkRouter {
  private val lastRoute = ConcurrentHashMap<String, ProbeCommandLink>()
  private val lastRx23Ms = ConcurrentHashMap<String, Long>()
  /** 网口组播发现刷新，用于 0x23 超时判定 */
  private val lastMulticastMs = ConcurrentHashMap<String, Long>()

  fun recordRx23(probeId: String, link: ProbeCommandLink) {
    lastRoute[probeId] = link
    lastRx23Ms[probeId] = System.currentTimeMillis()
  }

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
