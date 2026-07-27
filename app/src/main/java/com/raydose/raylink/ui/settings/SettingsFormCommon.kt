package com.raydose.raylink.ui.settings

import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.BorderStroke
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowDropDown
import androidx.compose.material.icons.outlined.Visibility
import androidx.compose.material.icons.outlined.VisibilityOff
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.HorizontalDivider
import com.raydose.raylink.ui.components.RaylinkSlider
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import com.raydose.raylink.R
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.rememberTabletFormFactor
import com.raydose.raylink.ui.theme.TabletFormFactor
import com.raydose.raylink.ui.theme.RaylinkSettingsEditorPanel
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary

internal val SettingsFormLabelWidth = 168.dp
internal val SettingsFormLabelWidthCompact = 148.dp
internal val SettingsFormRowSpacing = 12.dp
/** 标签右侧编辑区占行宽比例（网络信息等页输入框半宽） */
internal val SettingsFieldAreaFraction = 0.5f
internal val SettingsFieldAreaFractionCompact = 0.62f

@Composable
internal fun settingsFormLabelWidth(): Dp {
    val formFactor = rememberTabletFormFactor()
    return if (formFactor == TabletFormFactor.Compact) {
        SettingsFormLabelWidthCompact
    } else {
        SettingsFormLabelWidth
    }
}

@Composable
internal fun settingsFieldAreaFraction(): Float {
    val formFactor = rememberTabletFormFactor()
    return if (formFactor == TabletFormFactor.Compact) {
        SettingsFieldAreaFractionCompact
    } else {
        SettingsFieldAreaFraction
    }
}
/** 网络表单右侧「保存」列宽，无保存的行也占位以保证输入框对齐 */
private val SettingsSaveSlotWidth = 88.dp
private val FieldTextColor = Color(0xFF1E2433)
internal val SettingsCompactActionMinWidth = 140.dp
internal val SettingsCompactActionMinHeight = 48.dp
/** 下拉弹层：半透明灰色 */
private val SettingsDropdownMenuBg = Color(0xCC505860)
private val SettingsDropdownBorderColor = Color(0x809098A8)

@Composable
internal fun SettingsDropdownControl(
    value: String,
    options: List<String>,
    onSelected: (Int) -> Unit,
    modifier: Modifier = Modifier,
    valueFontSize: TextUnit = 17.sp,
    menuFontSize: TextUnit = 16.sp,
    fillWidth: Boolean = false,
) {
    var expanded by remember { mutableStateOf(false) }
    var triggerWidth by remember { mutableStateOf(SettingsCompactActionMinWidth) }
    val density = LocalDensity.current

    Box(modifier = modifier) {
        Button(
            onClick = { expanded = true },
            modifier = Modifier
                .onSizeChanged { size ->
                    triggerWidth = with(density) {
                        maxOf(size.width.toDp(), SettingsCompactActionMinWidth)
                    }
                }
                .defaultMinSize(
                    minWidth = SettingsCompactActionMinWidth,
                    minHeight = SettingsCompactActionMinHeight,
                )
                .then(if (fillWidth) Modifier.fillMaxWidth() else Modifier),
            colors = ButtonDefaults.buttonColors(containerColor = RaylinkAccentBlue),
            contentPadding = PaddingValues(horizontal = 10.dp, vertical = 8.dp),
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.Center,
            ) {
                Text(
                    text = value,
                    color = RaylinkTextPrimary,
                    fontSize = valueFontSize,
                    maxLines = 1,
                    softWrap = false,
                    textAlign = TextAlign.Center,
                )
                Icon(
                    imageVector = Icons.Filled.ArrowDropDown,
                    contentDescription = null,
                    tint = RaylinkTextPrimary,
                    modifier = Modifier
                        .size(22.dp)
                        .graphicsLayer(rotationZ = if (expanded) 180f else 0f),
                )
            }
        }
        DropdownMenu(
            expanded = expanded,
            onDismissRequest = { expanded = false },
            modifier = Modifier.width(triggerWidth),
            shape = RoundedCornerShape(6.dp),
            containerColor = SettingsDropdownMenuBg,
            tonalElevation = 0.dp,
            shadowElevation = 2.dp,
            border = BorderStroke(1.dp, SettingsDropdownBorderColor),
        ) {
            options.forEachIndexed { index, option ->
                Box(
                    modifier = Modifier
                        .width(triggerWidth)
                        .height(SettingsCompactActionMinHeight)
                        .clickable {
                            expanded = false
                            onSelected(index)
                        },
                    contentAlignment = Alignment.Center,
                ) {
                    Text(
                        text = option,
                        color = RaylinkTextPrimary,
                        fontSize = menuFontSize,
                        textAlign = TextAlign.Center,
                        maxLines = 1,
                        softWrap = false,
                    )
                }
                if (index < options.lastIndex) {
                    HorizontalDivider(
                        color = SettingsDropdownBorderColor,
                        thickness = 0.5.dp,
                    )
                }
            }
        }
    }
}

