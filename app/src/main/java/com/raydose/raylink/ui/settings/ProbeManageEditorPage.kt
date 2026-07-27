package com.raydose.raylink.ui.settings

import androidx.compose.foundation.clickable
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import com.raydose.raylink.ui.components.RaylinkSlider
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.stringResource
import com.raydose.raylink.R
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.model.ProbeManageDraft
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkDoorOpen
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import com.raydose.raylink.ui.theme.TabletFormFactor
import com.raydose.raylink.ui.theme.rememberTabletFormFactor

private val FormLabelWidth = 148.dp
private val FormRowSpacing = 14.dp
private val FormColumnGap = 28.dp
private val FieldTextColor = Color(0xFF1E2433)

private val FormLabelSp = 20.sp
private val FormFieldSp = 19.sp
private val FormDataDetailSp = 22.sp
private val FormActionSp = 18.sp
private val FormUnitSp = 16.sp
private val FormToggleLabelSp = 20.sp
private val FormSwitchLabelGap = 14.dp
private val FormSwitchRowMinHeight = 52.dp
private val DeleteIconSize = 36.dp
private val DeleteButtonSize = 56.dp

@Composable
fun ProbeManageEditorPage(
    draft: ProbeManageDraft,
    onDraftChange: (ProbeManageDraft) -> Unit,
    onDataDetailClick: () -> Unit,
    onDeleteClick: () -> Unit,
    onVolumeCommitted: () -> Unit,
    onFirmwareUpdateClick: () -> Unit = {},
    modifier: Modifier = Modifier,
) {
    val formFactor = rememberTabletFormFactor()
    val isCompact = formFactor == TabletFormFactor.Compact
    val rowSpacing = if (isCompact) 10.dp else FormRowSpacing
    val columnGap = if (isCompact) 16.dp else FormColumnGap

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = if (isCompact) 12.dp else 28.dp, vertical = 12.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onDataDetailClick) {
                Text(
                    text = stringResource(R.string.probe_section_data_details),
                    color = RaylinkAccentBlue,
                    fontSize = FormDataDetailSp,
                )
            }
            ExternalAlarmBadge(connected = draft.externalAlarmConnected)
        }

        Spacer(modifier = Modifier.height(14.dp))

        ProbeFormGridRow(
            gap = columnGap,
            left = {
                ProbeInfoLabelCell(label = stringResource(R.string.probe_label_model), value = draft.savedProbe.model)
            },
            right = {
                ProbeInfoLabelCell(label = stringResource(R.string.probe_label_serial), value = draft.savedProbe.serial)
            },
        )

        Spacer(modifier = Modifier.height(rowSpacing))

        ProbeFormGridRow(
            gap = columnGap,
            left = {
                ProbeInfoLabelCell(label = stringResource(R.string.network_label_ip), value = draft.ip)
            },
            right = {
                ProbeInfoLabelCell(label = stringResource(R.string.network_label_device_id), value = draft.protoAddr)
            },
        )

        Spacer(modifier = Modifier.height(rowSpacing))

        ProbeFormGridRow(
            gap = columnGap,
            left = {
                ProbeInfoLabelCell(
                    label = stringResource(R.string.probe_label_software_version),
                    value = draft.softwareVersion.ifBlank { "—" },
                )
            },
            right = {
                // 与上一行「设备 ID」label 左对齐（右半列起始）
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = FormSwitchRowMinHeight),
                    contentAlignment = Alignment.CenterStart,
                ) {
                    SettingsInlineActionButton(
                        text = stringResource(R.string.action_update),
                        onClick = onFirmwareUpdateClick,
                        filled = true,
                        enabled = draft.isTcpOnline,
                    )
                }
            },
        )

        Spacer(modifier = Modifier.height(if (isCompact) 12.dp else 18.dp))

        ProbeFormGridRow(
            gap = columnGap,
            left = {
                ProbeLabeledFieldCell(
                    label = stringResource(R.string.probe_label_slave_name),
                    value = draft.displayName,
                    onValueChange = { onDraftChange(draft.withDisplayName(it)) },
                )
            },
            right = {
                ToggleCell(
                    label = stringResource(R.string.probe_label_slave_screen),
                    checked = draft.slaveScreenOn,
                    onCheckedChange = { onDraftChange(draft.copy(slaveScreenOn = it)) },
                    contentAlignment = Alignment.CenterStart,
                    enabled = draft.isTcpOnline,
                )
            },
        )

        Spacer(modifier = Modifier.height(rowSpacing))

        ProbeFormGridRow(
            gap = columnGap,
            left = {
                ProbeLabeledFieldCell(
                    label = stringResource(R.string.probe_label_slave_location),
                    value = draft.location,
                    onValueChange = { onDraftChange(draft.withLocation(it)) },
                )
            },
            right = {
                ToggleCell(
                    label = stringResource(R.string.probe_label_alarm_light),
                    checked = draft.alarmLightOn,
                    onCheckedChange = { onDraftChange(draft.copy(alarmLightOn = it)) },
                    contentAlignment = Alignment.CenterStart,
                    enabled = draft.isTcpOnline,
                )
            },
        )

        Spacer(modifier = Modifier.height(rowSpacing))

        ProbeThresholdAlarmRow(
            thresholdLabel = stringResource(R.string.probe_threshold_upper),
            value = draft.doseUpperUsv,
            onValueChange = { onDraftChange(draft.copy(doseUpperUsv = it)) },
            alarmOn = draft.radiationUpperAlarmOn,
            onAlarmChange = { onDraftChange(draft.copy(radiationUpperAlarmOn = it)) },
            trailingHalf = null,
        )

        Spacer(modifier = Modifier.height(rowSpacing))

        ProbeThresholdAlarmRow(
            thresholdLabel = stringResource(R.string.probe_threshold_lower),
            value = draft.doseLowerUsv,
            onValueChange = { onDraftChange(draft.copy(doseLowerUsv = it)) },
            alarmOn = draft.radiationLowerAlarmOn,
            onAlarmChange = { onDraftChange(draft.copy(radiationLowerAlarmOn = it)) },
            trailingHalf = null,
        )

        Spacer(modifier = Modifier.height(rowSpacing))

        ProbeFormGridRow(
            gap = columnGap,
            left = {
                VolumeCell(
                    volume = draft.volume,
                    onVolumeChange = { onDraftChange(draft.copy(volume = it)) },
                    onVolumeCommitted = onVolumeCommitted,
                )
            },
            right = { Box(modifier = Modifier.fillMaxWidth()) },
        )

        Spacer(modifier = Modifier.height(rowSpacing))

        ProbeFormGridRow(
            gap = columnGap,
            left = { Box(modifier = Modifier.fillMaxWidth()) },
            right = {
                ProbeFormGridRightSlot {
                    val deleteEnabled = !draft.isTcpOnline
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        IconButton(
                            onClick = onDeleteClick,
                            enabled = deleteEnabled,
                            modifier = Modifier.size(DeleteButtonSize),
                        ) {
                            Icon(
                                imageVector = Icons.Outlined.Delete,
                                contentDescription = stringResource(R.string.cd_delete_probe),
                                tint = if (deleteEnabled) {
                                    RaylinkDoorOpen
                                } else {
                                    RaylinkTextSecondary.copy(alpha = 0.5f)
                                },
                                modifier = Modifier.size(DeleteIconSize),
                            )
                        }
                        Text(
                            text = if (deleteEnabled) {
                                stringResource(R.string.probe_delete)
                            } else {
                                stringResource(R.string.probe_delete_online_blocked)
                            },
                            color = if (deleteEnabled) {
                                RaylinkDoorOpen
                            } else {
                                RaylinkTextSecondary.copy(alpha = 0.5f)
                            },
                            fontSize = if (isCompact) 15.sp else FormActionSp,
                        )
                    }
                }
            },
        )

        Spacer(modifier = Modifier.height(16.dp))
    }
}

