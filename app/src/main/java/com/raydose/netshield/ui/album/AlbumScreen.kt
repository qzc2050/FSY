package com.raydose.netshield.ui.album

import android.graphics.BitmapFactory
import android.net.Uri
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Edit
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.DialogProperties
import com.raydose.netshield.R
import com.raydose.netshield.data.AlbumImageRepository
import com.raydose.netshield.data.FileManagerRepository
import com.raydose.netshield.ui.components.MessageEditDialog
import com.raydose.netshield.model.AlbumMessage
import com.raydose.netshield.model.AlbumSettings
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.ui.components.CompactRadiationHeader
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldAtmosphereBackgroundBrush
import com.raydose.netshield.ui.theme.NetShieldAtmospherePlayerOverlay
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun AlbumScreen(
    probes: List<SlaveProbeUi>,
    settings: AlbumSettings,
    messages: List<AlbumMessage>,
    fileManagerRepository: FileManagerRepository,
    usbGrantEpoch: Int,
    onSettingsChange: (AlbumSettings) -> Unit,
    onMessagesChange: (List<AlbumMessage>) -> Unit,
    onRequestUsbAccess: () -> Unit,
    onBack: () -> Unit,
    modifier: Modifier = Modifier,
) {
    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val summaryHeight = maxHeight * ScreenSpec.SETTINGS_PROBE_SUMMARY_HEIGHT_FRACTION
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(NetShieldAtmosphereBackgroundBrush),
        ) {
            CompactRadiationHeader(
                probes = probes,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(summaryHeight),
            )
            AlbumContent(
                settings = settings,
                messages = messages,
                fileManagerRepository = fileManagerRepository,
                usbGrantEpoch = usbGrantEpoch,
                onSettingsChange = onSettingsChange,
                onMessagesChange = onMessagesChange,
                onRequestUsbAccess = onRequestUsbAccess,
                onClose = onBack,
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f),
            )
        }
    }
}

