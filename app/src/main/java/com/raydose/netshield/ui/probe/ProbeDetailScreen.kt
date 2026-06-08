package com.raydose.netshield.ui.probe

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Edit
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.window.DialogProperties
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.TextRange
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.data.FileManagerRepository
import com.raydose.netshield.model.DailyDoseSummary
import com.raydose.netshield.model.FileStorageLocation
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.ui.components.CompactRadiationHeader
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldAtmosphereBackgroundBrush
import com.raydose.netshield.ui.theme.NetShieldAtmospherePlayerOverlay
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec

@Composable
fun ProbeDetailScreen(
    probes: List<SlaveProbeUi>,
    initialProbeId: String?,
    organizationName: String,
    onSaveOrganizationName: (String) -> Unit,
    onSaveIdentity: (probeId: String, displayName: String, location: String) -> Unit,
    fileManagerRepository: FileManagerRepository,
    usbGrantEpoch: Int,
    onExportToPath: (probeId: String, storage: FileStorageLocation, directoryPath: String) -> String,
    dailyDosesFor: (probeId: String, fallbackBaseRateUsvH: Double) -> List<DailyDoseSummary>,
    onBack: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val validProbes = probes.filter { it.id != "placeholder" }
    if (validProbes.isEmpty()) {
        Box(
            modifier = modifier
                .fillMaxSize()
                .background(NetShieldAtmosphereBackgroundBrush),
            contentAlignment = Alignment.Center,
        ) {
            Text(text = "暂无探头数据", color = NetShieldTextSecondary, fontSize = 30.sp)
        }
        return
    }

    val orderedProbes = remember(validProbes, initialProbeId) {
        val selected = validProbes.firstOrNull { it.id == initialProbeId }
        if (selected == null) {
            validProbes
        } else {
            listOf(selected) + validProbes.filter { it.id != selected.id }
        }
    }
    val columns = remember(orderedProbes) {
        List(4) { index -> orderedProbes.getOrNull(index) }
    }

    val nameDrafts = remember(validProbes) { mutableStateMapOf<String, String>() }
    val locationDrafts = remember(validProbes) { mutableStateMapOf<String, String>() }
    val columnHints = remember { mutableStateMapOf<String, String>() }
    var orgNameDraft by remember(organizationName) { mutableStateOf(organizationName) }
    var orgEditBuffer by remember(organizationName) { mutableStateOf(organizationName) }
    var orgEditing by remember { mutableStateOf(false) }
    var pageHint by remember { mutableStateOf<String?>(null) }

    LaunchedEffect(validProbes) {
        validProbes.forEach { probe ->
            if (!nameDrafts.containsKey(probe.id)) nameDrafts[probe.id] = probe.name
            if (!locationDrafts.containsKey(probe.id)) locationDrafts[probe.id] = probe.location
        }
    }

    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val summaryHeight = maxHeight * ScreenSpec.SETTINGS_PROBE_SUMMARY_HEIGHT_FRACTION
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(NetShieldAtmosphereBackgroundBrush),
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(summaryHeight),
            ) {
                CompactRadiationHeader(
                    probes = probes,
                    modifier = Modifier.fillMaxSize(),
                )
                IconButton(
                    onClick = onBack,
                    modifier = Modifier
                        .align(Alignment.TopEnd)
                        .padding(top = 8.dp, end = 12.dp),
                ) {
                    Text("✕", color = NetShieldTextPrimary, fontSize = 34.sp)
                }
            }

            OrgNameBar(
                orgName = orgNameDraft,
                orgEditing = orgEditing,
                editBuffer = orgEditBuffer,
                pageHint = pageHint,
                onEditClick = {
                    orgEditBuffer = orgNameDraft
                    orgEditing = true
                    pageHint = null
                },
                onEditBufferChange = { orgEditBuffer = it },
                onSave = {
                    orgNameDraft = orgEditBuffer
                    onSaveOrganizationName(orgEditBuffer)
                    orgEditing = false
                    pageHint = "机构名称已保存"
                },
                onCancel = {
                    orgEditBuffer = orgNameDraft
                    orgEditing = false
                    pageHint = null
                },
            )

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .padding(horizontal = 14.dp, vertical = 10.dp),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                columns.forEach { probe ->
                    if (probe == null) {
                        EmptyProbeColumn(modifier = Modifier.weight(1f))
                    } else {
                        val name = nameDrafts[probe.id].orEmpty()
                        val location = locationDrafts[probe.id].orEmpty()
                        val baseRate = probe.doseRateText.toDoubleOrNull() ?: 0.0
                        val rows = remember(probe.id, probe.doseRateText) {
                            dailyDosesFor(probe.id, baseRate)
                        }
                        ProbeDataColumn(
                            probe = probe,
                            name = name,
                            location = location,
                            rows = rows,
                            hint = columnHints[probe.id],
                            onNameChange = { nameDrafts[probe.id] = it },
                            onLocationChange = { locationDrafts[probe.id] = it },
                            onSave = {
                                onSaveIdentity(probe.id, name, location)
                                columnHints[probe.id] = "已保存"
                            },
                            fileManagerRepository = fileManagerRepository,
                            usbGrantEpoch = usbGrantEpoch,
                            onExportToPath = { storage, directoryPath ->
                                columnHints[probe.id] = onExportToPath(probe.id, storage, directoryPath)
                            },
                            modifier = Modifier.weight(1f),
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun OrgNameBar(
    orgName: String,
    orgEditing: Boolean,
    editBuffer: String,
    pageHint: String?,
    onEditClick: () -> Unit,
    onEditBufferChange: (String) -> Unit,
    onSave: () -> Unit,
    onCancel: () -> Unit,
) {
    val focusRequester = remember { FocusRequester() }
    var fieldValue by remember { mutableStateOf(TextFieldValue()) }
    val titleStyle = TextStyle(
        color = NetShieldTextPrimary,
        fontSize = 32.sp,
        fontWeight = FontWeight.Normal,
    )
    val fieldStyle = TextStyle(
        color = NetShieldTextPrimary,
        fontSize = 24.sp,
        fontWeight = FontWeight.Normal,
    )

    LaunchedEffect(orgEditing, editBuffer) {
        if (orgEditing) {
            fieldValue = TextFieldValue(
                text = editBuffer,
                selection = TextRange(editBuffer.length),
            )
            focusRequester.requestFocus()
        }
    }

    BoxWithConstraints(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 20.dp, vertical = 10.dp),
    ) {
        val fieldWidth = maxWidth * 0.2f
        Column(modifier = Modifier.fillMaxWidth()) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                if (orgEditing) {
                    BasicTextField(
                        value = fieldValue,
                        onValueChange = { updated ->
                            fieldValue = updated
                            onEditBufferChange(updated.text)
                        },
                        textStyle = fieldStyle,
                        singleLine = true,
                        cursorBrush = SolidColor(NetShieldAccentBlue),
                        modifier = Modifier
                            .width(fieldWidth)
                            .clip(RoundedCornerShape(6.dp))
                            .background(Color.Black.copy(alpha = 0.22f))
                            .padding(horizontal = 10.dp, vertical = 6.dp)
                            .focusRequester(focusRequester),
                        decorationBox = { inner -> Box(contentAlignment = Alignment.CenterStart) { inner() } },
                    )
                    Button(
                        onClick = onSave,
                        colors = ButtonDefaults.buttonColors(containerColor = NetShieldAccentBlue),
                    ) {
                        Text("保存", color = NetShieldTextPrimary, fontSize = 20.sp)
                    }
                    TextButton(onClick = onCancel) {
                        Text("取消", color = NetShieldTextSecondary, fontSize = 20.sp)
                    }
                } else {
                    Text(
                        text = orgName.ifBlank { "NetShield" },
                        style = titleStyle,
                        maxLines = 1,
                    )
                    Icon(
                        imageVector = Icons.Outlined.Edit,
                        contentDescription = "编辑机构名称",
                        tint = NetShieldTextPrimary,
                        modifier = Modifier
                            .clickable(onClick = onEditClick)
                            .padding(4.dp),
                    )
                }
            }

            pageHint?.let {
                Spacer(modifier = Modifier.height(6.dp))
                Text(text = it, color = NetShieldAccentBlue, fontSize = 18.sp)
            }
        }
    }
}

