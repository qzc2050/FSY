package com.raydose.raylink.ui.settings

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
import androidx.compose.material.icons.filled.Description
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.KeyboardArrowUp
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
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import com.raydose.raylink.R
import com.raydose.raylink.ui.labelText
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.raydose.raylink.data.FileManagerRepository
import com.raydose.raylink.model.FileListItem
import com.raydose.raylink.model.FileStorageLocation
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkSettingsEditorPanel
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.util.Locale

private val ApkExtension = "apk"

@Composable
fun ApkUpdatePickerDialog(
    repository: FileManagerRepository,
    usbGrantEpoch: Int,
    isInstalling: Boolean,
    onDismiss: () -> Unit,
    onRequestUsbAccess: () -> Unit,
    onInstall: (FileStorageLocation, String) -> Unit,
) {
    val storageUsbNotFound = stringResource(R.string.storage_usb_not_found)
    val storageNotAvailable = stringResource(R.string.storage_not_available)
    val readDirFailed = stringResource(R.string.storage_read_dir_failed)
    val apkFallbackName = stringResource(R.string.apk_selected_fallback)
    val preparingLabel = stringResource(R.string.apk_preparing)

    var storage by remember { mutableStateOf(FileStorageLocation.Usb) }
    var entries by remember { mutableStateOf<List<FileListItem>>(emptyList()) }
    var isLoading by remember { mutableStateOf(true) }
    var requiresUsbAccess by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf<String?>(null) }
    var reloadToken by remember { mutableIntStateOf(0) }
    val pathStack = remember { mutableStateListOf<Pair<String, String>>() }
    var selectedPath by remember { mutableStateOf<String?>(null) }

    val currentPathLabel = pathStack.joinToString(" / ") { it.second }
    val rowBackground = Color.White.copy(alpha = 0.08f)

    LaunchedEffect(storage, usbGrantEpoch, reloadToken) {
        isLoading = true
        message = null
        selectedPath = null
        runCatching {
            val root = withContext(Dispatchers.IO) { repository.resolveRoot(storage) }
            if (root == null) {
                pathStack.clear()
                entries = emptyList()
                requiresUsbAccess = storage == FileStorageLocation.Usb
                message = if (storage == FileStorageLocation.Usb) {
                    storageUsbNotFound
                } else {
                    storageNotAvailable
                }
                return@runCatching
            }
            requiresUsbAccess = false
            if (pathStack.isEmpty() || pathStack.first().first != root.first) {
                pathStack.clear()
                pathStack += root.first to root.second
            }
            val activePath = pathStack.last().first
            entries = withContext(Dispatchers.IO) {
                repository.listItems(storage, activePath, "")
                    .filter { item ->
                        item.isDirectory || item.name.substringAfterLast('.', "")
                            .lowercase(Locale.US) == ApkExtension
                    }
            }
        }.onFailure { error ->
            message = error.message ?: readDirFailed
            entries = emptyList()
        }
        isLoading = false
    }

    Dialog(onDismissRequest = { if (!isInstalling) onDismiss() }) {
        Column(
            modifier = Modifier
                .fillMaxWidth(0.82f)
                .clip(RoundedCornerShape(12.dp))
                .background(RaylinkSettingsEditorPanel)
                .padding(20.dp),
        ) {
            Text(
                text = stringResource(R.string.apk_picker_title),
                color = RaylinkTextPrimary,
                fontSize = 24.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Text(
                text = stringResource(R.string.apk_picker_subtitle),
                color = RaylinkTextSecondary,
                fontSize = 16.sp,
                modifier = Modifier.padding(top = 8.dp),
            )

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 16.dp),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                StorageTab(
                    text = FileStorageLocation.Local.labelText(),
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
                    text = FileStorageLocation.Usb.labelText(),
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
                fontSize = 16.sp,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.padding(top = 12.dp),
            )

            selectedPath?.let {
                Text(
                    text = stringResource(
                        R.string.label_selected,
                        entries.firstOrNull { item -> item.path == it }?.name ?: apkFallbackName,
                    ),
                    color = RaylinkAccentBlue,
                    fontSize = 16.sp,
                    modifier = Modifier.padding(top = 6.dp),
                )
            }

            message?.let {
                Text(text = it, color = RaylinkAccentBlue, fontSize = 16.sp, modifier = Modifier.padding(top = 6.dp))
            }

            if (requiresUsbAccess) {
                TextButton(onClick = onRequestUsbAccess, enabled = !isInstalling) {
                    Text(stringResource(R.string.storage_grant_usb), color = RaylinkTextPrimary, fontSize = 18.sp)
                }
            } else {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 8.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(rowBackground)
                        .clickable(enabled = pathStack.size > 1 && !isLoading && !isInstalling) {
                            if (pathStack.size > 1) {
                                pathStack.removeAt(pathStack.lastIndex)
                                reloadToken++
                            }
                        }
                        .padding(horizontal = 12.dp, vertical = 10.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(Icons.Default.KeyboardArrowUp, contentDescription = null, tint = RaylinkTextPrimary)
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
                            .heightIn(max = 320.dp)
                            .padding(top = 8.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        items(entries, key = { it.path }) { item ->
                            val selected = selectedPath == item.path
                            Row(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .clip(RoundedCornerShape(8.dp))
                                    .background(
                                        if (selected) RaylinkAccentBlue.copy(alpha = 0.25f) else rowBackground,
                                    )
                                    .clickable(enabled = !isInstalling) {
                                        if (item.isDirectory) {
                                            pathStack += item.path to item.name
                                            reloadToken++
                                        } else {
                                            selectedPath = item.path
                                        }
                                    }
                                    .padding(horizontal = 12.dp, vertical = 10.dp),
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                Icon(
                                    imageVector = if (item.isDirectory) Icons.Default.Folder else Icons.Default.Description,
                                    contentDescription = null,
                                    tint = if (selected) RaylinkAccentBlue else RaylinkTextPrimary,
                                )
                                Text(
                                    text = item.name,
                                    color = RaylinkTextPrimary,
                                    fontSize = 18.sp,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis,
                                    modifier = Modifier
                                        .weight(1f)
                                        .padding(start = 10.dp),
                                )
                            }
                        }
                    }
                }
            }

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 18.dp),
                horizontalArrangement = Arrangement.End,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                TextButton(onClick = onDismiss, enabled = !isInstalling) {
                    Text(stringResource(R.string.action_cancel), color = RaylinkTextPrimary, fontSize = 18.sp)
                }
                TextButton(
                    onClick = {
                        val path = selectedPath ?: return@TextButton
                        onInstall(storage, path)
                    },
                    enabled = !isInstalling && !selectedPath.isNullOrBlank(),
                ) {
                    Text(
                        text = if (isInstalling) preparingLabel else stringResource(R.string.action_install),
                        color = RaylinkAccentBlue,
                        fontSize = 18.sp,
                    )
                }
            }
        }
    }
}

@Composable
private fun StorageTab(
    text: String,
    selected: Boolean,
    onClick: () -> Unit,
) {
    Text(
        text = text,
        color = if (selected) RaylinkAccentBlue else RaylinkTextSecondary,
        fontSize = 18.sp,
        fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal,
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .clickable(onClick = onClick)
            .padding(horizontal = 12.dp, vertical = 8.dp),
    )
}
