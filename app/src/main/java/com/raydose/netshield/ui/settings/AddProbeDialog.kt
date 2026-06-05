package com.raydose.netshield.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.raydose.netshield.model.DiscoveredDevice
import com.raydose.netshield.model.SavedProbe
import com.raydose.netshield.model.matchesSaved
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldSettingsContentBg
import com.raydose.netshield.ui.theme.NetShieldSettingsEditorPanel
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

@Composable
fun AddProbeDialog(
    discovered: List<DiscoveredDevice>,
    draftProbes: List<SavedProbe>,
    onDismiss: () -> Unit,
    onAdd: (DiscoveredDevice) -> Unit,
) {
    Dialog(onDismissRequest = onDismiss) {
        Column(
            modifier = Modifier
                .fillMaxWidth(0.72f)
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
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 16.dp, bottom = 10.dp),
            ) {
                Text("型号", color = NetShieldTextSecondary, fontSize = 20.sp, modifier = Modifier.weight(1.2f))
                Text("IP", color = NetShieldTextSecondary, fontSize = 20.sp, modifier = Modifier.weight(1.4f))
                Text("ID", color = NetShieldTextSecondary, fontSize = 20.sp, modifier = Modifier.weight(0.6f))
                Text("", modifier = Modifier.weight(0.8f))
            }
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
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(8.dp))
                                .background(NetShieldSettingsContentBg)
                                .padding(horizontal = 12.dp, vertical = 12.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text(
                                device.model.ifBlank { "—" },
                                color = NetShieldTextPrimary,
                                fontSize = 19.sp,
                                modifier = Modifier.weight(1.2f),
                            )
                            Text(
                                device.ip,
                                color = NetShieldTextPrimary,
                                fontSize = 19.sp,
                                modifier = Modifier.weight(1.4f),
                            )
                            Text(
                                device.protoAddr,
                                color = NetShieldTextPrimary,
                                fontSize = 19.sp,
                                modifier = Modifier.weight(0.6f),
                            )
                            if (added) {
                                Text(
                                    "已添加",
                                    color = NetShieldTextSecondary,
                                    fontSize = 19.sp,
                                    modifier = Modifier.weight(0.8f),
                                )
                            } else {
                                TextButton(
                                    onClick = { onAdd(device) },
                                    modifier = Modifier.weight(0.8f),
                                ) {
                                    Text("添加", color = NetShieldAccentBlue, fontSize = 19.sp)
                                }
                            }
                        }
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
