package com.raydose.netshield.ui.files

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.raydose.netshield.data.FileManagerRepository
import com.raydose.netshield.model.FileListItem
import com.raydose.netshield.model.FileManagerUiState
import com.raydose.netshield.model.FileStorageLocation
import com.raydose.netshield.model.PendingFileTransfer
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

class FileManagerViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = FileManagerRepository(application)
    private val _uiState = MutableStateFlow(FileManagerUiState(isLoading = true))
    val uiState: StateFlow<FileManagerUiState> = _uiState.asStateFlow()
    private val directoryStack = mutableListOf<Pair<String, String>>()

    init {
        switchStorage(FileStorageLocation.Local)
    }

    fun switchStorage(location: FileStorageLocation) {
        viewModelScope.launch {
            _uiState.update {
                it.copy(
                    storageLocation = location,
                    isLoading = true,
                    searchQuery = "",
                    message = null,
                    requiresUsbAccess = false,
                    selectionMode = false,
                    selectedPaths = emptySet(),
                )
            }
            val root = withContext(Dispatchers.IO) { repository.resolveRoot(location) }
            if (root == null) {
                _uiState.update {
                    it.copy(
                        rootPath = "",
                        currentPath = "",
                        currentPathLabel = "",
                        items = emptyList(),
                        isLoading = false,
                        requiresUsbAccess = location == FileStorageLocation.Usb,
                        message = if (location == FileStorageLocation.Usb) "请先授权U盘目录" else "未检测到可用存储",
                    )
                }
                return@launch
            }
            directoryStack.clear()
            directoryStack += root.first to root.second
            _uiState.update {
                it.copy(
                    rootPath = root.first,
                    currentPath = root.first,
                    currentPathLabel = buildDisplayPath(location),
                    requiresUsbAccess = false,
                )
            }
            loadItems()
        }
    }

    fun grantUsbTree(uriString: String) {
        repository.saveUsbTreeUri(uriString)
        switchStorage(FileStorageLocation.Usb)
    }

    fun updateSearchQuery(query: String) {
        _uiState.update { it.copy(searchQuery = query, selectionMode = false, selectedPaths = emptySet()) }
        loadItems()
    }

    fun openDirectory(item: FileListItem) {
        if (_uiState.value.selectionMode) return
        if (!item.isDirectory) return
        directoryStack += item.path to item.name
        _uiState.update {
            it.copy(
                currentPath = item.path,
                currentPathLabel = buildDisplayPath(it.storageLocation),
                searchQuery = "",
                selectionMode = false,
                selectedPaths = emptySet(),
            )
        }
        loadItems()
    }

    fun navigateUp() {
        if (directoryStack.size <= 1) return
        directoryStack.removeLastOrNull()
        val target = directoryStack.lastOrNull() ?: return
        _uiState.update {
            it.copy(
                currentPath = target.first,
                currentPathLabel = buildDisplayPath(it.storageLocation),
                selectionMode = false,
                selectedPaths = emptySet(),
            )
        }
        loadItems()
    }

    fun beginSelection(item: FileListItem) {
        _uiState.update {
            it.copy(
                selectionMode = true,
                selectedPaths = it.selectedPaths + item.path,
            )
        }
    }

    fun toggleItemSelection(item: FileListItem) {
        _uiState.update { state ->
            val selected = if (item.path in state.selectedPaths) {
                state.selectedPaths - item.path
            } else {
                state.selectedPaths + item.path
            }
            state.copy(
                selectedPaths = selected,
                selectionMode = selected.isNotEmpty() || state.selectionMode,
            )
        }
    }

    fun selectAllVisible() {
        _uiState.update { state ->
            state.copy(
                selectionMode = true,
                selectedPaths = state.items.map { it.path }.toSet(),
            )
        }
    }

    fun clearSelection() {
        _uiState.update { it.copy(selectedPaths = emptySet()) }
    }

    fun exitSelectionMode() {
        _uiState.update { it.copy(selectionMode = false, selectedPaths = emptySet()) }
    }

    fun deleteSelectedItems() {
        val items = _uiState.value.items.filter { it.path in _uiState.value.selectedPaths }
        if (items.isEmpty()) {
            _uiState.update { it.copy(message = "请先勾选文件") }
            return
        }
        performFileAction {
            var successCount = 0
            var failedCount = 0
            items.forEach { item ->
                repository.deleteItem(item.storageLocation, item.path)
                    .onSuccess { successCount++ }
                    .onFailure { failedCount++ }
            }
            _uiState.update { state ->
                state.copy(
                    selectionMode = false,
                    selectedPaths = emptySet(),
                    message = when {
                        failedCount == 0 -> "已删除 $successCount 项"
                        successCount == 0 -> "删除失败"
                        else -> "成功 $successCount 项，失败 $failedCount 项"
                    },
                )
            }
        }
    }

    fun stageCopySelected() {
        val items = _uiState.value.items.filter { it.path in _uiState.value.selectedPaths }
        val first = items.firstOrNull() ?: run {
            _uiState.update { it.copy(message = "请先勾选文件") }
            return
        }
        if (items.size > 1) {
            stageCopy(first)
            _uiState.update { it.copy(message = "已复制首项到剪贴板：${first.name}（共 ${items.size} 项，粘贴需逐次操作）") }
        } else {
            stageCopy(first)
        }
    }

    fun stageMoveSelected() {
        val items = _uiState.value.items.filter { it.path in _uiState.value.selectedPaths }
        val first = items.firstOrNull() ?: run {
            _uiState.update { it.copy(message = "请先勾选文件") }
            return
        }
        if (items.size > 1) {
            stageMove(first)
            _uiState.update { it.copy(message = "已选择移动首项：${first.name}（共 ${items.size} 项，移动需逐次操作）") }
        } else {
            stageMove(first)
        }
    }

    fun copyItem(sourcePath: String, targetDirectoryPath: String) {
        performFileAction {
            repository.copyItem(
                sourceLocation = _uiState.value.storageLocation,
                sourcePath = sourcePath,
                targetLocation = _uiState.value.storageLocation,
                targetDirectoryPath = targetDirectoryPath,
            )
                .onSuccess { _uiState.update { state -> state.copy(message = "复制成功") } }
                .onFailure { ex -> _uiState.update { state -> state.copy(message = "复制失败：${ex.message ?: "未知错误"}") } }
        }
    }

    fun moveItem(sourcePath: String, targetDirectoryPath: String) {
        performFileAction {
            repository.moveItem(
                sourceLocation = _uiState.value.storageLocation,
                sourcePath = sourcePath,
                targetLocation = _uiState.value.storageLocation,
                targetDirectoryPath = targetDirectoryPath,
            )
                .onSuccess { _uiState.update { state -> state.copy(message = "移动成功") } }
                .onFailure { ex -> _uiState.update { state -> state.copy(message = "移动失败：${ex.message ?: "未知错误"}") } }
        }
    }

    fun deleteItem(item: FileListItem) {
        performFileAction {
            repository.deleteItem(item.storageLocation, item.path)
                .onSuccess { _uiState.update { state -> state.copy(message = "删除成功") } }
                .onFailure { ex -> _uiState.update { state -> state.copy(message = "删除失败：${ex.message ?: "未知错误"}") } }
        }
    }

    fun createFolder(name: String) {
        performFileAction {
            repository.createFolder(_uiState.value.storageLocation, _uiState.value.currentPath, name)
                .onSuccess { _uiState.update { state -> state.copy(message = "新建文件夹成功") } }
                .onFailure { ex -> _uiState.update { state -> state.copy(message = "新建文件夹失败：${ex.message ?: "未知错误"}") } }
        }
    }

    fun renameItem(item: FileListItem, newName: String) {
        performFileAction {
            repository.renameItem(item.storageLocation, item.path, newName)
                .onSuccess { _uiState.update { state -> state.copy(message = "重命名成功") } }
                .onFailure { ex -> _uiState.update { state -> state.copy(message = "重命名失败：${ex.message ?: "未知错误"}") } }
        }
    }

    fun showHint(message: String) {
        _uiState.update { it.copy(message = message) }
    }

    fun stageCopy(item: FileListItem) {
        _uiState.update {
            it.copy(
                pendingTransfer = PendingFileTransfer(
                    sourcePath = item.path,
                    sourceName = item.name,
                    isMove = false,
                    sourceLocation = item.storageLocation,
                ),
                message = "已复制到剪贴板：${item.name}",
            )
        }
    }

    fun stageMove(item: FileListItem) {
        _uiState.update {
            it.copy(
                pendingTransfer = PendingFileTransfer(
                    sourcePath = item.path,
                    sourceName = item.name,
                    isMove = true,
                    sourceLocation = item.storageLocation,
                ),
                message = "已选择移动：${item.name}",
            )
        }
    }

    fun clearPendingTransfer() {
        _uiState.update { it.copy(pendingTransfer = null, message = "已清空剪贴板") }
    }

    fun pastePendingTransfer() {
        val pending = _uiState.value.pendingTransfer ?: run {
            _uiState.update { it.copy(message = "剪贴板为空") }
            return
        }
        val currentPath = _uiState.value.currentPath
        val sourceFile = File(pending.sourcePath)
        if (!sourceFile.exists()) {
            if (pending.sourceLocation == FileStorageLocation.Local) {
                _uiState.update { it.copy(pendingTransfer = null, message = "源文件不存在，已清空剪贴板") }
                return
            }
        }
        if (pending.sourceLocation == FileStorageLocation.Local && pending.isMove && sourceFile.parentFile?.absolutePath == currentPath) {
            _uiState.update { it.copy(message = "当前已是目标目录") }
            return
        }
        if (pending.sourceLocation == _uiState.value.storageLocation && pending.sourcePath == currentPath) {
            _uiState.update { it.copy(message = "不能粘贴到自身") }
            return
        }
        performFileAction {
            val result = if (pending.isMove) {
                repository.moveItem(pending.sourceLocation, pending.sourcePath, _uiState.value.storageLocation, currentPath)
            } else {
                repository.copyItem(pending.sourceLocation, pending.sourcePath, _uiState.value.storageLocation, currentPath)
            }
            result
                .onSuccess {
                    _uiState.update { state ->
                        state.copy(
                            pendingTransfer = null,
                            message = if (pending.isMove) "移动成功" else "复制成功",
                        )
                    }
                }
                .onFailure { ex ->
                    _uiState.update { state ->
                        state.copy(
                            message = "粘贴失败：${ex.message ?: "未知错误"}",
                        )
                    }
                }
        }
    }

    private fun performFileAction(action: suspend () -> Unit) {
        viewModelScope.launch {
            _uiState.update { it.copy(isLoading = true) }
            withContext(Dispatchers.IO) {
                action()
            }
            loadItems()
        }
    }

    private fun loadItems() {
        viewModelScope.launch {
            val current = _uiState.value.currentPath
            if (current.isBlank()) {
                _uiState.update { it.copy(items = emptyList(), isLoading = false) }
                return@launch
            }
            _uiState.update { it.copy(isLoading = true) }
            val query = _uiState.value.searchQuery
            val items = withContext(Dispatchers.IO) {
                repository.listItems(_uiState.value.storageLocation, current, query)
            }
            _uiState.update {
                it.copy(items = items, isLoading = false)
            }
        }
    }

    private fun buildDisplayPath(location: FileStorageLocation): String {
        return when (location) {
            FileStorageLocation.Local -> directoryStack.lastOrNull()?.first.orEmpty()
            FileStorageLocation.Usb -> {
                val names = directoryStack.mapIndexedNotNull { index, entry ->
                    if (index == 0) null else entry.second
                }
                if (names.isEmpty()) {
                    "U盘 /"
                } else {
                    "U盘 / ${names.joinToString(" / ")}"
                }
            }
        }
    }
}