@Composable
private fun AlbumContent(
    settings: AlbumSettings,
    messages: List<AlbumMessage>,
    fileManagerRepository: FileManagerRepository,
    usbGrantEpoch: Int,
    onSettingsChange: (AlbumSettings) -> Unit,
    onMessagesChange: (List<AlbumMessage>) -> Unit,
    onRequestUsbAccess: () -> Unit,
    onClose: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val albumImageRepository = remember { AlbumImageRepository(context) }
    var showImagePicker by remember { mutableStateOf(false) }
    var isImportingImage by remember { mutableStateOf(false) }
    var imageImportMessage by remember { mutableStateOf<String?>(null) }
    var editingMessageId by remember { mutableStateOf<Long?>(null) }
    var editingText by remember { mutableStateOf<String?>(null) }
    var searchQuery by remember { mutableStateOf("") }
    var manageMode by remember { mutableStateOf(false) }
    var selectedMessageIds by remember { mutableStateOf<Set<Long>>(emptySet()) }
    var messagePendingDelete by remember { mutableStateOf<AlbumMessage?>(null) }
    var cleanupRangePending by remember { mutableStateOf<String?>(null) }
    var deleteSelectedPending by remember { mutableStateOf(false) }
    var scrollToLatestRequest by remember { mutableStateOf(0) }

    Column(
        modifier = modifier.background(NetShieldAtmosphereBackgroundBrush),
    ) {
        AlbumWindowBar(onClose = onClose, modifier = Modifier.fillMaxWidth().height(58.dp))
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f)
                .padding(start = 18.dp, end = 22.dp, bottom = 28.dp),
            horizontalArrangement = Arrangement.spacedBy(28.dp),
        ) {
            AlbumPreviewPanel(
                selectedImageUri = settings.selectedImageUri,
                applyStandby = settings.applyStandby,
                importMessage = imageImportMessage,
                isImportingImage = isImportingImage,
                imageAvailable = settings.selectedImageUri.isBlank() ||
                    albumImageRepository.isSelectedImageAvailable(settings.selectedImageUri),
                onPickImage = { showImagePicker = true },
                onApplyStandbyChange = { onSettingsChange(settings.copy(applyStandby = it)) },
                modifier = Modifier
                    .weight(0.38f)
                    .fillMaxHeight(),
            )
            MessagePanel(
                messages = messages,
                showHomeMessages = settings.showHomeMessages,
                showStandbyMessages = settings.showStandbyMessages,
                searchQuery = searchQuery,
                manageMode = manageMode,
                selectedMessageIds = selectedMessageIds,
                scrollToLatestRequest = scrollToLatestRequest,
                onShowHomeMessagesChange = { onSettingsChange(settings.copy(showHomeMessages = it)) },
                onShowStandbyMessagesChange = { onSettingsChange(settings.copy(showStandbyMessages = it)) },
                onSearchQueryChange = { searchQuery = it },
                onManageModeChange = { enabled ->
                    manageMode = enabled
                    if (!enabled) selectedMessageIds = emptySet()
                },
                onAddMessage = {
                    editingMessageId = null
                    editingText = ""
                },
                onEditMessage = { message ->
                    editingMessageId = message.id
                    editingText = message.text
                },
                onDeleteMessage = { message ->
                    messagePendingDelete = message
                },
                onMessageSelectionChange = { message, selected ->
                    selectedMessageIds = if (selected) {
                        selectedMessageIds + message.id
                    } else {
                        selectedMessageIds - message.id
                    }
                },
                onDeleteSelected = {
                    if (selectedMessageIds.isNotEmpty()) deleteSelectedPending = true
                },
                onCleanupByRange = { range ->
                    cleanupRangePending = range
                },
                modifier = Modifier
                    .weight(0.62f)
                    .fillMaxHeight(),
            )
        }
    }
    if (showImagePicker) {
        val initialDirectory = settings.lastPickerDirectory.ifBlank {
            albumImageRepository.defaultLocalPicturesDirectory().absolutePath
        }
        AlbumImagePickerDialog(
            repository = fileManagerRepository,
            albumImageRepository = albumImageRepository,
            usbGrantEpoch = usbGrantEpoch,
            isImporting = isImportingImage,
            initialStorage = settings.lastPickerStorage,
            initialDirectoryPath = initialDirectory,
            onDismiss = {
                if (!isImportingImage) showImagePicker = false
            },
            onRequestUsbAccess = onRequestUsbAccess,
            onImageSelected = { location, path, browseDirectory ->
                scope.launch {
                    isImportingImage = true
                    imageImportMessage = null
                    val result = withContext(Dispatchers.IO) {
                        albumImageRepository.importImage(
                            fileManagerRepository = fileManagerRepository,
                            sourceLocation = location,
                            sourcePath = path,
                        )
                    }
                    result
                        .onSuccess { uri ->
                            onSettingsChange(
                                albumImageRepository.settingsForSelectedImage(
                                    current = settings,
                                    importedUri = uri,
                                    pickerStorage = location,
                                    pickerDirectory = browseDirectory.ifBlank {
                                        File(path).parent.orEmpty()
                                    },
                                    sourcePath = path,
                                ),
                            )
                            imageImportMessage = "图片已复制到本机，拔出 U 盘不影响显示"
                            showImagePicker = false
                        }
                        .onFailure {
                            imageImportMessage = "图片导入失败：${it.message ?: "未知错误"}"
                        }
                    isImportingImage = false
                }
            },
        )
    }
    editingText?.let { text ->
        MessageEditDialog(
            initialText = text,
            isNew = editingMessageId == null,
            onDismiss = { editingText = null },
            onConfirm = { newText ->
                val trimmed = newText.trim()
                if (trimmed.isNotEmpty()) {
                    val updated = if (editingMessageId == null) {
                        listOf(
                            AlbumMessage(
                                id = nextMessageId(messages),
                                text = trimmed,
                                createdAtMillis = System.currentTimeMillis(),
                            ),
                        ) + messages
                    } else {
                        messages.map { message ->
                            if (message.id == editingMessageId) message.copy(text = trimmed) else message
                        }
                    }
                    onMessagesChange(updated)
                    if (editingMessageId == null) {
                        searchQuery = ""
                        scrollToLatestRequest += 1
                    }
                }
                editingText = null
            },
        )
    }
    messagePendingDelete?.let { message ->
        ConfirmDeleteDialog(
            title = "删除留言",
            message = "确定删除这条留言吗？",
            onDismiss = { messagePendingDelete = null },
            onConfirm = {
                onMessagesChange(messages.filterNot { it.id == message.id })
                selectedMessageIds = selectedMessageIds - message.id
                messagePendingDelete = null
            },
        )
    }
    cleanupRangePending?.let { range ->
        ConfirmDeleteDialog(
            title = "清理留言",
            message = "确定删除${range}的留言吗？",
            onDismiss = { cleanupRangePending = null },
            onConfirm = {
                val updated = deleteMessagesByRange(messages, range)
                onMessagesChange(updated)
                selectedMessageIds = selectedMessageIds.intersect(updated.map { it.id }.toSet())
                cleanupRangePending = null
            },
        )
    }
    if (deleteSelectedPending) {
        ConfirmDeleteDialog(
            title = "删除选中留言",
            message = "确定删除选中的 ${selectedMessageIds.size} 条留言吗？",
            onDismiss = { deleteSelectedPending = false },
            onConfirm = {
                onMessagesChange(messages.filterNot { it.id in selectedMessageIds })
                selectedMessageIds = emptySet()
                manageMode = false
                deleteSelectedPending = false
            },
        )
    }
}

