package com.raydose.raylink.model

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
    /** 递归搜索时显示相对当前目录的父路径，如 Music */
    val parentPathLabel: String? = null,
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
    val selectionMode: Boolean = false,
    val selectedPaths: Set<String> = emptySet(),
) {
    val selectedCount: Int get() = selectedPaths.size
    val allVisibleSelected: Boolean get() = items.isNotEmpty() && items.all { it.path in selectedPaths }
}