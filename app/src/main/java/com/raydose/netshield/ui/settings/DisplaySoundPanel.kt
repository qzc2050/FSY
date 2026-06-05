package com.raydose.netshield.ui.settings

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.AppLanguage
import com.raydose.netshield.model.DisplaySoundSettings
import com.raydose.netshield.model.ProbeCardDisplayMode

private val StandbyOptions = listOf(
    3 to "3 分钟",
    5 to "5 分钟",
    10 to "10 分钟",
    30 to "30 分钟",
    60 to "1 小时",
    -1 to "永不",
)

private val CardCountOptions = listOf(1, 2, 4)

@Composable
fun DisplaySoundPanel(
    settings: DisplaySoundSettings,
    onChange: (DisplaySoundSettings) -> Unit,
    onBrightnessCommitted: () -> Unit,
    onSystemVolumeCommitted: () -> Unit,
    onSaveClick: () -> Unit,
    onPreviewStandby: () -> Unit,
    modifier: Modifier = Modifier,
) {
    SettingsPanelScaffold(
        modifier = modifier.fillMaxSize(),
        onSaveClick = onSaveClick,
        extraActions = {
            OutlinedButton(onClick = onPreviewStandby) {
                Text("显示待机画面", fontSize = 16.sp)
            }
        },
    ) {
        SettingsDropdownRow(
            label = "系统语言",
            value = settings.language.label,
            options = AppLanguage.entries.map { it.label },
        ) { index -> onChange(settings.copy(language = AppLanguage.entries[index])) }

        SettingsDropdownRow(
            label = "监测组件显示",
            value = settings.probeCardMode.label,
            options = ProbeCardDisplayMode.entries.map { it.label },
        ) { index -> onChange(settings.copy(probeCardMode = ProbeCardDisplayMode.entries[index])) }

        val standbyIndex = StandbyOptions.indexOfFirst { it.first == settings.standbyMinutes }.coerceAtLeast(0)
        SettingsDropdownRow(
            label = "待机时间",
            value = StandbyOptions[standbyIndex].second,
            options = StandbyOptions.map { it.second },
        ) { index -> onChange(settings.copy(standbyMinutes = StandbyOptions[index].first)) }

        val sliderEndAtCenter = 0.618f
        SettingsSliderRow(
            label = "系统亮度",
            value = settings.brightness,
            sliderEndFraction = sliderEndAtCenter,
            onValueChange = { onChange(settings.copy(brightness = it)) },
            onValueChangeFinished = onBrightnessCommitted,
        )
        SettingsSliderRow(
            label = "系统音量",
            value = settings.systemVolume,
            sliderEndFraction = sliderEndAtCenter,
            onValueChange = { onChange(settings.copy(systemVolume = it)) },
            onValueChangeFinished = onSystemVolumeCommitted,
        )
        SettingsSliderRow(
            label = "本机报警音量",
            value = settings.hostAlarmVolume,
            sliderEndFraction = sliderEndAtCenter,
            onValueChange = { onChange(settings.copy(hostAlarmVolume = it)) },
        )
        SettingsSliderRow(
            label = "提示音量",
            value = settings.promptVolume,
            sliderEndFraction = sliderEndAtCenter,
            onValueChange = { onChange(settings.copy(promptVolume = it)) },
        )

        val countIndex = CardCountOptions.indexOf(settings.visibleProbeCards).coerceAtLeast(0)
        SettingsDropdownRow(
            label = "同时显示探头数",
            value = "${settings.visibleProbeCards}",
            options = CardCountOptions.map { "$it" },
        ) { index -> onChange(settings.copy(visibleProbeCards = CardCountOptions[index])) }

        SettingsSwitchRow("静音", settings.mute) { onChange(settings.copy(mute = it)) }
        SettingsSwitchRow("暂停报警 5 分钟", settings.pauseAlarmFiveMinutes) {
            onChange(settings.copy(pauseAlarmFiveMinutes = it))
        }
    }
}