/** 一行两格：左 1/2（标签+输入等）、右 1/2（对齐 image19） */
@Composable
private fun ProbeFormGridRow(
    gap: Dp,
    left: @Composable () -> Unit,
    right: @Composable () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(gap),
    ) {
        Box(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth(),
        ) {
            left()
        }
        Box(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth(),
        ) {
            right()
        }
    }
}

/** 右半列内容：在后续 1/2 宽度内水平居中（如数据详情、删除） */
@Composable
private fun ProbeFormGridRightSlot(
    content: @Composable () -> Unit,
) {
    Box(
        modifier = Modifier.fillMaxWidth(),
        contentAlignment = Alignment.Center,
    ) {
        content()
    }
}

@Composable
fun ExternalAlarmBadge(connected: Boolean) {
    Text(
        text = if (connected) {
            stringResource(R.string.probe_ext_alarm_connected)
        } else {
            stringResource(R.string.probe_ext_alarm_disconnected)
        },
        color = if (connected) RaylinkAccentBlue else RaylinkTextSecondary,
        fontSize = FormActionSp,
        modifier = Modifier
            .clip(RoundedCornerShape(6.dp))
            .background(RaylinkTextPrimary.copy(alpha = 0.1f))
            .padding(horizontal = 12.dp, vertical = 6.dp),
    )
}