@Composable
private fun AlbumWindowBar(
    onClose: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier.padding(start = 18.dp, end = 18.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "电子相册",
            color = NetShieldTextPrimary,
            fontSize = 28.sp,
            fontWeight = FontWeight.SemiBold,
        )
        Spacer(modifier = Modifier.weight(1f))
        Text(
            text = "×",
            color = NetShieldTextPrimary,
            fontSize = 36.sp,
            modifier = Modifier.clickable(onClick = onClose),
        )
    }
}

@Composable
private fun AlbumPreviewPanel(
    selectedImageUri: String,
    applyStandby: Boolean,
    importMessage: String?,
    isImportingImage: Boolean,
    imageAvailable: Boolean,
    onPickImage: () -> Unit,
    onApplyStandbyChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier.padding(top = 26.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        AlbumPreviewImage(
            selectedImageUri = selectedImageUri,
            imageAvailable = imageAvailable,
        )
        Spacer(modifier = Modifier.height(18.dp))
        Button(
            onClick = onPickImage,
            enabled = !isImportingImage,
            colors = ButtonDefaults.buttonColors(containerColor = NetShieldAccentBlue),
            shape = RoundedCornerShape(28.dp),
            modifier = Modifier.size(width = 88.dp, height = 58.dp),
        ) {
            Text(
                text = if (isImportingImage) "…" else "选择",
                color = NetShieldTextPrimary,
                fontSize = 18.sp,
            )
        }
        importMessage?.let {
            Text(
                text = it,
                color = NetShieldTextSecondary,
                fontSize = 14.sp,
                modifier = Modifier.padding(top = 8.dp),
            )
        }
        if (selectedImageUri.isNotBlank() && !imageAvailable) {
            Text(
                text = "当前图片不可用（可能来自已拔出的 U 盘），请重新选择",
                color = Color(0xFFFFB4B4),
                fontSize = 14.sp,
                modifier = Modifier.padding(top = 8.dp),
            )
        }
        Spacer(modifier = Modifier.height(16.dp))
        AlbumSwitchRow("应用于待机画面", applyStandby, onApplyStandbyChange)
    }
}

