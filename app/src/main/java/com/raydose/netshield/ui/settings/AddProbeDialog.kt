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
import androidx.compose.foundation.layout.widthIn
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

/** 添加探头弹窗：屏宽 1/3，居中（须关闭平台默认窄宽）。 */
private const val ADD_PROBE_DIALOG_WIDTH_FRACTION = 1f / 3f

private val IdColumnWidth = 56.dp
private val ActionColumnWidth = 88.dp
private val ColumnGap = 32.dp

@Composable
fun AddProbeDialog(
    discovered: List<DiscoveredDevice>,
    draftProbes: List<SavedProbe>,
    onDismiss: () -> Unit,
    onAdd: (DiscoveredDevice) -> Unit,
) {
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
                    .fillMaxWidth(ADD_PROBE_DIALOG_WIDTH_FRACTION)
                    .clip(RoundedCornerShape(12.dp))
                    .background(NetShieldSettingsEditorPanel)
                    .padding(20.dp),
            ) {
                Text(
                    text = "添加探头 — 在线设备",
                    color = NetShieldTextPrimary,
                    fontSize = 26.sp,
                    fontWeight = FontWeight.SemiBold,
                )
                AddProbeTableHeader(
                    modifier = Modifier.padding(top = 16.dp, bottom = 10.dp),
                )
                if (discovered.isEmpty()) {
                    Text(
                        text = "未发现设备，请确认从机已上电且与主机同网段。",
                        color = NetShieldTextSecondary,
                        fontSize = 20.sp,
                        modifier = Modifier.padding(vertical = 28.dp),
                    )
                } else {
                    LazyColumn(
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(max = 320.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        items(discovered, key = { it.stableId }) { device ->
                            val added = draftProbes.any { matchesSaved(it, device) }
                            AddProbeDeviceRow(
                                device = device,
                                added = added,
                                onAdd = { onAdd(device) },
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
                        Text("关闭", color = NetShieldTextPrimary, fontSize = 20.sp)
                    }
                }
            }
        }
    }
}

@Composable
private fun AddProbeTableHeader(modifier: Modifier = Modifier) {
    Row(
        modifier = modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(ColumnGap),
    ) {
        Text(
            text = "型号",
            color = NetShieldTextSecondary,
            fontSize = 20.sp,
            modifier = Modifier.weight(1f),
        )
        Text(
            text = "IP",
            color = NetShieldTextSecondary,
            fontSize = 20.sp,
            modifier = Modifier.weight(1.2f),
        )
        Text(
            text = "ID",
            color = NetShieldTextSecondary,
            fontSize = 20.sp,
            modifier = Modifier.width(IdColumnWidth),
        )
        Text(
            text = "",
            modifier = Modifier.width(ActionColumnWidth),
        )
    }
}

@Composable
private fun AddProbeDeviceRow(
    device: DiscoveredDevice,
    added: Boolean,
    onAdd: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(NetShieldSettingsContentBg)
            .padding(horizontal = 12.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(ColumnGap),
    ) {
        Text(
            text = device.model.ifBlank { "—" },
            color = NetShieldTextPrimary,
            fontSize = 19.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.weight(1f),
        )
        Text(
            text = device.ip,
            color = NetShieldTextPrimary,
            fontSize = 19.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier
                .weight(1.2f)
                .widthIn(min = 168.dp),
        )
        Text(
            text = device.protoAddr,
            color = NetShieldTextPrimary,
            fontSize = 19.sp,
            maxLines = 1,
            modifier = Modifier.width(IdColumnWidth),
        )
        Box(
            modifier = Modifier.width(ActionColumnWidth),
            contentAlignment = Alignment.CenterEnd,
        ) {
            if (added) {
                Text(
                    text = "已添加",
                    color = NetShieldTextSecondary,
                    fontSize = 19.sp,
                )
            } else {
                TextButton(onClick = onAdd) {
                    Text("添加", color = NetShieldAccentBlue, fontSize = 19.sp)
                }
            }
        }
    }
}
