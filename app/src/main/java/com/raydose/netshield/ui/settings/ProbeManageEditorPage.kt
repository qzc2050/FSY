package com.raydose.netshield.ui.settings

import androidx.compose.foundation.clickable
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
import com.raydose.netshield.ui.components.NetShieldSlider
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.ProbeManageDraft
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldDoorOpen
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

private val FormLabelWidth = 108.dp
private val FormRowSpacing = 14.dp
private val FormColumnGap = 28.dp
private val FieldTextColor = Color(0xFF1E2433)

private val FormLabelSp = 20.sp
private val FormFieldSp = 19.sp
private val FormDataDetailSp = 22.sp
private val FormActionSp = 18.sp
private val FormThresholdLabelSp = 18.sp
private val FormUnitSp = 16.sp
private val FormAlarmSp = 18.sp
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
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(horizontal = 28.dp, vertical = 12.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onDataDetailClick) {
                Text(
                    text = "数据详情",
                    color = NetShieldAccentBlue,
                    fontSize = FormDataDetailSp,
                )
            }
            ExternalAlarmBadge(connected = draft.externalAlarmConnected)
        }

        Spacer(modifier = Modifier.height(14.dp))

        ProbeFormGridRow(
            gap = FormColumnGap,
            left = {
                ProbeInfoLabelCell(label = "型号", value = draft.savedProbe.model)
            },
            right = {
                ProbeInfoLabelCell(label = "序列号", value = draft.savedProbe.serial)
            },
        )

        Spacer(modifier = Modifier.height(FormRowSpacing))

        ProbeFormGridRow(
            gap = FormColumnGap,
            left = {
                ProbeInfoLabelCell(label = "IP 地址", value = draft.ip)
            },
            right = {
                ProbeInfoLabelCell(label = "设备 ID", value = draft.protoAddr)
            },
        )

        Spacer(modifier = Modifier.height(18.dp))

        ProbeFormGridRow(
            gap = FormColumnGap,
            left = {
                ProbeLabeledFieldCell(
                    label = "从机名称",
                    value = draft.displayName,
                    onValueChange = { onDraftChange(draft.withDisplayName(it)) },
                )
            },
            right = {
                ToggleCell(
                    label = "从机屏幕",
                    checked = draft.slaveScreenOn,
                    onCheckedChange = { onDraftChange(draft.copy(slaveScreenOn = it)) },
                    contentAlignment = Alignment.CenterStart,
                )
            },
        )

        Spacer(modifier = Modifier.height(FormRowSpacing))

        ProbeFormGridRow(
            gap = FormColumnGap,
            left = {
                ProbeLabeledFieldCell(
                    label = "从机位置",
                    value = draft.location,
                    onValueChange = { onDraftChange(draft.withLocation(it)) },
                )
            },
            right = {
                ToggleCell(
                    label = "报警灯光",
                    checked = draft.alarmLightOn,
                    onCheckedChange = { onDraftChange(draft.copy(alarmLightOn = it)) },
                    contentAlignment = Alignment.CenterStart,
                )
            },
        )

        Spacer(modifier = Modifier.height(FormRowSpacing))

        ProbeThresholdAlarmRow(
            thresholdLabel = "报警上限值",
            value = draft.doseUpperUsv,
            onValueChange = { onDraftChange(draft.copy(doseUpperUsv = it)) },
            alarmOn = draft.radiationUpperAlarmOn,
            onAlarmChange = { onDraftChange(draft.copy(radiationUpperAlarmOn = it)) },
            trailingHalf = null,
        )

        Spacer(modifier = Modifier.height(FormRowSpacing))

        ProbeThresholdAlarmRow(
            thresholdLabel = "报警下限值",
            value = draft.doseLowerUsv,
            onValueChange = { onDraftChange(draft.copy(doseLowerUsv = it)) },
            alarmOn = draft.radiationLowerAlarmOn,
            onAlarmChange = { onDraftChange(draft.copy(radiationLowerAlarmOn = it)) },
            trailingHalf = null,
        )

        Spacer(modifier = Modifier.height(FormRowSpacing))

        ProbeFormGridRow(
            gap = FormColumnGap,
            left = {
                VolumeCell(
                    volume = draft.volume,
                    onVolumeChange = { onDraftChange(draft.copy(volume = it)) },
                    onVolumeCommitted = onVolumeCommitted,
                )
            },
            right = { Box(modifier = Modifier.fillMaxWidth()) },
        )

        Spacer(modifier = Modifier.height(FormRowSpacing))

        ProbeFormGridRow(
            gap = FormColumnGap,
            left = { Box(modifier = Modifier.fillMaxWidth()) },
            right = {
                ProbeFormGridRightSlot {
                    val deleteEnabled = !draft.isTcpOnline
                    IconButton(
                        onClick = onDeleteClick,
                        enabled = deleteEnabled,
                        modifier = Modifier.size(DeleteButtonSize),
                    ) {
                        Icon(
                            imageVector = Icons.Outlined.Delete,
                            contentDescription = "删除探头",
                            tint = if (deleteEnabled) {
                                NetShieldDoorOpen
                            } else {
                                NetShieldTextSecondary.copy(alpha = 0.35f)
                            },
                            modifier = Modifier.size(DeleteIconSize),
                        )
                    }
                }
            },
        )

        Spacer(modifier = Modifier.weight(1f))
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
        text = if (connected) "外置报警 已连接" else "外置报警 未连接",
        color = if (connected) NetShieldAccentBlue else NetShieldTextSecondary,
        fontSize = FormActionSp,
        modifier = Modifier
            .clip(RoundedCornerShape(6.dp))
            .background(NetShieldTextPrimary.copy(alpha = 0.1f))
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
            color = NetShieldTextPrimary,
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