@Composable
private fun AlbumPreviewImage(selectedImageUri: String, imageAvailable: Boolean) {
    val imageModifier = Modifier
        .fillMaxWidth()
        .aspectRatio(16f / 9f)
        .clip(RoundedCornerShape(8.dp))
    if (selectedImageUri.isBlank() || !imageAvailable) {
        Image(
            painter = painterResource(R.drawable.music_galaxy_cover),
            contentDescription = "相册预览",
            contentScale = ContentScale.Crop,
            modifier = imageModifier,
        )
    } else {
        val context = LocalContext.current
        val bitmap = remember(selectedImageUri) {
            runCatching { decodeAlbumPreviewBitmap(context, selectedImageUri)?.asImageBitmap() }.getOrNull()
        }
        if (bitmap != null) {
            Image(
                bitmap = bitmap,
                contentDescription = "相册预览",
                contentScale = ContentScale.Crop,
                modifier = imageModifier,
            )
        } else {
            Image(
                painter = painterResource(R.drawable.music_galaxy_cover),
                contentDescription = "相册预览",
                contentScale = ContentScale.Crop,
                modifier = imageModifier,
            )
        }
    }
}

@Composable
private fun MessagePanel(
    messages: List<AlbumMessage>,
    showHomeMessages: Boolean,
    showStandbyMessages: Boolean,
    searchQuery: String,
    manageMode: Boolean,
    selectedMessageIds: Set<Long>,
    scrollToLatestRequest: Int,
    onShowHomeMessagesChange: (Boolean) -> Unit,
    onShowStandbyMessagesChange: (Boolean) -> Unit,
    onSearchQueryChange: (String) -> Unit,
    onManageModeChange: (Boolean) -> Unit,
    onAddMessage: () -> Unit,
    onEditMessage: (AlbumMessage) -> Unit,
    onDeleteMessage: (AlbumMessage) -> Unit,
    onMessageSelectionChange: (AlbumMessage, Boolean) -> Unit,
    onDeleteSelected: () -> Unit,
    onCleanupByRange: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    val visibleMessages = messages
        .sortedByDescending { it.createdAtMillis }
        .filter { message ->
            searchQuery.isBlank() || message.text.contains(searchQuery, ignoreCase = true)
        }
    val listState = rememberLazyListState()
    LaunchedEffect(scrollToLatestRequest) {
        if (scrollToLatestRequest > 0 && visibleMessages.isNotEmpty()) {
            listState.animateScrollToItem(0)
        }
    }
    Column(
        modifier = modifier
            .background(NetShieldAtmospherePlayerOverlay)
            .padding(top = 42.dp, start = 8.dp, end = 8.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                text = "留言（${messages.size}）",
                color = NetShieldTextPrimary,
                fontSize = 24.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Spacer(modifier = Modifier.weight(1f))
            TextButton(onClick = { onManageModeChange(!manageMode) }) {
                Text(
                    text = if (manageMode) "完成" else "管理",
                    color = NetShieldTextPrimary,
                    fontSize = 18.sp,
                )
            }
            Icon(
                imageVector = Icons.Outlined.Edit,
                contentDescription = "编辑留言",
                tint = NetShieldTextPrimary,
                modifier = Modifier
                    .size(28.dp)
                    .clickable(onClick = onAddMessage),
            )
        }
        Spacer(modifier = Modifier.height(10.dp))
        OutlinedTextField(
            value = searchQuery,
            onValueChange = onSearchQueryChange,
            singleLine = true,
            label = { Text("搜索留言", color = NetShieldTextSecondary) },
            textStyle = androidx.compose.ui.text.TextStyle(
                color = NetShieldTextPrimary,
                fontSize = 18.sp,
            ),
            colors = OutlinedTextFieldDefaults.colors(
                focusedTextColor = NetShieldTextPrimary,
                unfocusedTextColor = NetShieldTextPrimary,
                focusedContainerColor = Color(0x332D3B9F),
                unfocusedContainerColor = Color(0x332D3B9F),
                cursorColor = NetShieldAccentBlue,
                focusedBorderColor = NetShieldAccentBlue,
                unfocusedBorderColor = Color.White.copy(alpha = 0.38f),
                focusedLabelColor = NetShieldAccentBlue,
                unfocusedLabelColor = NetShieldTextSecondary,
            ),
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(modifier = Modifier.height(10.dp))
        LazyColumn(
            state = listState,
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            items(visibleMessages, key = { it.id }) { message ->
                MessageRow(
                    message = message,
                    manageMode = manageMode,
                    selected = message.id in selectedMessageIds,
                    onClick = {
                        if (manageMode) {
                            onMessageSelectionChange(message, message.id !in selectedMessageIds)
                        } else {
                            onEditMessage(message)
                        }
                    },
                    onDelete = { onDeleteMessage(message) },
                    onSelectionChange = { selected -> onMessageSelectionChange(message, selected) },
                )
            }
            if (visibleMessages.isEmpty()) {
                item {
                    Text(
                        text = if (searchQuery.isBlank()) "暂无留言" else "没有匹配的留言",
                        color = NetShieldTextSecondary,
                        fontSize = 18.sp,
                        modifier = Modifier.padding(top = 12.dp),
                    )
                }
            }
        }
        Spacer(modifier = Modifier.height(14.dp))
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            AlbumSwitchRow("主页显示留言", showHomeMessages, onShowHomeMessagesChange)
            AlbumSwitchRow("待机页显示留言", showStandbyMessages, onShowStandbyMessagesChange)
        }
        Spacer(modifier = Modifier.height(12.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Spacer(modifier = Modifier.weight(1f))
            if (manageMode) {
                Text(
                    text = "已选 ${selectedMessageIds.size}",
                    color = NetShieldTextPrimary,
                    fontSize = 15.sp,
                )
                Spacer(modifier = Modifier.width(12.dp))
                Button(
                    onClick = onDeleteSelected,
                    enabled = selectedMessageIds.isNotEmpty(),
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFE84B5A)),
                    shape = RoundedCornerShape(18.dp),
                    modifier = Modifier.height(36.dp),
                ) {
                    Text("删除选中", color = NetShieldTextPrimary, fontSize = 14.sp)
                }
            } else {
                Button(
                    onClick = { onCleanupByRange("一周前") },
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFE84B5A)),
                    shape = RoundedCornerShape(18.dp),
                    modifier = Modifier.height(36.dp),
                ) {
                    Text("清理一周前", color = NetShieldTextPrimary, fontSize = 14.sp)
                }
                Spacer(modifier = Modifier.width(12.dp))
                Button(
                    onClick = { onCleanupByRange("一月前") },
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFE84B5A)),
                    shape = RoundedCornerShape(18.dp),
                    modifier = Modifier.height(36.dp),
                ) {
                    Text("清理一月前", color = NetShieldTextPrimary, fontSize = 14.sp)
                }
            }
        }
    }
}

