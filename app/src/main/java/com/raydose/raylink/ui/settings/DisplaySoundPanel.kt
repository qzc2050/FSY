package com.raydose.raylink.ui.settings

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.model.AppLanguage
import com.raydose.raylink.model.DisplaySoundSettings
import com.raydose.raylink.model.ProbeCardDisplayMode
import com.raydose.raylink.model.isPauseAlarmActive
import kotlinx.coroutines.delay

private val StandbyOptions = listOf(
    3 to "3 分钟",
    5 to "5 分钟",
    10 to "10 分钟",
    30 to "30 分钟",
    60 to "1 小时",
    -1 to "永不",
)

private val CardCountOptions = listOf(1, 2, 4)

private val DisplaySoundLabelSp = 20.sp
private val DisplaySoundValueSp = 19.sp
private val DisplaySoundMenuSp = 18.sp
private val DisplaySoundPercentSp = 17.sp
private val DisplaySoundButtonSp = 18.sp
/** 20sp 标签需更宽列宽，控件整体右移，避免长标签换行 */
private val DisplaySoundLabelWidth = 176.dp

@Composable
fun DisplaySoundPanel(
    settings: DisplaySoundSettings,
    onChange: (DisplaySoundSettings) -> Unit,
    onBrightnessPreview: (Float) -> Unit,
    onBrightnessCommitted: () -> Unit,
    onSystemVolumeCommitted: () -> Unit,
    onHostAlarmVolumeCommitted: () -> Unit,
    onPromptVolumeCommitted: () -> Unit,
    onMuteCommitted: (Boolean) -> Unit,
    onPauseAlarmClick: () -> Unit,
    onSaveClick: () -> Unit,
    onPreviewStandby: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var nowMillis by remember { mutableLongStateOf(System.currentTimeMillis()) }
    LaunchedEffect(settings.pauseAlarmUntilMillis) {
        while (true) {
            nowMillis = System.currentTimeMillis()
            delay(1_000L)
        }
    }

    SettingsPanelScaffold(
        modifier = modifier.fillMaxSize(),
        onSaveClick = onSaveClick,
        extraActions = {
            OutlinedButton(onClick = onPreviewStandby) {
                Text("显示待机画面", fontSize = DisplaySoundButtonSp)
            }
        },
    ) {
        SettingsDropdownRow(
            label = "系统语言",
            value = settings.language.label,
            options = AppLanguage.entries.map { it.label },
            labelFontSize = DisplaySoundLabelSp,
            valueFontSize = DisplaySoundValueSp,
            menuFontSize = DisplaySoundMenuSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
        ) { index -> onChange(settings.copy(language = AppLanguage.entries[index])) }

        SettingsDropdownRow(
            label = "监测组件显示",
            value = settings.probeCardMode.label,
            options = ProbeCardDisplayMode.entries.map { it.label },
            labelFontSize = DisplaySoundLabelSp,
            valueFontSize = DisplaySoundValueSp,
            menuFontSize = DisplaySoundMenuSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
        ) { index -> onChange(settings.copy(probeCardMode = ProbeCardDisplayMode.entries[index])) }

        val standbyIndex = StandbyOptions.indexOfFirst { it.first == settings.standbyMinutes }.coerceAtLeast(0)
        SettingsDropdownRow(
            label = "待机时间",
            value = StandbyOptions[standbyIndex].second,
            options = StandbyOptions.map { it.second },
            labelFontSize = DisplaySoundLabelSp,
            valueFontSize = DisplaySoundValueSp,
            menuFontSize = DisplaySoundMenuSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
        ) { index -> onChange(settings.copy(standbyMinutes = StandbyOptions[index].first)) }

        val sliderEndAtCenter = 0.618f
        SettingsSliderRow(
            label = "系统亮度",
            value = settings.brightness,
            sliderEndFraction = sliderEndAtCenter,
            labelFontSize = DisplaySoundLabelSp,
            percentFontSize = DisplaySoundPercentSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
            onValueChange = { value ->
                onChange(settings.copy(brightness = value))
                onBrightnessPreview(value)
            },
            onValueChangeFinished = onBrightnessCommitted,
        )
        SettingsSliderRow(
            label = "系统音量",
            value = settings.systemVolume,
            sliderEndFraction = sliderEndAtCenter,
            labelFontSize = DisplaySoundLabelSp,
            percentFontSize = DisplaySoundPercentSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
            onValueChange = { onChange(settings.copy(systemVolume = it)) },
            onValueChangeFinished = onSystemVolumeCommitted,
        )
        SettingsSliderRow(
            label = "本机报警音量",
            value = settings.hostAlarmVolume,
            sliderEndFraction = sliderEndAtCenter,
            labelFontSize = DisplaySoundLabelSp,
            percentFontSize = DisplaySoundPercentSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
            onValueChange = { onChange(settings.copy(hostAlarmVolume = it)) },
            onValueChangeFinished = onHostAlarmVolumeCommitted,
        )
        SettingsSliderRow(
            label = "提示音量",
            value = settings.promptVolume,
            sliderEndFraction = sliderEndAtCenter,
            labelFontSize = DisplaySoundLabelSp,
            percentFontSize = DisplaySoundPercentSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
            onValueChange = { onChange(settings.copy(promptVolume = it)) },
            onValueChangeFinished = onPromptVolumeCommitted,
        )

        val countIndex = CardCountOptions.indexOf(settings.visibleProbeCards).coerceAtLeast(0)
        SettingsDropdownRow(
            label = "同时显示探头数",
            value = "${settings.visibleProbeCards}",
            options = CardCountOptions.map { "$it" },
            labelFontSize = DisplaySoundLabelSp,
            valueFontSize = DisplaySoundValueSp,
            menuFontSize = DisplaySoundMenuSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
        ) { index -> onChange(settings.copy(visibleProbeCards = CardCountOptions[index])) }

        SettingsSwitchRow(
            label = "静音",
            checked = settings.mute,
            onCheckedChange = onMuteCommitted,
            labelFontSize = DisplaySoundLabelSp,
            enlargedSwitch = true,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
        )

        val pauseActive = settings.isPauseAlarmActive(nowMillis)
        val remainingSec = ((settings.pauseAlarmUntilMillis - nowMillis) / 1000L).coerceAtLeast(0L)
        val pauseButtonText = if (pauseActive) {
            val minutes = remainingSec / 60L
            val seconds = remainingSec % 60L
            "暂停中 ${minutes}:${"%02d".format(seconds)}"
        } else {
            "暂停 5 分钟"
        }
        SettingsLabeledButtonRow(
            label = "暂停报警5分钟",
            buttonText = pauseButtonText,
            onClick = onPauseAlarmClick,
            labelFontSize = DisplaySoundLabelSp,
            buttonFontSize = DisplaySoundButtonSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
        )
    }
}