@Composable
private fun ProbeDataColumn(
    probe: SlaveProbeUi,
    name: String,
    location: String,
    rows: List<DailyDoseSummary>,
    hint: String?,
    onNameChange: (String) -> Unit,
    onLocationChange: (String) -> Unit,
    onSave: () -> Unit,
    fileManagerRepository: FileManagerRepository,
    usbGrantEpoch: Int,
    onExportToPath: (FileStorageLocation, String) -> Unit,
    modifier: Modifier = Modifier,
) {
    var showEditDialog by remember { mutableStateOf(false) }
    var showExportDialog by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxHeight()
            .clip(RoundedCornerShape(10.dp))
            .background(NetShieldAtmospherePlayerOverlay)
            .padding(10.dp),
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 4.dp),
        ) {
            Column(
                modifier = Modifier
                    .align(Alignment.CenterStart)
                    .fillMaxWidth(0.35f),
            ) {
                Text(
                    text = if (name.isBlank()) "Detector" else name,
                    color = NetShieldTextPrimary,
                    fontSize = 22.sp,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Text(
                    text = if (probe.isOnline) "在线" else "离线",
                    color = if (probe.isOnline) NetShieldAccentBlue else NetShieldTextSecondary,
                    fontSize = 18.sp,
                )
            }
            Text(
                text = location.ifBlank { "大门口" },
                color = NetShieldTextPrimary,
                fontSize = 20.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.align(Alignment.Center),
            )
            Icon(
                imageVector = Icons.Outlined.Edit,
                contentDescription = "编辑探头信息",
                tint = NetShieldTextPrimary,
                modifier = Modifier
                    .align(Alignment.CenterEnd)
                    .size(28.dp)
                    .clickable { showEditDialog = true }
                    .padding(4.dp),
            )
        }

        hint?.let {
            Spacer(modifier = Modifier.height(4.dp))
            Text(text = it, color = NetShieldAccentBlue, fontSize = 16.sp)
        }

        Spacer(modifier = Modifier.height(8.dp))
        Text(
            text = "每日累积剂量",
            color = NetShieldTextSecondary,
            fontSize = 19.sp,
        )
        Spacer(modifier = Modifier.height(6.dp))
        LazyColumn(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            items(rows, key = { it.dateText }) { row ->
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(8.dp))
                        .background(Color.Black.copy(alpha = 0.2f))
                        .padding(horizontal = 10.dp, vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        text = row.dateText,
                        color = NetShieldTextSecondary,
                        fontSize = 18.sp,
                        modifier = Modifier.weight(1f),
                    )
                    Text(text = row.accumDoseText, color = NetShieldTextPrimary, fontSize = 18.sp)
                }
            }
        }

        Spacer(modifier = Modifier.height(8.dp))
        Button(
            onClick = { showExportDialog = true },
            colors = ButtonDefaults.buttonColors(containerColor = NetShieldAccentBlue),
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("导出", color = NetShieldTextPrimary, fontSize = 20.sp)
        }
    }

    if (showExportDialog) {
        ExportPathPickerDialog(
            repository = fileManagerRepository,
            usbGrantEpoch = usbGrantEpoch,
            onDismiss = { showExportDialog = false },
            onConfirm = { storage, directoryPath ->
                onExportToPath(storage, directoryPath)
                showExportDialog = false
            },
        )
    }

    if (showEditDialog) {
        ProbeIdentityEditDialog(
            initialName = name,
            initialLocation = location,
            onDismiss = { showEditDialog = false },
            onConfirm = { newName, newLocation ->
                onNameChange(newName)
                onLocationChange(newLocation)
                onSave()
                showEditDialog = false
            },
        )
    }
}