@Composable
private fun MessageRow(
    message: AlbumMessage,
    manageMode: Boolean,
    selected: Boolean,
    onClick: () -> Unit,
    onDelete: () -> Unit,
    onSelectionChange: (Boolean) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .defaultMinSize(minHeight = 52.dp)
            .clip(RoundedCornerShape(4.dp))
            .background(Color(0xFF3946A1))
            .clickable(onClick = onClick)
            .padding(start = 14.dp, end = 8.dp, top = 10.dp, bottom = 10.dp),
        verticalAlignment = Alignment.Top,
    ) {
        if (manageMode) {
            Box(
                modifier = Modifier
                    .padding(top = 2.dp)
                    .size(26.dp)
                    .clip(RoundedCornerShape(13.dp))
                    .background(if (selected) NetShieldAccentBlue else Color.White.copy(alpha = 0.35f))
                    .clickable { onSelectionChange(!selected) },
                contentAlignment = Alignment.Center,
            ) {
                if (selected) {
                    Text(
                        text = "✓",
                        color = NetShieldTextPrimary,
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold,
                    )
                }
            }
            Spacer(modifier = Modifier.width(10.dp))
        }
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = formatMessageTimestamp(message.createdAtMillis),
                color = NetShieldTextSecondary,
                fontSize = 14.sp,
            )
            Text(
                text = message.text,
                color = NetShieldTextPrimary,
                fontSize = 20.sp,
                lineHeight = 28.sp,
                modifier = Modifier.padding(top = 4.dp),
            )
        }
        if (!manageMode) {
            Box(
                modifier = Modifier
                    .padding(top = 2.dp)
                    .size(28.dp)
                    .clip(RoundedCornerShape(14.dp))
                    .background(Color.Red)
                    .clickable(onClick = onDelete),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    text = "×",
                    color = NetShieldTextPrimary,
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Bold,
                )
            }
        }
    }
}