@Composable
private fun ProbeInfoLabelCell(
    label: String,
    value: String,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        FormLabel(text = label)
        Text(
            text = value.ifBlank { "—" },
            color = RaylinkTextPrimary,
            fontSize = FormFieldSp,
            modifier = Modifier.weight(1f),
            maxLines = 1,
            softWrap = false,
        )
    }
}

@Composable
private fun ProbeLabeledFieldCell(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        FormLabel(text = label)
        ProbeValueField(
            value = value,
            onValueChange = onValueChange,
            modifier = Modifier.weight(1f),
        )
    }
}

/** 报警阈值行：无尾部时左半阈值输入、右半报警开关（与从机屏幕/报警灯光同列对齐）；有尾部时 1/4 + 1/4 + 1/2 */
@Composable
private fun ProbeThresholdAlarmRow(
    thresholdLabel: String,
    value: String,
    onValueChange: (String) -> Unit,
    alarmOn: Boolean,
    onAlarmChange: (Boolean) -> Unit,
    trailingHalf: (@Composable () -> Unit)?,
) {
    val alarmLabel = stringResource(R.string.probe_label_alarm)
    if (trailingHalf == null) {
        ProbeFormGridRow(
            gap = FormColumnGap,
            left = {
                ThresholdValueCell(
                    label = thresholdLabel,
                    value = value,
                    onValueChange = onValueChange,
                )
            },
            right = {
                ToggleCell(
                    label = alarmLabel,
                    checked = alarmOn,
                    onCheckedChange = onAlarmChange,
                    contentAlignment = Alignment.CenterStart,
                )
            },
        )
        return
    }

    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(FormColumnGap),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth(),
        ) {
            ThresholdValueCell(
                label = thresholdLabel,
                value = value,
                onValueChange = onValueChange,
            )
        }
        Box(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .heightIn(min = FormSwitchRowMinHeight),
            contentAlignment = Alignment.CenterStart,
        ) {
            ToggleCell(
                label = alarmLabel,
                checked = alarmOn,
                onCheckedChange = onAlarmChange,
                contentAlignment = Alignment.CenterStart,
            )
        }
        Box(
            modifier = Modifier
                .weight(2f)
                .fillMaxWidth()
                .heightIn(min = FormSwitchRowMinHeight),
            contentAlignment = Alignment.Center,
        ) {
            trailingHalf()
        }
    }
}