/** 报警阈值行：1/4 数值+单位，1/4 报警开关，1/2 尾部（上限行放报警灯光并居中） */
@Composable
private fun ProbeThresholdAlarmRow(
    thresholdLabel: String,
    value: String,
    onValueChange: (String) -> Unit,
    alarmOn: Boolean,
    onAlarmChange: (Boolean) -> Unit,
    trailingHalf: (@Composable () -> Unit)?,
) {
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
            ThresholdValueQuarterCell(
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
            contentAlignment = Alignment.Center,
        ) {
            AlarmToggleQuarterCell(
                checked = alarmOn,
                onCheckedChange = onAlarmChange,
            )
        }
        Box(
            modifier = Modifier
                .weight(2f)
                .fillMaxWidth()
                .heightIn(min = FormSwitchRowMinHeight),
            contentAlignment = Alignment.Center,
        ) {
            trailingHalf?.invoke()
        }
    }
}

@Composable
private fun ThresholdValueQuarterCell(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = label,
            color = NetShieldTextSecondary,
            fontSize = FormThresholdLabelSp,
            modifier = Modifier.width(96.dp),
            maxLines = 2,
        )
        ProbeValueField(
            value = value,
            onValueChange = onValueChange,
            modifier = Modifier
                .weight(1f)
                .padding(horizontal = 4.dp),
        )
        Text(
            text = "μSv/h",
            color = NetShieldTextSecondary,
            fontSize = FormUnitSp,
            modifier = Modifier.padding(start = 2.dp),
        )
    }
}

@Composable
private fun AlarmToggleQuarterCell(
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
) {
    ProbeToggleRow(
        label = "报警",
        labelFontSize = FormAlarmSp,
        checked = checked,
        onCheckedChange = onCheckedChange,
        horizontalArrangement = Arrangement.Center,
    )
}

@Composable
private fun ToggleCell(
    label: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    contentAlignment: Alignment = Alignment.CenterEnd,
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
            color = NetShieldTextSecondary,
            fontSize = labelFontSize,
            maxLines = 1,
            softWrap = false,
        )
        Spacer(modifier = Modifier.width(FormSwitchLabelGap))
        ProbeSwitch(checked = checked, onCheckedChange = onCheckedChange)
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
        FormLabel(text = "报警音量")
        NetShieldSlider(
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
        color = NetShieldTextSecondary,
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
        cursorBrush = SolidColor(NetShieldAccentBlue),
        modifier = modifier
            .clip(RoundedCornerShape(6.dp))
            .background(Color.White)
            .padding(horizontal = 12.dp, vertical = 9.dp),
        singleLine = true,
    )
}

@Composable
private fun ProbeSwitch(
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
) {
    val switchInteraction = remember { MutableInteractionSource() }
    Box(
        modifier = Modifier
            .defaultMinSize(minWidth = 56.dp, minHeight = FormSwitchRowMinHeight)
            .clickable(
                interactionSource = switchInteraction,
                indication = null,
                onClick = { onCheckedChange(!checked) },
            )
            .padding(horizontal = 4.dp),
        contentAlignment = Alignment.Center,
    ) {
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            modifier = Modifier.graphicsLayer(scaleX = 1.3f, scaleY = 1.3f),
            colors = SwitchDefaults.colors(
                checkedThumbColor = NetShieldTextPrimary,
                checkedTrackColor = NetShieldAccentBlue,
                uncheckedThumbColor = NetShieldTextSecondary,
                uncheckedTrackColor = Color(0xFF4A4A5A),
            ),
        )
    }
}