@Composable
private fun SettingsNetworkFieldRow(
    label: String,
    fieldAreaFraction: Float,
    showSave: Boolean,
    onSaveClick: (() -> Unit)?,
    field: @Composable BoxScope.(Modifier) -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(label)
        Box(modifier = Modifier.fillMaxWidth(fieldAreaFraction)) {
            field(Modifier.fillMaxWidth())
        }
        Box(
            modifier = Modifier.width(SettingsSaveSlotWidth),
            contentAlignment = Alignment.CenterEnd,
        ) {
            if (showSave && onSaveClick != null) {
                SettingsInlineSaveButton(onClick = onSaveClick)
            }
        }
    }
}

@Composable
fun SettingsScrollContent(
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 28.dp, vertical = 16.dp),
        verticalArrangement = Arrangement.spacedBy(SettingsFormRowSpacing),
    ) {
        content()
    }
}

@Composable
internal fun SettingsInlineSaveButton(
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Button(
        onClick = onClick,
        modifier = modifier,
        colors = ButtonDefaults.buttonColors(containerColor = RaylinkAccentBlue),
    ) {
        Text(stringResource(R.string.action_save), fontSize = 16.sp, color = RaylinkTextPrimary)
    }
}

@Composable
internal fun SettingsInlineActionButton(
    text: String,
    onClick: () -> Unit,
    filled: Boolean = false,
    enabled: Boolean = true,
    modifier: Modifier = Modifier,
) {
    if (filled) {
        Button(
            onClick = onClick,
            modifier = modifier,
            enabled = enabled,
            colors = ButtonDefaults.buttonColors(containerColor = RaylinkAccentBlue),
        ) {
            Text(text, fontSize = 16.sp, color = RaylinkTextPrimary)
        }
    } else {
        androidx.compose.material3.OutlinedButton(
            onClick = onClick,
            modifier = modifier,
            enabled = enabled,
        ) {
            Text(text, fontSize = 16.sp, color = RaylinkTextPrimary)
        }
    }
}

/** 半宽输入 + 右侧操作按钮（时间设置：设置 / 同步到设备） */
@Composable
internal fun SettingsTextFieldHalfRowWithActions(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    fieldAreaFraction: Float? = null,
    actions: @Composable RowScope.() -> Unit,
) {
    val fraction = fieldAreaFraction ?: settingsFieldAreaFraction()
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        SettingsFormLabel(label)
        SettingsValueField(
            value = value,
            onValueChange = onValueChange,
            modifier = Modifier.fillMaxWidth(fraction),
        )
        actions()
    }
}

@Composable
fun SettingsPanelScaffold(
    modifier: Modifier = Modifier,
    onSaveClick: () -> Unit,
    showSaveButton: Boolean = true,
    saveEnabled: Boolean = true,
    extraActions: @Composable () -> Unit = {},
    content: @Composable () -> Unit,
) {
    Column(modifier = modifier.fillMaxSize()) {
        Column(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 28.dp, vertical = 16.dp),
            verticalArrangement = Arrangement.spacedBy(SettingsFormRowSpacing),
        ) {
            content()
        }
        if (showSaveButton) {
            HorizontalDivider(color = RaylinkTextPrimary.copy(alpha = 0.15f))
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 28.dp, vertical = 14.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                extraActions()
                Button(
                    onClick = onSaveClick,
                    enabled = saveEnabled,
                    colors = ButtonDefaults.buttonColors(containerColor = RaylinkAccentBlue),
                ) {
                    Text(stringResource(R.string.action_save), fontSize = 17.sp, color = RaylinkTextPrimary)
                }
            }
        }
    }
}

@Composable
internal fun SettingsSectionTitle(text: String) {
    Text(
        text = text,
        color = RaylinkTextPrimary,
        fontSize = 20.sp,
        fontWeight = FontWeight.SemiBold,
        modifier = Modifier.padding(top = 8.dp, bottom = 4.dp),
    )
}

