package com.raydose.netshield.ui.settings

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.TimeSettings
import com.raydose.netshield.ui.theme.NetShieldTextPrimary

/** 与开关行数值区对齐的日期/时间字号 */
private val TimeValueFontSize = 26.sp

@Composable
fun TimeSettingsPanel(
    settings: TimeSettings,
    systemTimeHint: String,
    onChange: (TimeSettings) -> Unit,
    onSyncToDevice: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val (datePart, timePart) = systemTimeHint.split(" ", limit = 2).let {
        it.firstOrNull().orEmpty() to it.getOrElse(1) { "" }
    }
    SettingsScrollContent(modifier = modifier.fillMaxSize()) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 8.dp),
        ) {
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(
                    SettingsFormRowSpacing,
                ),
            ) {
                TimeDateValueRow(label = "日期", value = datePart)
                TimeDateValueRow(label = "时间", value = timePart)
            }
            Box(modifier = Modifier.matchParentSize()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth(0.5f)
                        .fillMaxHeight()
                        .align(Alignment.CenterEnd),
                    contentAlignment = Alignment.Center,
                ) {
                    SettingsInlineActionButton(
                        text = "同步到设备",
                        onClick = onSyncToDevice,
                        filled = true,
                    )
                }
            }
        }
        SettingsSwitchRow("24 小时制", settings.use24Hour) {
            onChange(settings.copy(use24Hour = it))
        }
        SettingsSwitchRow("农历显示", settings.showLunar) {
            onChange(settings.copy(showLunar = it))
        }
        SettingsSwitchRow("公历显示", settings.showGregorian) {
            onChange(settings.copy(showGregorian = it))
        }
        SettingsSwitchRow("节假日显示", settings.showHoliday) {
            onChange(settings.copy(showHoliday = it))
        }
    }
}

/** 标签与 [SettingsSwitchRow] 左列对齐，数值与 Switch 起始位置对齐 */
@Composable
private fun TimeDateValueRow(label: String, value: String) {
    androidx.compose.foundation.layout.Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(label)
        Text(
            text = value.ifBlank { "—" },
            color = NetShieldTextPrimary,
            fontSize = TimeValueFontSize,
            fontWeight = FontWeight.Light,
        )
    }
}
