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
import com.raydose.raylink.ui.theme.RaylinkDoorOpen
import com.raydose.raylink.ui.theme.RaylinkSettingsEditorPanel
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary

@Composable
fun DeleteProbeConfirmDialog(
    probeName: String,
    onDismiss: () -> Unit,
    onConfirm: () -> Unit,
) {
    Dialog(onDismissRequest = onDismiss) {
        Column(
            modifier = Modifier
                .fillMaxWidth(0.55f)
                .clip(RoundedCornerShape(12.dp))
                .background(RaylinkSettingsEditorPanel)
                .padding(24.dp),
        ) {
            Text(
                text = stringResource(R.string.delete_probe_title),
                color = RaylinkTextPrimary,
                fontSize = 22.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Text(
                text = stringResource(R.string.delete_probe_message, probeName),
                color = RaylinkTextSecondary,
                fontSize = 17.sp,
                modifier = Modifier.padding(top = 12.dp),
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
                    Text(stringResource(R.string.delete_probe_confirm), color = RaylinkDoorOpen, fontSize = 17.sp)
                }
            }
        }
    }
}