@Composable
internal fun SettingsSectionHeaderRow(
    title: String,
    titleFontSize: TextUnit = 20.sp,
    onActionClick: (() -> Unit)? = null,
    actionText: String? = null,
    actionFontSize: TextUnit = 17.sp,
) {
    val resolvedActionText = actionText ?: stringResource(R.string.action_edit)
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 8.dp, bottom = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = title,
            color = RaylinkTextPrimary,
            fontSize = titleFontSize,
            fontWeight = FontWeight.SemiBold,
        )
        if (onActionClick != null) {
            SettingsInlineActionButton(
                text = resolvedActionText,
                onClick = onActionClick,
                filled = true,
                modifier = Modifier.defaultMinSize(minWidth = 88.dp, minHeight = 44.dp),
            )
        }
    }
}

@Composable
internal fun SettingsDropdownRow(
    label: String,
    value: String,
    options: List<String>,
    labelFontSize: TextUnit = 17.sp,
    valueFontSize: TextUnit = 17.sp,
    menuFontSize: TextUnit = 16.sp,
    showDropdownHint: Boolean = true,
    labelWidth: Dp = SettingsFormLabelWidth,
    labelSingleLine: Boolean = false,
    onSelected: (Int) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(
            text = label,
            width = labelWidth,
            fontSize = labelFontSize,
            maxLines = if (labelSingleLine) 1 else Int.MAX_VALUE,
            softWrap = !labelSingleLine,
        )
        if (showDropdownHint) {
            SettingsDropdownControl(
                value = value,
                options = options,
                onSelected = onSelected,
                valueFontSize = valueFontSize,
                menuFontSize = menuFontSize,
            )
        } else {
            Box {
                TextButton(onClick = { expanded = true }) {
                    Text(value, color = RaylinkTextPrimary, fontSize = valueFontSize)
                }
                DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                    options.forEachIndexed { index, option ->
                        DropdownMenuItem(
                            text = { Text(option, fontSize = menuFontSize) },
                            onClick = {
                                expanded = false
                                onSelected(index)
                            },
                        )
                    }
                }
            }
        }
    }
}

/** 百分比文字预留宽度 */
private val SettingsSliderPercentReserve = 48.dp

/**
 * @param sliderEndFraction 滑条右端在整行中的横向位置；0.5f = 行宽正中（显示与声音页）
 */
@Composable
internal fun SettingsSliderRow(
    label: String,
    value: Float,
    sliderEndFraction: Float = 1f,
    onValueChange: (Float) -> Unit,
    onValueChangeFinished: (() -> Unit)? = null,
    labelFontSize: TextUnit = 17.sp,
    percentFontSize: TextUnit = 15.sp,
    labelWidth: Dp = SettingsFormLabelWidth,
    labelSingleLine: Boolean = false,
) {
    val endFraction = sliderEndFraction.coerceIn(0.35f, 1f)
    BoxWithConstraints(modifier = Modifier.fillMaxWidth()) {
        val sliderTrackWidth = (maxWidth * endFraction - labelWidth - SettingsSliderPercentReserve)
            .coerceAtLeast(120.dp)
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            SettingsFormLabel(
                text = label,
                width = labelWidth,
                fontSize = labelFontSize,
                maxLines = if (labelSingleLine) 1 else Int.MAX_VALUE,
                softWrap = !labelSingleLine,
            )
            RaylinkSlider(
                value = value,
                onValueChange = onValueChange,
                onValueChangeFinished = onValueChangeFinished,
                modifier = Modifier.width(sliderTrackWidth),
            )
            Text(
                text = "${(value * 100).toInt()}%",
                color = RaylinkTextSecondary,
                fontSize = percentFontSize,
                modifier = Modifier.padding(start = 8.dp),
            )
            if (endFraction < 1f) {
                Spacer(modifier = Modifier.weight(1f))
            }
        }
    }
}