@Composable
private fun ProbeIdentityEditDialog(
    initialName: String,
    initialLocation: String,
    onDismiss: () -> Unit,
    onConfirm: (String, String) -> Unit,
) {
    var nameDraft by remember(initialName, initialLocation) {
        mutableStateOf(
            TextFieldValue(
                text = initialName,
                selection = TextRange(initialName.length),
            ),
        )
    }
    var locationDraft by remember(initialName, initialLocation) {
        mutableStateOf(
            TextFieldValue(
                text = initialLocation,
                selection = TextRange(initialLocation.length),
            ),
        )
    }
    val dialogBackground = Color(0xFF3946A1)
    val dialogAccent = Color(0xFF8EA2FF)
    val inputBackground = Color(0xFF4452B8)
    val fieldColors = OutlinedTextFieldDefaults.colors(
        focusedTextColor = NetShieldTextPrimary,
        unfocusedTextColor = NetShieldTextPrimary,
        focusedContainerColor = inputBackground,
        unfocusedContainerColor = inputBackground,
        cursorColor = dialogAccent,
        focusedBorderColor = dialogAccent,
        unfocusedBorderColor = Color(0xFF6F7CE0),
        focusedLabelColor = dialogAccent,
        unfocusedLabelColor = Color(0xFFD6DCFF),
    )

    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(
                text = "编辑探头信息",
                color = NetShieldTextPrimary,
                fontSize = 30.sp,
                fontWeight = FontWeight.SemiBold,
            )
        },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(14.dp)) {
                OutlinedTextField(
                    value = nameDraft,
                    onValueChange = { nameDraft = it },
                    label = { Text("探头名", fontSize = 22.sp) },
                    singleLine = true,
                    textStyle = TextStyle(color = NetShieldTextPrimary, fontSize = 24.sp),
                    colors = fieldColors,
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = locationDraft,
                    onValueChange = { locationDraft = it },
                    label = { Text("位置", fontSize = 22.sp) },
                    singleLine = true,
                    textStyle = TextStyle(color = NetShieldTextPrimary, fontSize = 24.sp),
                    colors = fieldColors,
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        },
        confirmButton = {
            TextButton(
                onClick = { onConfirm(nameDraft.text, locationDraft.text) },
            ) {
                Text("保存", color = NetShieldTextPrimary, fontSize = 24.sp)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("取消", color = Color(0xFFD6DCFF), fontSize = 24.sp)
            }
        },
        containerColor = dialogBackground,
        modifier = Modifier.fillMaxWidth(0.42f),
        properties = DialogProperties(usePlatformDefaultWidth = false),
    )
}

@Composable
private fun EmptyProbeColumn(modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .fillMaxHeight()
            .clip(RoundedCornerShape(10.dp))
            .background(NetShieldAtmospherePlayerOverlay.copy(alpha = 0.55f)),
        contentAlignment = Alignment.Center,
    ) {
        Text(text = "空位", color = NetShieldTextSecondary, fontSize = 22.sp)
    }
}

