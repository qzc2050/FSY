package com.raydose.netshield.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
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
import com.raydose.netshield.ui.components.NetShieldSlider
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
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldSettingsEditorPanel
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

internal val SettingsFormLabelWidth = 140.dp
internal val SettingsFormRowSpacing = 12.dp
/** 标签右侧编辑区占行宽比例（网络信息等页输入框半宽） */
internal val SettingsFieldAreaFraction = 0.5f
/** 网络表单右侧「保存」列宽，无保存的行也占位以保证输入框对齐 */
private val SettingsSaveSlotWidth = 88.dp
private val FieldTextColor = Color(0xFF1E2433)

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
        colors = ButtonDefaults.buttonColors(containerColor = NetShieldAccentBlue),
    ) {
        Text("保存", fontSize = 16.sp, color = NetShieldTextPrimary)
    }
}

@Composable
internal fun SettingsInlineActionButton(
    text: String,
    onClick: () -> Unit,
    filled: Boolean = false,
    modifier: Modifier = Modifier,
) {
    if (filled) {
        Button(
            onClick = onClick,
            modifier = modifier,
            colors = ButtonDefaults.buttonColors(containerColor = NetShieldAccentBlue),
        ) {
            Text(text, fontSize = 16.sp, color = NetShieldTextPrimary)
        }
    } else {
        androidx.compose.material3.OutlinedButton(onClick = onClick, modifier = modifier) {
            Text(text, fontSize = 16.sp, color = NetShieldTextPrimary)
        }
    }
}

/** 半宽输入 + 右侧操作按钮（时间设置：设置 / 同步到设备） */
@Composable
internal fun SettingsTextFieldHalfRowWithActions(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    fieldAreaFraction: Float = SettingsFieldAreaFraction,
    actions: @Composable RowScope.() -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        SettingsFormLabel(label)
        SettingsValueField(
            value = value,
            onValueChange = onValueChange,
            modifier = Modifier.fillMaxWidth(fieldAreaFraction),
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
            HorizontalDivider(color = NetShieldTextPrimary.copy(alpha = 0.15f))
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
                    colors = ButtonDefaults.buttonColors(containerColor = NetShieldAccentBlue),
                ) {
                    Text("保存", fontSize = 17.sp, color = NetShieldTextPrimary)
                }
            }
        }
    }
}

@Composable
internal fun SettingsSectionTitle(text: String) {
    Text(
        text = text,
        color = NetShieldTextPrimary,
        fontSize = 20.sp,
        fontWeight = FontWeight.SemiBold,
        modifier = Modifier.padding(top = 8.dp, bottom = 4.dp),
    )
}

@Composable
internal fun SettingsDropdownRow(
    label: String,
    value: String,
    options: List<String>,
    onSelected: (Int) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(label)
        Box {
            TextButton(onClick = { expanded = true }) {
                Text(value, color = NetShieldTextPrimary, fontSize = 17.sp)
            }
            DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                options.forEachIndexed { index, option ->
                    DropdownMenuItem(
                        text = { Text(option, fontSize = 16.sp) },
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
) {
    val endFraction = sliderEndFraction.coerceIn(0.35f, 1f)
    BoxWithConstraints(modifier = Modifier.fillMaxWidth()) {
        val sliderTrackWidth = (maxWidth * endFraction - SettingsFormLabelWidth - SettingsSliderPercentReserve)
            .coerceAtLeast(120.dp)
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            SettingsFormLabel(label)
            NetShieldSlider(
                value = value,
                onValueChange = onValueChange,
                onValueChangeFinished = onValueChangeFinished,
                modifier = Modifier.width(sliderTrackWidth),
            )
            Text(
                text = "${(value * 100).toInt()}%",
                color = NetShieldTextSecondary,
                fontSize = 15.sp,
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
    onCheckedChange: (Boolean) -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        SettingsFormLabel(label)
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            colors = SwitchDefaults.colors(
                checkedThumbColor = NetShieldTextPrimary,
                checkedTrackColor = NetShieldAccentBlue,
                uncheckedThumbColor = NetShieldTextSecondary,
                uncheckedTrackColor = Color(0xFF4A4A5A),
            ),
        )
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
            Text(value.ifBlank { "—" }, color = NetShieldTextPrimary, fontSize = 17.sp)
        } else {
            SettingsValueField(value = value, onValueChange = onValueChange, modifier = Modifier.weight(1f))
        }
    }
}

@Composable
internal fun SettingsReadOnlyHalfRow(
    label: String,
    value: String,
    fieldAreaFraction: Float = SettingsFieldAreaFraction,
    alignSaveColumn: Boolean = false,
) {
    if (!alignSaveColumn) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            SettingsFormLabel(label)
            Text(
                text = value.ifBlank { "—" },
                color = NetShieldTextPrimary,
                fontSize = 17.sp,
                modifier = Modifier.fillMaxWidth(fieldAreaFraction),
            )
        }
        return
    }
    SettingsNetworkFieldRow(
        label = label,
        fieldAreaFraction = fieldAreaFraction,
        showSave = false,
        onSaveClick = null,
    ) { mod ->
        Text(
            text = value.ifBlank { "—" },
            color = NetShieldTextPrimary,
            fontSize = 17.sp,
            modifier = mod.padding(vertical = 9.dp),
        )
    }
}

