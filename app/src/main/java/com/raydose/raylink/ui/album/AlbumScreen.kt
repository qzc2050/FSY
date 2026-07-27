package com.raydose.raylink.ui.album

import android.graphics.BitmapFactory
import android.net.Uri
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
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
import androidx.compose.foundation.layout.widthIn
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
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.DialogProperties
import com.raydose.raylink.R
import com.raydose.raylink.data.AlbumImageRepository
import com.raydose.raylink.data.FileManagerRepository
import com.raydose.raylink.ui.components.MessageEditDialog
import com.raydose.raylink.model.AlbumMessage
import com.raydose.raylink.model.AlbumSettings
import com.raydose.raylink.model.SlaveProbeUi
import com.raydose.raylink.ui.components.CompactRadiationHeader
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkAtmosphereBackgroundBrush
import com.raydose.raylink.ui.theme.RaylinkAtmospherePlayerOverlay
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import com.raydose.raylink.ui.tr
import com.raydose.raylink.ui.theme.ScreenSpec
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
                .background(RaylinkAtmosphereBackgroundBrush),
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
    var cleanupRangePending by remember { mutableStateOf<MessageCleanupRange?>(null) }
    var deleteSelectedPending by remember { mutableStateOf(false) }
    var scrollToLatestRequest by remember { mutableStateOf(0) }

    Column(
        modifier = modifier.background(RaylinkAtmosphereBackgroundBrush),
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
                            imageImportMessage = context.tr(R.string.album_import_success)
                            showImagePicker = false
                        }
                        .onFailure {
                            imageImportMessage = context.tr(
                                R.string.album_import_failed,
                                it.message ?: context.tr(R.string.error_unknown),
                            )
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
            title = stringResource(R.string.album_delete_message_title),
            message = stringResource(R.string.album_delete_message_confirm),
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
            title = stringResource(R.string.album_cleanup_title),
            message = stringResource(
                R.string.album_cleanup_confirm,
                when (range) {
                    MessageCleanupRange.Week -> stringResource(R.string.range_one_week_ago)
                    MessageCleanupRange.Month -> stringResource(R.string.range_one_month_ago)
                },
            ),
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
            title = stringResource(R.string.album_delete_selected_title),
            message = stringResource(R.string.album_delete_selected_confirm, selectedMessageIds.size),
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
            text = stringResource(R.string.album_title),
            color = RaylinkTextPrimary,
            fontSize = 28.sp,
            fontWeight = FontWeight.SemiBold,
        )
        Spacer(modifier = Modifier.weight(1f))
        Text(
            text = "×",
            color = RaylinkTextPrimary,
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
            colors = ButtonDefaults.buttonColors(containerColor = RaylinkAccentBlue),
            shape = RoundedCornerShape(28.dp),
            contentPadding = PaddingValues(horizontal = 20.dp, vertical = 0.dp),
            modifier = Modifier
                .height(58.dp)
                .widthIn(min = 88.dp),
        ) {
            Text(
                text = if (isImportingImage) "…" else stringResource(R.string.action_select),
                color = RaylinkTextPrimary,
                fontSize = 18.sp,
                maxLines = 1,
                softWrap = false,
            )
        }
        importMessage?.let {
            Text(
                text = it,
                color = RaylinkTextSecondary,
                fontSize = 14.sp,
                modifier = Modifier.padding(top = 8.dp),
            )
        }
        if (selectedImageUri.isNotBlank() && !imageAvailable) {
            Text(
                text = stringResource(R.string.album_image_unavailable),
                color = Color(0xFFFFB4B4),
                fontSize = 14.sp,
                modifier = Modifier.padding(top = 8.dp),
            )
        }
        Spacer(modifier = Modifier.height(16.dp))
        AlbumSwitchRow(stringResource(R.string.album_apply_standby), applyStandby, onApplyStandbyChange)
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
    onCleanupByRange: (MessageCleanupRange) -> Unit,
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
            .background(RaylinkAtmospherePlayerOverlay)
            .padding(top = 42.dp, start = 8.dp, end = 8.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                text = stringResource(R.string.album_messages_count, messages.size),
                color = RaylinkTextPrimary,
                fontSize = 24.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Spacer(modifier = Modifier.weight(1f))
            TextButton(onClick = { onManageModeChange(!manageMode) }) {
                Text(
                    text = if (manageMode) {
                        stringResource(R.string.action_done)
                    } else {
                        stringResource(R.string.action_manage)
                    },
                    color = RaylinkTextPrimary,
                    fontSize = 18.sp,
                )
            }
            Icon(
                imageVector = Icons.Outlined.Edit,
                contentDescription = "编辑留言",
                tint = RaylinkTextPrimary,
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
            label = { Text(stringResource(R.string.album_search_placeholder), color = RaylinkTextSecondary) },
            textStyle = androidx.compose.ui.text.TextStyle(
                color = RaylinkTextPrimary,
                fontSize = 18.sp,
            ),
            colors = OutlinedTextFieldDefaults.colors(
                focusedTextColor = RaylinkTextPrimary,
                unfocusedTextColor = RaylinkTextPrimary,
                focusedContainerColor = Color(0x332D3B9F),
                unfocusedContainerColor = Color(0x332D3B9F),
                cursorColor = RaylinkAccentBlue,
                focusedBorderColor = RaylinkAccentBlue,
                unfocusedBorderColor = Color.White.copy(alpha = 0.38f),
                focusedLabelColor = RaylinkAccentBlue,
                unfocusedLabelColor = RaylinkTextSecondary,
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
                        text = if (searchQuery.isBlank()) {
                            stringResource(R.string.home_no_messages)
                        } else {
                            stringResource(R.string.album_no_search_results)
                        },
                        color = RaylinkTextSecondary,
                        fontSize = 18.sp,
                        modifier = Modifier.padding(top = 12.dp),
                    )
                }
            }
        }
        Spacer(modifier = Modifier.height(14.dp))
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            AlbumSwitchRow(stringResource(R.string.album_show_home_messages), showHomeMessages, onShowHomeMessagesChange)
            AlbumSwitchRow(stringResource(R.string.album_show_standby_messages), showStandbyMessages, onShowStandbyMessagesChange)
        }
        Spacer(modifier = Modifier.height(12.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            Spacer(modifier = Modifier.weight(1f))
            if (manageMode) {
                Text(
                    text = stringResource(R.string.files_selected_count, selectedMessageIds.size),
                    color = RaylinkTextPrimary,
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
                    Text(stringResource(R.string.album_delete_selected), color = RaylinkTextPrimary, fontSize = 14.sp)
                }
            } else {
                Button(
                    onClick = { onCleanupByRange(MessageCleanupRange.Week) },
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFE84B5A)),
                    shape = RoundedCornerShape(18.dp),
                    modifier = Modifier.height(36.dp),
                ) {
                    Text(stringResource(R.string.album_cleanup_week), color = RaylinkTextPrimary, fontSize = 14.sp)
                }
                Spacer(modifier = Modifier.width(12.dp))
                Button(
                    onClick = { onCleanupByRange(MessageCleanupRange.Month) },
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFE84B5A)),
                    shape = RoundedCornerShape(18.dp),
                    modifier = Modifier.height(36.dp),
                ) {
                    Text(stringResource(R.string.album_cleanup_month), color = RaylinkTextPrimary, fontSize = 14.sp)
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
                    .background(if (selected) RaylinkAccentBlue else Color.White.copy(alpha = 0.35f))
                    .clickable { onSelectionChange(!selected) },
                contentAlignment = Alignment.Center,
            ) {
                if (selected) {
                    Text(
                        text = "✓",
                        color = RaylinkTextPrimary,
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
                color = RaylinkTextSecondary,
                fontSize = 14.sp,
            )
            Text(
                text = message.text,
                color = RaylinkTextPrimary,
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
                    color = RaylinkTextPrimary,
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
                color = RaylinkTextPrimary,
                fontSize = 26.sp,
                fontWeight = FontWeight.SemiBold,
            )
        },
        text = {
            Text(
                text = message,
                color = RaylinkTextPrimary,
                fontSize = 20.sp,
            )
        },
        confirmButton = {
            TextButton(onClick = onConfirm) {
                Text(stringResource(R.string.delete_probe_confirm), color = Color(0xFFFFD3D8), fontSize = 20.sp)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(R.string.action_cancel), color = Color(0xFFD6DCFF), fontSize = 20.sp)
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
            color = RaylinkTextPrimary,
            fontSize = 15.sp,
        )
        Spacer(modifier = Modifier.width(8.dp))
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            colors = SwitchDefaults.colors(
                checkedThumbColor = RaylinkTextPrimary,
                checkedTrackColor = RaylinkAccentBlue,
                uncheckedThumbColor = RaylinkTextPrimary,
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

private enum class MessageCleanupRange { Week, Month }

private fun deleteMessagesByRange(
    messages: List<AlbumMessage>,
    selectedRange: MessageCleanupRange,
): List<AlbumMessage> {
    val now = System.currentTimeMillis()
    val cutoff = when (selectedRange) {
        MessageCleanupRange.Month -> now - 30L * 24L * 60L * 60L * 1000L
        MessageCleanupRange.Week -> now - 7L * 24L * 60L * 60L * 1000L
    }
    return messages.filter { it.createdAtMillis >= cutoff }
}
