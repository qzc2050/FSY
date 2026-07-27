package com.raydose.raylink.ui.settings

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
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.raydose.raylink.R
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkSettingsEditorPanel
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary

@Composable
fun ZjbFirmwareConfirmDialog(
    fileName: String,
    sizeBytes: Long,
    onDismiss: () -> Unit,
    onConfirm: () -> Unit,
    title: String? = null,
    hint: String? = null,
) {
    val resolvedTitle = title ?: stringResource(R.string.zjb_fw_confirm_title)
    val resolvedHint = hint ?: stringResource(R.string.zjb_fw_confirm_hint)

    Dialog(onDismissRequest = onDismiss) {
        Column(
            modifier = Modifier
                .fillMaxWidth(0.62f)
                .clip(RoundedCornerShape(12.dp))
                .background(RaylinkSettingsEditorPanel)
                .padding(24.dp),
        ) {
            Text(
                text = resolvedTitle,
                color = RaylinkTextPrimary,
                fontSize = 22.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Text(
                text = stringResource(R.string.zjb_fw_file_label, fileName),
                color = RaylinkTextSecondary,
                fontSize = 17.sp,
                modifier = Modifier.padding(top = 12.dp),
            )
            Text(
                text = stringResource(R.string.zjb_fw_size_label, formatFirmwareSize(sizeBytes)),
                color = RaylinkTextSecondary,
                fontSize = 17.sp,
                modifier = Modifier.padding(top = 6.dp),
            )
            Text(
                text = resolvedHint,
                color = RaylinkTextSecondary,
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
                    Text(stringResource(R.string.action_cancel), color = RaylinkTextPrimary, fontSize = 17.sp)
                }
                TextButton(onClick = onConfirm) {
                    Text(stringResource(R.string.zjb_fw_confirm_action), color = RaylinkAccentBlue, fontSize = 17.sp)
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
