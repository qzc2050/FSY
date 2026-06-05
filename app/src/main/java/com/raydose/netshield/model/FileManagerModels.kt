package com.raydose.netshield.model

enum class FileStorageLocation(val label: String) {
    Local("本地存储"),
    Usb("U盘存储"),
}

data class FileListItem(
    val path: String,
    val name: String,
    val isDirectory: Boolean,
    val sizeBytes: Long,
    val modifiedAt: Long,
    val storageLocation: FileStorageLocation,
)

data class PendingFileTransfer(
    val sourcePath: String,
    val sourceName: String,
    val isMove: Boolean,
    val sourceLocation: FileStorageLocation,
)

data class FileManagerUiState(
    val storageLocation: FileStorageLocation = FileStorageLocation.Local,
    val rootPath: String = "",
    val currentPath: String = "",
    val currentPathLabel: String = "",
    val items: List<FileListItem> = emptyList(),
    val searchQuery: String = "",
    val pendingTransfer: PendingFileTransfer? = null,
    val requiresUsbAccess: Boolean = false,
    val isLoading: Boolean = false,
    val message: String? = null,
)