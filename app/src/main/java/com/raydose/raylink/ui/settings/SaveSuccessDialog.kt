package com.raydose.raylink.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.R
import com.raydose.raylink.ui.theme.RaylinkSettingsContentBg
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import kotlinx.coroutines.delay

/** 报警音量与分页「1/1」之间的保存成功提示，1 秒后自动消失。 */
@Composable
fun SaveSuccessToast(onDismiss: () -> Unit) {
    LaunchedEffect(Unit) {
        delay(1000)
        onDismiss()
    }
    Box(
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .background(RaylinkSettingsContentBg)
            .padding(horizontal = 18.dp, vertical = 8.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = stringResource(R.string.settings_save_success),
            color = RaylinkTextPrimary,
            fontSize = 16.sp,
            fontWeight = FontWeight.Medium,
        )
    }
}
