package com.raydose.netshield.ui.home

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.ui.components.ProbeCardSlotSize
import com.raydose.netshield.ui.components.SlaveProbeCard
import com.raydose.netshield.ui.theme.NetShieldCardOffline
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

/** 主页探头区分页数（同时显示 [visiblePerPage] 个为一页）。 */
fun homeProbePageCount(probeCount: Int, visiblePerPage: Int): Int {
    if (probeCount <= 0) return 1
    val per = visiblePerPage.coerceIn(1, 4)
    return (probeCount + per - 1) / per
}

/** 取第 [page] 页要占用的格位（1/2/4 格，不足补 null）。 */
fun homeProbeGridSlots(
    probes: List<SlaveProbeUi>,
    page: Int,
    visiblePerPage: Int,
): List<SlaveProbeUi?> {
    val per = visiblePerPage.coerceIn(1, 4)
    val start = page * per
    return List(per) { offset -> probes.getOrNull(start + offset) }
}

@Composable
fun HomeProbeGridPage(
    slots: List<SlaveProbeUi?>,
    visiblePerPage: Int,
    onProbeDetailClick: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    val count = visiblePerPage.coerceIn(1, 4)
    val slotSize = when (count) {
        1 -> ProbeCardSlotSize.Full
        2 -> ProbeCardSlotSize.Half
        else -> ProbeCardSlotSize.Quarter
    }
    val gap = when (count) {
        1 -> 0.dp
        2 -> 10.dp
        else -> 8.dp
    }

    when (count) {
        1 -> {
            val probe = slots.firstOrNull()
            if (probe != null) {
                SlaveProbeCard(
                    probe = probe,
                    onDetailClick = { onProbeDetailClick(probe.id) },
                    slotSize = slotSize,
                    modifier = modifier.fillMaxSize(),
                )
            } else {
                EmptyProbeGridCell(modifier = modifier.fillMaxSize())
            }
        }
        2 -> Row(
            modifier = modifier.fillMaxSize(),
            horizontalArrangement = Arrangement.spacedBy(gap),
        ) {
            slots.take(2).forEach { probe ->
                ProbeGridCell(
                    probe = probe,
                    slotSize = slotSize,
                    onProbeDetailClick = onProbeDetailClick,
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxHeight(),
                )
            }
        }
        else -> Column(
            modifier = modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(gap),
        ) {
            Row(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxSize(),
                horizontalArrangement = Arrangement.spacedBy(gap),
            ) {
                slots.take(2).forEach { probe ->
                    ProbeGridCell(
                        probe = probe,
                        slotSize = slotSize,
                        onProbeDetailClick = onProbeDetailClick,
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxHeight(),
                    )
                }
            }
            Row(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxSize(),
                horizontalArrangement = Arrangement.spacedBy(gap),
            ) {
                slots.drop(2).take(2).forEach { probe ->
                    ProbeGridCell(
                        probe = probe,
                        slotSize = slotSize,
                        onProbeDetailClick = onProbeDetailClick,
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxHeight(),
                    )
                }
            }
        }
    }
}

@Composable
private fun ProbeGridCell(
    probe: SlaveProbeUi?,
    slotSize: ProbeCardSlotSize,
    onProbeDetailClick: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    if (probe != null) {
        SlaveProbeCard(
            probe = probe,
            onDetailClick = { onProbeDetailClick(probe.id) },
            slotSize = slotSize,
            modifier = modifier,
        )
    } else {
        EmptyProbeGridCell(modifier = modifier)
    }
}

@Composable
private fun EmptyProbeGridCell(modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(16.dp))
            .background(NetShieldCardOffline.copy(alpha = 0.35f)),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = "空位",
            color = NetShieldTextSecondary.copy(alpha = 0.5f),
            fontSize = 16.sp,
        )
    }
}
