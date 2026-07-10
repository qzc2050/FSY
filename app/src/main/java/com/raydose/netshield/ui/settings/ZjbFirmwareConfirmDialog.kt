package com.raydose.netshield.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldSettingsEditorPanel
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

@Composable
fun ZjbFirmwareConfirmDialog(
    fileName: String,
    sizeBytes: Long,
    onDismiss: () -> Unit,
    onConfirm: () -> Unit,
) {
    Dialog(onDismissRequest = onDismiss) {
        Column(
            modifier = Modifier
                .fillMaxWidth(0.62f)
                .clip(RoundedCornerShape(12.dp))
                .background(NetShieldSettingsEditorPanel)
                .padding(24.dp),
        ) {
            Text(
                text = "确认升级转接板固件？",
                color = NetShieldTextPrimary,
                fontSize = 22.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Text(
                text = "文件：$fileName",
                color = NetShieldTextSecondary,
                fontSize = 17.sp,
                modifier = Modifier.padding(top = 12.dp),
            )
            Text(
                text = "大小：${formatFirmwareSize(sizeBytes)}",
                color = NetShieldTextSecondary,
                fontSize = 17.sp,
                modifier = Modifier.padding(top = 6.dp),
            )
            Text(
                text = "升级完成后转接板将自动重启，请稍候再查看硬件版本。",
                color = NetShieldTextSecondary,
                fontSize = 16.sp,
                modifier = Modifier.padding(top = 10.dp),
            )
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 24.dp),
                horizontalArrangement = Arrangement.End,
            ) {
                TextButton(onClick = onDismiss) {
                    Text("取消", color = NetShieldTextPrimary, fontSize = 17.sp)
                }
                TextButton(onClick = onConfirm) {
                    Text("确认升级", color = NetShieldAccentBlue, fontSize = 17.sp)
                }
            }
        }
    }
}

private fun formatFirmwareSize(sizeBytes: Long): String {
    if (sizeBytes < 1024L) return "${sizeBytes} B"
    val kb = sizeBytes / 1024.0
    return if (kb >= 100.0) {
        String.format("%.0f KB", kb)
    } else {
        String.format("%.1f KB", kb)
    }
}