@Composable
private fun ConfirmDeleteDialog(
    title: String,
    message: String,
    onDismiss: () -> Unit,
    onConfirm: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(
                text = title,
                color = NetShieldTextPrimary,
                fontSize = 26.sp,
                fontWeight = FontWeight.SemiBold,
            )
        },
        text = {
            Text(
                text = message,
                color = NetShieldTextPrimary,
                fontSize = 20.sp,
            )
        },
        confirmButton = {
            TextButton(onClick = onConfirm) {
                Text("确定删除", color = Color(0xFFFFD3D8), fontSize = 20.sp)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("取消", color = Color(0xFFD6DCFF), fontSize = 20.sp)
            }
        },
        containerColor = Color(0xFF3946A1),
        modifier = Modifier.fillMaxWidth(0.52f),
        properties = DialogProperties(usePlatformDefaultWidth = false),
    )
}

@Composable
private fun AlbumSwitchRow(
    label: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.Center,
    ) {
        Text(
            text = label,
            color = NetShieldTextPrimary,
            fontSize = 15.sp,
        )
        Spacer(modifier = Modifier.width(8.dp))
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            colors = SwitchDefaults.colors(
                checkedThumbColor = NetShieldTextPrimary,
                checkedTrackColor = NetShieldAccentBlue,
                uncheckedThumbColor = NetShieldTextPrimary,
                uncheckedTrackColor = Color.White.copy(alpha = 0.35f),
            ),
        )
    }
}

private fun nextMessageId(messages: List<AlbumMessage>): Long =
    (messages.maxOfOrNull { it.id } ?: 0L) + 1L

private fun formatMessageTimestamp(millis: Long): String =
    SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.getDefault()).format(Date(millis))

private fun decodeAlbumPreviewBitmap(context: android.content.Context, uriString: String): android.graphics.Bitmap? {
    if (uriString.isBlank()) return null
    return runCatching {
        when {
            uriString.startsWith("file:") -> {
                val path = Uri.parse(uriString).path ?: return@runCatching null
                BitmapFactory.decodeFile(path)
            }
            uriString.startsWith("/") -> BitmapFactory.decodeFile(uriString)
            else -> context.contentResolver.openInputStream(Uri.parse(uriString))?.use(BitmapFactory::decodeStream)
        }
    }.getOrNull()
}

private fun deleteMessagesByRange(
    messages: List<AlbumMessage>,
    selectedRange: String,
): List<AlbumMessage> {
    val now = System.currentTimeMillis()
    val cutoff = when (selectedRange) {
        "一月前" -> now - 30L * 24L * 60L * 60L * 1000L
        else -> now - 7L * 24L * 60L * 60L * 1000L
    }
    return messages.filter { it.createdAtMillis >= cutoff }
}