@Composable
internal fun SettingsTextFieldHalfRow(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    fieldAreaFraction: Float = SettingsFieldAreaFraction,
    alignSaveColumn: Boolean = false,
    isPassword: Boolean = false,
) {
    if (!alignSaveColumn) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            SettingsFormLabel(label)
            SettingsValueField(
                value = value,
                onValueChange = onValueChange,
                isPassword = isPassword,
                modifier = Modifier.fillMaxWidth(fieldAreaFraction),
            )
        }
        return
    }
    SettingsNetworkFieldRow(
        label = label,
        fieldAreaFraction = fieldAreaFraction,
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
    fieldAreaFraction: Float = SettingsFieldAreaFraction,
    alignSaveColumn: Boolean = false,
    isPassword: Boolean = false,
) {
    if (!alignSaveColumn) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            SettingsFormLabel(label)
            SettingsValueField(
                value = value,
                onValueChange = onValueChange,
                isPassword = isPassword,
                modifier = Modifier.fillMaxWidth(fieldAreaFraction),
            )
            Spacer(modifier = Modifier.width(12.dp))
            SettingsInlineSaveButton(onClick = onSaveClick)
        }
        return
    }
    SettingsNetworkFieldRow(
        label = label,
        fieldAreaFraction = fieldAreaFraction,
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
    fieldAreaFraction: Float = SettingsFieldAreaFraction,
    alignSaveColumn: Boolean = false,
) {
    var expanded by remember { mutableStateOf(false) }
    val dropdown: @Composable BoxScope.(Modifier) -> Unit = { mod ->
        Box(modifier = mod) {
            TextButton(onClick = { expanded = true }) {
                Text(value, color = NetShieldTextPrimary, fontSize = 17.sp)
            }
            DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                options.forEachIndexed { index, option ->
                    DropdownMenuItem(
                        text = { Text(option, fontSize = 16.sp) },
                        onClick = {
                            expanded = false
                            onSelected(index)
                        },
                    )
                }
            }
        }
    }
    if (!alignSaveColumn) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            SettingsFormLabel(label)
            Box(modifier = Modifier.fillMaxWidth(fieldAreaFraction)) {
                dropdown(Modifier)
            }
        }
        return
    }
    SettingsNetworkFieldRow(
        label = label,
        fieldAreaFraction = fieldAreaFraction,
        showSave = false,
        onSaveClick = null,
        field = dropdown,
    )
}

@Composable
internal fun SettingsFormLabel(text: String, width: Dp = SettingsFormLabelWidth) {
    Text(
        text = text,
        color = NetShieldTextSecondary,
        fontSize = 17.sp,
        modifier = Modifier
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
            .clip(RoundedCornerShape(6.dp))
            .background(Color.White),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        BasicTextField(
            value = value,
            onValueChange = onValueChange,
            textStyle = TextStyle(color = FieldTextColor, fontSize = 17.sp),
            cursorBrush = SolidColor(NetShieldAccentBlue),
            visualTransformation = if (isPassword && !passwordVisible) {
                PasswordVisualTransformation()
            } else {
                VisualTransformation.None
            },
            modifier = Modifier
                .weight(1f)
                .padding(horizontal = 12.dp, vertical = 9.dp),
            singleLine = true,
        )
        if (isPassword) {
            IconButton(
                onClick = { passwordVisible = !passwordVisible },
                modifier = Modifier.size(40.dp),
            ) {
                Icon(
                    imageVector = if (passwordVisible) Icons.Outlined.VisibilityOff else Icons.Outlined.Visibility,
                    contentDescription = if (passwordVisible) "隐藏密码" else "显示密码",
                    tint = NetShieldAccentBlue,
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
            .background(NetShieldSettingsEditorPanel)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        content()
    }
}
