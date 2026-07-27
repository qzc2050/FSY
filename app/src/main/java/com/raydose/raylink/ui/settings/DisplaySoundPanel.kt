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
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.R
import com.raydose.raylink.model.AppLanguage
import com.raydose.raylink.model.DisplaySoundSettings
import com.raydose.raylink.model.ProbeCardDisplayMode
import com.raydose.raylink.model.isPauseAlarmActive
import com.raydose.raylink.ui.labelText
import kotlinx.coroutines.delay

private val StandbyOptionMinutes = listOf(3, 5, 10, 30, 60, -1)

private val CardCountOptions = listOf(1, 2, 4)

private val DisplaySoundLabelSp = 20.sp
private val DisplaySoundValueSp = 19.sp
private val DisplaySoundMenuSp = 18.sp
private val DisplaySoundPercentSp = 17.sp
private val DisplaySoundButtonSp = 18.sp
/** 20sp 标签需更宽列宽，控件整体右移，避免长标签换行 */
private val DisplaySoundLabelWidth = 220.dp

@Composable
private fun standbyMinuteLabel(minutes: Int): String = when (minutes) {
    3 -> stringResource(R.string.settings_standby_3min)
    5 -> stringResource(R.string.settings_standby_5min)
    10 -> stringResource(R.string.settings_standby_10min)
    30 -> stringResource(R.string.settings_standby_30min)
    60 -> stringResource(R.string.settings_standby_1hour)
    else -> stringResource(R.string.settings_standby_never)
}

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

    val languageLabels = listOf(AppLanguage.Zh.labelText(), AppLanguage.En.labelText())
    val probeDisplayLabels = listOf(
        ProbeCardDisplayMode.Fixed.labelText(),
        ProbeCardDisplayMode.Scroll.labelText(),
    )
    val standbyLabels = StandbyOptionMinutes.map { standbyMinuteLabel(it) }

    SettingsPanelScaffold(
        modifier = modifier.fillMaxSize(),
        onSaveClick = onSaveClick,
        extraActions = {
            OutlinedButton(onClick = onPreviewStandby) {
                Text(stringResource(R.string.settings_show_standby), fontSize = DisplaySoundButtonSp)
            }
        },
    ) {
        SettingsDropdownRow(
            label = stringResource(R.string.settings_system_language),
            value = settings.language.labelText(),
            options = languageLabels,
            labelFontSize = DisplaySoundLabelSp,
            valueFontSize = DisplaySoundValueSp,
            menuFontSize = DisplaySoundMenuSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
        ) { index -> onChange(settings.copy(language = AppLanguage.entries[index])) }

        SettingsDropdownRow(
            label = stringResource(R.string.settings_probe_display_mode),
            value = settings.probeCardMode.labelText(),
            options = probeDisplayLabels,
            labelFontSize = DisplaySoundLabelSp,
            valueFontSize = DisplaySoundValueSp,
            menuFontSize = DisplaySoundMenuSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
        ) { index -> onChange(settings.copy(probeCardMode = ProbeCardDisplayMode.entries[index])) }

        val standbyIndex = StandbyOptionMinutes.indexOfFirst { it == settings.standbyMinutes }.coerceAtLeast(0)
        SettingsDropdownRow(
            label = stringResource(R.string.settings_standby_time),
            value = standbyLabels[standbyIndex],
            options = standbyLabels,
            labelFontSize = DisplaySoundLabelSp,
            valueFontSize = DisplaySoundValueSp,
            menuFontSize = DisplaySoundMenuSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
        ) { index -> onChange(settings.copy(standbyMinutes = StandbyOptionMinutes[index])) }

        val sliderEndAtCenter = 0.618f
        SettingsSliderRow(
            label = stringResource(R.string.settings_brightness),
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
            label = stringResource(R.string.settings_system_volume),
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
            label = stringResource(R.string.settings_host_alarm_volume),
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
            label = stringResource(R.string.settings_prompt_volume),
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
            label = stringResource(R.string.settings_visible_probe_count),
            value = "${settings.visibleProbeCards}",
            options = CardCountOptions.map { "$it" },
            labelFontSize = DisplaySoundLabelSp,
            valueFontSize = DisplaySoundValueSp,
            menuFontSize = DisplaySoundMenuSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
        ) { index -> onChange(settings.copy(visibleProbeCards = CardCountOptions[index])) }

        SettingsSwitchRow(
            label = stringResource(R.string.settings_mute),
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
            stringResource(
                R.string.settings_pause_alarm_active,
                remainingSec / 60L,
                remainingSec % 60L,
            )
        } else {
            stringResource(R.string.settings_pause_alarm_idle)
        }
        SettingsLabeledButtonRow(
            label = stringResource(R.string.settings_pause_alarm_label),
            buttonText = pauseButtonText,
            onClick = onPauseAlarmClick,
            labelFontSize = DisplaySoundLabelSp,
            buttonFontSize = DisplaySoundButtonSp,
            labelWidth = DisplaySoundLabelWidth,
            labelSingleLine = true,
        )
    }
}
