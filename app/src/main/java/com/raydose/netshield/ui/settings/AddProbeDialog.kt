package com.raydose.netshield.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.raydose.netshield.model.DiscoveredDevice
import com.raydose.netshield.model.SavedProbe
import com.raydose.netshield.model.matchesSaved
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldSettingsContentBg
import com.raydose.netshield.ui.theme.NetShieldSettingsEditorPanel
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.rememberTabletFormFactor
import com.raydose.netshield.ui.theme.ScreenSpec
import com.raydose.netshield.ui.theme.TabletFormFactor

/** 添加探头弹窗：10 寸 1/2 宽居中，13 寸 1/3 宽居中（须关闭平台默认窄宽）。 */
private val IdColumnWidth = 56.dp
private val IdColumnWidthCompact = 48.dp
private val ActionColumnWidth = 88.dp
private val ActionColumnWidthCompact = 72.dp
private val ColumnGap = 32.dp
private val ColumnGapCompact = 12.dp

@Composable
fun AddProbeDialog(
    discovered: List<DiscoveredDevice>,
    draftProbes: List<SavedProbe>,
    onDismiss: () -> Unit,
    onAdd: (DiscoveredDevice) -> Unit,
) {
    val formFactor = rememberTabletFormFactor()
    val isCompact = formFactor == TabletFormFactor.Compact
    val dialogWidthFraction = ScreenSpec.addProbeDialogWidthFraction(formFactor)
    val columnGap = if (isCompact) ColumnGapCompact else ColumnGap
    val titleSp = if (isCompact) 22.sp else 26.sp
    val bodySp = if (isCompact) 17.sp else 19.sp
    val headerSp = if (isCompact) 18.sp else 20.sp
    val idColumnWidth = if (isCompact) IdColumnWidthCompact else IdColumnWidth
    val actionColumnWidth = if (isCompact) ActionColumnWidthCompact else ActionColumnWidth
    val panelPadding = if (isCompact) 16.dp else 20.dp
    val listMaxHeight = if (isCompact) 360.dp else 320.dp

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false),
    ) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 24.dp),
            contentAlignment = Alignment.Center,
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth(dialogWidthFraction)
                    .clip(RoundedCornerShape(12.dp))
                    .background(NetShieldSettingsEditorPanel)
                    .padding(panelPadding),
            ) {
                Text(
                    text = "添加探头 — 在线设备",
                    color = NetShieldTextPrimary,
                    fontSize = titleSp,
                    fontWeight = FontWeight.SemiBold,
                )
                AddProbeTableHeader(
                    modifier = Modifier.padding(top = 16.dp, bottom = 10.dp),
                    columnGap = columnGap,
                    headerSp = headerSp,
                    idColumnWidth = idColumnWidth,
                    actionColumnWidth = actionColumnWidth,
                    ipWeight = if (isCompact) 1.6f else 1.2f,
                )
                if (discovered.isEmpty()) {
                    Text(
                        text = "未发现设备，请确认从机已上电（同网段组播，或经转接板串口/CAN）。",
                        color = NetShieldTextSecondary,
                        fontSize = headerSp,
                        modifier = Modifier.padding(vertical = 28.dp),
                    )
                } else {
                    LazyColumn(
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(max = listMaxHeight),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        items(discovered, key = { it.stableId }) { device ->
                            val added = draftProbes.any { matchesSaved(it, device) }
                            AddProbeDeviceRow(
                                device = device,
                                added = added,
                                onAdd = { onAdd(device) },
                                columnGap = columnGap,
                                bodySp = bodySp,
                                idColumnWidth = idColumnWidth,
                                actionColumnWidth = actionColumnWidth,
                                ipWeight = if (isCompact) 1.6f else 1.2f,
                            )
                        }
                    }
                }
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 16.dp),
                    horizontalArrangement = Arrangement.End,
                ) {
                    TextButton(onClick = onDismiss) {
                        Text("关闭", color = NetShieldTextPrimary, fontSize = headerSp)
                    }
                }
            }
        }
    }
}

@Composable
private fun AddProbeTableHeader(
    modifier: Modifier = Modifier,
    columnGap: Dp,
    headerSp: TextUnit,
    idColumnWidth: Dp,
    actionColumnWidth: Dp,
    ipWeight: Float,
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(columnGap),
    ) {
        Text(
            text = "型号",
            color = NetShieldTextSecondary,
            fontSize = headerSp,
            modifier = Modifier.weight(1f),
        )
        Text(
            text = "IP",
            color = NetShieldTextSecondary,
            fontSize = headerSp,
            modifier = Modifier.weight(ipWeight),
        )
        Text(
            text = "ID",
            color = NetShieldTextSecondary,
            fontSize = headerSp,
            modifier = Modifier.width(idColumnWidth),
        )
        Text(
            text = "",
            modifier = Modifier.width(actionColumnWidth),
        )
    }
}

@Composable
private fun AddProbeDeviceRow(
    device: DiscoveredDevice,
    added: Boolean,
    onAdd: () -> Unit,
    columnGap: Dp,
    bodySp: TextUnit,
    idColumnWidth: Dp,
    actionColumnWidth: Dp,
    ipWeight: Float,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(NetShieldSettingsContentBg)
            .padding(horizontal = 12.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(columnGap),
    ) {
        Text(
            text = device.model.ifBlank { "—" },
            color = NetShieldTextPrimary,
            fontSize = bodySp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.weight(1f),
        )
        Text(
            text = device.ip.ifBlank { "串口/CAN" },
            color = NetShieldTextPrimary,
            fontSize = bodySp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.weight(ipWeight),
        )
        Text(
            text = device.protoAddr,
            color = NetShieldTextPrimary,
            fontSize = bodySp,
            maxLines = 1,
            modifier = Modifier.width(idColumnWidth),
        )
        Box(
            modifier = Modifier.width(actionColumnWidth),
            contentAlignment = Alignment.CenterEnd,
        ) {
            if (added) {
                Text(
                    text = "已添加",
                    color = NetShieldTextSecondary,
                    fontSize = bodySp,
                )
            } else {
                TextButton(onClick = onAdd) {
                    Text("添加", color = NetShieldAccentBlue, fontSize = bodySp)
                }
            }
        }
    }
}