@Composable
internal fun SettingsSwitchRow(
    label: String,
    checked: Boolean,
    labelFontSize: TextUnit = 17.sp,
    enlargedSwitch: Boolean = false,
    labelWidth: Dp = SettingsFormLabelWidth,
    labelSingleLine: Boolean = false,
    onCheckedChange: (Boolean) -> Unit,
) {
    val switchInteraction = remember { MutableInteractionSource() }
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(
            text = label,
            width = labelWidth,
            fontSize = labelFontSize,
            maxLines = if (labelSingleLine) 1 else Int.MAX_VALUE,
            softWrap = !labelSingleLine,
        )
        Box(
            modifier = if (enlargedSwitch) {
                Modifier
                    .defaultMinSize(minWidth = 80.dp, minHeight = 52.dp)
                    .clickable(
                        interactionSource = switchInteraction,
                        indication = null,
                        onClick = { onCheckedChange(!checked) },
                    )
            } else {
                Modifier
            },
            contentAlignment = Alignment.CenterStart,
        ) {
            Switch(
                checked = checked,
                onCheckedChange = onCheckedChange,
                modifier = if (enlargedSwitch) {
                    Modifier.graphicsLayer(scaleX = 1.35f, scaleY = 1.35f)
                } else {
                    Modifier
                },
                colors = SwitchDefaults.colors(
                    checkedThumbColor = RaylinkTextPrimary,
                    checkedTrackColor = RaylinkAccentBlue,
                    uncheckedThumbColor = RaylinkTextSecondary,
                    uncheckedTrackColor = Color(0xFF4A4A5A),
                ),
            )
        }
    }
}

@Composable
internal fun SettingsLabeledButtonRow(
    label: String,
    buttonText: String,
    onClick: () -> Unit,
    labelFontSize: TextUnit = 17.sp,
    buttonFontSize: TextUnit = 17.sp,
    enabled: Boolean = true,
    labelWidth: Dp = SettingsFormLabelWidth,
    labelSingleLine: Boolean = false,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(
            text = label,
            width = labelWidth,
            fontSize = labelFontSize,
            maxLines = if (labelSingleLine) 1 else Int.MAX_VALUE,
            softWrap = !labelSingleLine,
        )
        Button(
            onClick = onClick,
            enabled = enabled,
            modifier = Modifier.defaultMinSize(
                minWidth = SettingsCompactActionMinWidth,
                minHeight = SettingsCompactActionMinHeight,
            ),
            colors = ButtonDefaults.buttonColors(containerColor = RaylinkAccentBlue),
        ) {
            Text(buttonText, fontSize = buttonFontSize, color = RaylinkTextPrimary)
        }
    }
}

@Composable
internal fun SettingsTextFieldRow(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    readOnly: Boolean = false,
    labelWidth: Dp = SettingsFormLabelWidth,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(label, width = labelWidth)
        if (readOnly) {
            Text(value.ifBlank { "—" }, color = RaylinkTextPrimary, fontSize = 17.sp)
        } else {
            SettingsValueField(value = value, onValueChange = onValueChange, modifier = Modifier.weight(1f))
        }
    }
}

@Composable
internal fun SettingsReadOnlyHalfRow(
    label: String,
    value: String,
    fieldAreaFraction: Float? = null,
    alignSaveColumn: Boolean = false,
    labelFontSize: TextUnit = 17.sp,
    valueFontSize: TextUnit = 17.sp,
    labelWidth: Dp? = null,
    labelSingleLine: Boolean = false,
) {
    val fraction = fieldAreaFraction ?: settingsFieldAreaFraction()
    val labelW = labelWidth ?: settingsFormLabelWidth()
    if (!alignSaveColumn) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            SettingsFormLabel(
                text = label,
                width = labelW,
                fontSize = labelFontSize,
                maxLines = if (labelSingleLine) 1 else Int.MAX_VALUE,
                softWrap = !labelSingleLine,
            )
            Text(
                text = value.ifBlank { "—" },
                color = RaylinkTextPrimary,
                fontSize = valueFontSize,
                modifier = Modifier.fillMaxWidth(fraction),
            )
        }
        return
    }
    SettingsNetworkFieldRow(
        label = label,
        fieldAreaFraction = fraction,
        showSave = false,
        onSaveClick = null,
    ) { mod ->
        Text(
            text = value.ifBlank { "—" },
            color = RaylinkTextPrimary,
            fontSize = valueFontSize,
            modifier = mod.padding(vertical = 9.dp),
        )
    }
}

@Composable
internal fun SettingsTextFieldHalfRow(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    fieldAreaFraction: Float? = null,
    alignSaveColumn: Boolean = false,
    isPassword: Boolean = false,
) {
    val fraction = fieldAreaFraction ?: settingsFieldAreaFraction()
    if (!alignSaveColumn) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            SettingsFormLabel(label, width = settingsFormLabelWidth())
            SettingsValueField(
                value = value,
                onValueChange = onValueChange,
                isPassword = isPassword,
                modifier = Modifier.fillMaxWidth(fraction),
            )
        }
        return
    }
    SettingsNetworkFieldRow(
        label = label,
        fieldAreaFraction = fraction,
        showSave = false,
        onSaveClick = null,
    ) { mod ->
        SettingsValueField(
            value = value,
            onValueChange = onValueChange,
            isPassword = isPassword,
            modifier = mod,
        )
    }
}