@Composable
private fun ThresholdValueCell(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        FormLabel(text = label, width = FormLabelWidth)
        ProbeValueField(
            value = value,
            onValueChange = onValueChange,
            modifier = Modifier
                .weight(1f)
                .padding(end = 6.dp),
        )
        Text(
            text = stringResource(R.string.unit_usv_per_hour),
            color = RaylinkTextSecondary,
            fontSize = FormUnitSp,
            modifier = Modifier.padding(start = 2.dp),
        )
    }
}

@Composable
private fun ToggleCell(
    label: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    contentAlignment: Alignment = Alignment.CenterEnd,
    enabled: Boolean = true,
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = FormSwitchRowMinHeight),
        contentAlignment = contentAlignment,
    ) {
        ProbeToggleRow(
            label = label,
            checked = checked,
            onCheckedChange = onCheckedChange,
            enabled = enabled,
            horizontalArrangement = if (contentAlignment == Alignment.CenterStart) {
                Arrangement.Start
            } else {
                Arrangement.End
            },
            modifier = Modifier.fillMaxWidth(),
        )
    }
}

@Composable
private fun ProbeToggleRow(
    label: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    labelFontSize: TextUnit = FormToggleLabelSp,
    horizontalArrangement: Arrangement.Horizontal = Arrangement.End,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .heightIn(min = FormSwitchRowMinHeight),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = horizontalArrangement,
    ) {
        Text(
            text = label,
            color = if (enabled) RaylinkTextSecondary else RaylinkTextSecondary.copy(alpha = 0.45f),
            fontSize = labelFontSize,
            maxLines = 1,
            softWrap = false,
        )
        Spacer(modifier = Modifier.width(FormSwitchLabelGap))
        ProbeSwitch(checked = checked, onCheckedChange = onCheckedChange, enabled = enabled)
    }
}

@Composable
private fun VolumeCell(
    volume: Float,
    onVolumeChange: (Float) -> Unit,
    onVolumeCommitted: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        FormLabel(text = stringResource(R.string.probe_label_alarm_volume))
        RaylinkSlider(
            value = volume,
            onValueChange = onVolumeChange,
            onValueChangeFinished = { onVolumeCommitted() },
            modifier = Modifier.weight(1f),
        )
    }
}

@Composable
private fun FormLabel(
    text: String,
    width: Dp = FormLabelWidth,
) {
    Text(
        text = text,
        color = RaylinkTextSecondary,
        fontSize = FormLabelSp,
        modifier = Modifier.width(width),
    )
}

@Composable
private fun ProbeValueField(
    value: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    BasicTextField(
        value = value,
        onValueChange = onValueChange,
        textStyle = TextStyle(color = FieldTextColor, fontSize = FormFieldSp),
        cursorBrush = SolidColor(RaylinkAccentBlue),
        modifier = modifier
            .defaultMinSize(minWidth = 96.dp, minHeight = 44.dp)
            .clip(RoundedCornerShape(6.dp))
            .background(Color.White)
            .padding(horizontal = 12.dp, vertical = 10.dp),
        singleLine = true,
    )
}

@Composable
private fun ProbeSwitch(
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    enabled: Boolean = true,
) {
    val switchInteraction = remember { MutableInteractionSource() }
    Box(
        modifier = Modifier
            .defaultMinSize(minWidth = 56.dp, minHeight = FormSwitchRowMinHeight)
            .then(
                if (enabled) {
                    Modifier.clickable(
                        interactionSource = switchInteraction,
                        indication = null,
                        onClick = { onCheckedChange(!checked) },
                    )
                } else {
                    Modifier
                },
            )
            .padding(horizontal = 4.dp),
        contentAlignment = Alignment.Center,
    ) {
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            enabled = enabled,
            modifier = Modifier.graphicsLayer(scaleX = 1.3f, scaleY = 1.3f),
            colors = SwitchDefaults.colors(
                checkedThumbColor = RaylinkTextPrimary,
                checkedTrackColor = RaylinkAccentBlue,
                uncheckedThumbColor = RaylinkTextSecondary,
                uncheckedTrackColor = Color(0xFF4A4A5A),
            ),
        )
    }
}
