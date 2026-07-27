package com.raydose.raylink.ui.probe

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.DialogProperties
import com.raydose.raylink.R
import com.raydose.raylink.data.FileManagerRepository
import com.raydose.raylink.model.FileListItem
import com.raydose.raylink.model.FileStorageLocation
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import com.raydose.raylink.ui.tr
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

@Composable
fun ExportPathPickerDialog(
    repository: FileManagerRepository,
    usbGrantEpoch: Int,
    onDismiss: () -> Unit,
    onConfirm: (FileStorageLocation, String) -> Unit,
) {
    val context = LocalContext.current
    var storage by remember { mutableStateOf(FileStorageLocation.Local) }
    var directories by remember { mutableStateOf<List<FileListItem>>(emptyList()) }
    var isLoading by remember { mutableStateOf(true) }
    var requiresUsbAccess by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf<String?>(null) }
    var reloadToken by remember { mutableIntStateOf(0) }
    val pathStack = remember { mutableStateListOf<Pair<String, String>>() }

    val currentPath = pathStack.lastOrNull()?.first.orEmpty()
    val currentPathLabel = pathStack.joinToString(" / ") { it.second }
    val localStorageLabel = stringResource(R.string.storage_local)
    val usbStorageLabel = stringResource(R.string.storage_usb)

    LaunchedEffect(storage, usbGrantEpoch, reloadToken) {
        isLoading = true
        message = null
        runCatching {
            val root = withContext(Dispatchers.IO) { repository.resolveRoot(storage) }
            if (root == null) {
                pathStack.clear()
                directories = emptyList()
                requiresUsbAccess = storage == FileStorageLocation.Usb
                message = if (storage == FileStorageLocation.Usb) {
                    context.tr(R.string.storage_usb_not_found)
                } else {
                    context.tr(R.string.storage_not_available)
                }
                return@runCatching
            }
            if (pathStack.isEmpty() || pathStack.first().first != root.first) {
                pathStack.clear()
                pathStack += root.first to root.second
            }
            val activePath = pathStack.last().first
            directories = withContext(Dispatchers.IO) {
                repository.listDirectoryEntries(storage, activePath)
            }
            requiresUsbAccess = false
        }.onFailure { error ->
            pathStack.clear()
            directories = emptyList()
            requiresUsbAccess = storage == FileStorageLocation.Usb
            message = when {
                storage == FileStorageLocation.Usb ->
                    context.tr(R.string.storage_usb_unavailable, error.message ?: context.tr(R.string.storage_usb_not_found))
                else -> error.message ?: context.tr(R.string.storage_read_dir_failed)
            }
        }
        isLoading = false
    }

    val dialogBackground = Color(0xFF3946A1)
    val rowBackground = Color(0xFF4452B8)

    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(
                text = stringResource(R.string.export_picker_title),
                color = RaylinkTextPrimary,
                fontSize = 28.sp,
                fontWeight = FontWeight.SemiBold,
            )
        },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    StorageTab(
                        text = localStorageLabel,
                        selected = storage == FileStorageLocation.Local,
                        onClick = {
                            if (storage != FileStorageLocation.Local) {
                                storage = FileStorageLocation.Local
                                pathStack.clear()
                                reloadToken++
                            }
                        },
                    )
                    StorageTab(
                        text = usbStorageLabel,
                        selected = storage == FileStorageLocation.Usb,
                        onClick = {
                            if (storage != FileStorageLocation.Usb) {
                                storage = FileStorageLocation.Usb
                                pathStack.clear()
                                reloadToken++
                            }
                        },
                    )
                }

                Text(
                    text = if (currentPathLabel.isBlank()) {
                        stringResource(R.string.label_current_path_empty)
                    } else {
                        stringResource(R.string.label_current_path, currentPathLabel)
                    },
                    color = RaylinkTextSecondary,
                    fontSize = 18.sp,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )

                message?.let {
                    Text(text = it, color = RaylinkAccentBlue, fontSize = 16.sp)
                }

                if (!requiresUsbAccess) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(8.dp))
                            .background(rowBackground)
                            .clickable(enabled = pathStack.size > 1 && !isLoading) {
                                if (pathStack.size > 1) {
                                    pathStack.removeAt(pathStack.lastIndex)
                                    reloadToken++
                                }
                            }
                            .padding(horizontal = 12.dp, vertical = 10.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Icon(
                            imageVector = Icons.Default.KeyboardArrowUp,
                            contentDescription = null,
                            tint = RaylinkTextPrimary,
                        )
                        Text(
                            text = stringResource(R.string.action_back_parent),
                            color = RaylinkTextPrimary,
                            fontSize = 18.sp,
                            modifier = Modifier.padding(start = 6.dp),
                        )
                    }

                    if (isLoading) {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(vertical = 24.dp),
                            horizontalArrangement = Arrangement.Center,
                        ) {
                            CircularProgressIndicator(color = RaylinkAccentBlue)
                        }
                    } else {
                        LazyColumn(
                            modifier = Modifier
                                .fillMaxWidth()
                                .heightIn(max = 280.dp),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            items(directories, key = { it.path }) { item ->
                                Row(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .clip(RoundedCornerShape(8.dp))
                                        .background(rowBackground)
                                        .clickable {
                                            pathStack += item.path to item.name
                                            reloadToken++
                                        }
                                        .padding(horizontal = 12.dp, vertical = 10.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.Folder,
                                        contentDescription = null,
                                        tint = RaylinkAccentBlue,
                                    )
                                    Text(
                                        text = item.name,
                                        color = RaylinkTextPrimary,
                                        fontSize = 18.sp,
                                        maxLines = 1,
                                        overflow = TextOverflow.Ellipsis,
                                        modifier = Modifier.padding(start = 8.dp),
                                    )
                                }
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                enabled = currentPath.isNotBlank() && !requiresUsbAccess && !isLoading,
                onClick = { onConfirm(storage, currentPath) },
            ) {
                Text(stringResource(R.string.action_confirm), color = RaylinkTextPrimary, fontSize = 22.sp)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(R.string.action_cancel), color = Color(0xFFD6DCFF), fontSize = 22.sp)
            }
        },
        containerColor = dialogBackground,
        modifier = Modifier.fillMaxWidth(0.56f),
        properties = DialogProperties(usePlatformDefaultWidth = false),
    )
}

@Composable
private fun StorageTab(
    text: String,
    selected: Boolean,
    onClick: () -> Unit,
) {
    Text(
        text = text,
        color = if (selected) RaylinkTextPrimary else RaylinkTextSecondary,
        fontSize = 18.sp,
        fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal,
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .background(if (selected) RaylinkAccentBlue else Color.Black.copy(alpha = 0.18f))
            .clickable(onClick = onClick)
            .padding(horizontal = 14.dp, vertical = 8.dp),
    )
}