/** 半宽输入 + 右侧「保存」（用于每个网络单元最后一行） */
@Composable
internal fun SettingsTextFieldHalfRowWithSave(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    onSaveClick: () -> Unit,
    fieldAreaFraction: Float? = null,
    alignSaveColumn: Boolean = false,
    isPassword: Boolean = false,
) {
    val fraction = fieldAreaFraction ?: settingsFieldAreaFraction()
    if (!alignSaveColumn) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            SettingsFormLabel(label, width = settingsFormLabelWidth())
            SettingsValueField(
                value = value,
                onValueChange = onValueChange,
                isPassword = isPassword,
                modifier = Modifier.fillMaxWidth(fraction),
            )
            Spacer(modifier = Modifier.width(12.dp))
            SettingsInlineSaveButton(onClick = onSaveClick)
        }
        return
    }
    SettingsNetworkFieldRow(
        label = label,
        fieldAreaFraction = fraction,
        showSave = true,
        onSaveClick = onSaveClick,
    ) { mod ->
        SettingsValueField(
            value = value,
            onValueChange = onValueChange,
            isPassword = isPassword,
            modifier = mod,
        )
    }
}

@Composable
internal fun SettingsDropdownHalfRow(
    label: String,
    value: String,
    options: List<String>,
    onSelected: (Int) -> Unit,
    fieldAreaFraction: Float? = null,
    alignSaveColumn: Boolean = false,
) {
    val fraction = fieldAreaFraction ?: settingsFieldAreaFraction()
    val dropdown: @Composable BoxScope.(Modifier) -> Unit = { mod ->
        SettingsDropdownControl(
            modifier = mod,
            value = value,
            options = options,
            onSelected = onSelected,
        )
    }
    if (!alignSaveColumn) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            SettingsFormLabel(label, width = settingsFormLabelWidth())
            Box(modifier = Modifier.fillMaxWidth(fraction)) {
                dropdown(Modifier)
            }
        }
        return
    }
    SettingsNetworkFieldRow(
        label = label,
        fieldAreaFraction = fraction,
        showSave = false,
        onSaveClick = null,
        field = dropdown,
    )
}

@Composable
internal fun SettingsFormLabel(
    text: String,
    width: Dp = SettingsFormLabelWidth,
    fontSize: TextUnit = 17.sp,
    maxLines: Int = Int.MAX_VALUE,
    softWrap: Boolean = true,
    modifier: Modifier = Modifier,
) {
    Text(
        text = text,
        color = RaylinkTextSecondary,
        fontSize = fontSize,
        maxLines = maxLines,
        softWrap = softWrap,
        modifier = modifier
            .width(width)
            .padding(end = 12.dp),
    )
}

@Composable
internal fun SettingsValueField(
    value: String,
    onValueChange: (String) -> Unit,
    modifier: Modifier = Modifier,
    isPassword: Boolean = false,
) {
    var passwordVisible by remember { mutableStateOf(false) }
    Row(
        modifier = modifier
            .defaultMinSize(minWidth = 96.dp, minHeight = 44.dp)
            .clip(RoundedCornerShape(6.dp))
            .background(Color.White),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        BasicTextField(
            value = value,
            onValueChange = onValueChange,
            textStyle = TextStyle(color = FieldTextColor, fontSize = 17.sp),
            cursorBrush = SolidColor(RaylinkAccentBlue),
            visualTransformation = if (isPassword && !passwordVisible) {
                PasswordVisualTransformation()
            } else {
                VisualTransformation.None
            },
            modifier = Modifier
                .weight(1f)
                .padding(horizontal = 12.dp, vertical = 10.dp),
            singleLine = true,
        )
        if (isPassword) {
            IconButton(
                onClick = { passwordVisible = !passwordVisible },
                modifier = Modifier.size(40.dp),
            ) {
                Icon(
                    imageVector = if (passwordVisible) Icons.Outlined.VisibilityOff else Icons.Outlined.Visibility,
                    contentDescription = if (passwordVisible) {
                        stringResource(R.string.cd_hide_password)
                    } else {
                        stringResource(R.string.cd_show_password)
                    },
                    tint = RaylinkAccentBlue,
                    modifier = Modifier.size(22.dp),
                )
            }
        }
    }
}

@Composable
internal fun SettingsCard(
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(10.dp))
            .background(RaylinkSettingsEditorPanel)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        content()
    }
}
