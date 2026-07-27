package com.raydose.raylink.ui.files

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.raydose.raylink.R
import com.raydose.raylink.data.FileManagerRepository
import com.raydose.raylink.model.FileListItem
import com.raydose.raylink.model.FileManagerUiState
import com.raydose.raylink.model.FileStorageLocation
import com.raydose.raylink.model.PendingFileTransfer
import com.raydose.raylink.ui.tr
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

class FileManagerViewModel(application: Application) : AndroidViewModel(application) {
    private val app get() = getApplication<Application>()
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
                        message = if (location == FileStorageLocation.Usb) {
                            app.tr(R.string.files_grant_usb_first)
                        } else {
                            app.tr(R.string.storage_not_available)
                        },
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
            _uiState.update { it.copy(message = app.tr(R.string.files_select_first)) }
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
                        failedCount == 0 -> app.tr(R.string.files_deleted_count, successCount)
                        successCount == 0 -> app.tr(R.string.files_delete_failed)
                        else -> app.tr(R.string.files_delete_partial, successCount, failedCount)
                    },
                )
            }
        }
    }

    fun stageCopySelected() {
        val items = _uiState.value.items.filter { it.path in _uiState.value.selectedPaths }
        val first = items.firstOrNull() ?: run {
            _uiState.update { it.copy(message = app.tr(R.string.files_select_first)) }
            return
        }
        if (items.size > 1) {
            stageCopy(first)
            _uiState.update {
                it.copy(message = app.tr(R.string.files_copy_clipboard_multi, first.name, items.size))
            }
        } else {
            stageCopy(first)
        }
    }

    fun stageMoveSelected() {
        val items = _uiState.value.items.filter { it.path in _uiState.value.selectedPaths }
        val first = items.firstOrNull() ?: run {
            _uiState.update { it.copy(message = app.tr(R.string.files_select_first)) }
            return
        }
        if (items.size > 1) {
            stageMove(first)
            _uiState.update {
                it.copy(message = app.tr(R.string.files_move_select_multi, first.name, items.size))
            }
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
                .onSuccess { _uiState.update { state -> state.copy(message = app.tr(R.string.files_copy_success)) } }
                .onFailure { ex ->
                    _uiState.update { state ->
                        state.copy(message = app.tr(R.string.files_copy_failed, ex.message ?: app.tr(R.string.error_unknown)))
                    }
                }
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
                .onSuccess { _uiState.update { state -> state.copy(message = app.tr(R.string.files_move_success)) } }
                .onFailure { ex ->
                    _uiState.update { state ->
                        state.copy(message = app.tr(R.string.files_move_failed, ex.message ?: app.tr(R.string.error_unknown)))
                    }
                }
        }
    }

    fun deleteItem(item: FileListItem) {
        performFileAction {
            repository.deleteItem(item.storageLocation, item.path)
                .onSuccess { _uiState.update { state -> state.copy(message = app.tr(R.string.files_delete_success)) } }
                .onFailure { ex ->
                    _uiState.update { state ->
                        state.copy(message = app.tr(R.string.files_delete_failed_ex, ex.message ?: app.tr(R.string.error_unknown)))
                    }
                }
        }
    }

    fun createFolder(name: String) {
        performFileAction {
            repository.createFolder(_uiState.value.storageLocation, _uiState.value.currentPath, name)
                .onSuccess { _uiState.update { state -> state.copy(message = app.tr(R.string.files_new_folder_success)) } }
                .onFailure { ex ->
                    _uiState.update { state ->
                        state.copy(message = app.tr(R.string.files_new_folder_failed, ex.message ?: app.tr(R.string.error_unknown)))
                    }
                }
        }
    }

    fun renameItem(item: FileListItem, newName: String) {
        performFileAction {
            repository.renameItem(item.storageLocation, item.path, newName)
                .onSuccess { _uiState.update { state -> state.copy(message = app.tr(R.string.files_rename_success)) } }
                .onFailure { ex ->
                    _uiState.update { state ->
                        state.copy(message = app.tr(R.string.files_rename_failed, ex.message ?: app.tr(R.string.error_unknown)))
                    }
                }
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
                message = app.tr(R.string.files_copied_one, item.name),
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
                message = app.tr(R.string.files_move_selected_one, item.name),
            )
        }
    }

    fun clearPendingTransfer() {
        _uiState.update { it.copy(pendingTransfer = null, message = app.tr(R.string.files_clipboard_cleared)) }
    }

    fun pastePendingTransfer() {
        val pending = _uiState.value.pendingTransfer ?: run {
            _uiState.update { it.copy(message = app.tr(R.string.files_clipboard_empty)) }
            return
        }
        val currentPath = _uiState.value.currentPath
        val sourceFile = File(pending.sourcePath)
        if (!sourceFile.exists()) {
            if (pending.sourceLocation == FileStorageLocation.Local) {
                _uiState.update { it.copy(pendingTransfer = null, message = app.tr(R.string.files_source_missing)) }
                return
            }
        }
        if (pending.sourceLocation == FileStorageLocation.Local && pending.isMove && sourceFile.parentFile?.absolutePath == currentPath) {
            _uiState.update { it.copy(message = app.tr(R.string.files_same_directory)) }
            return
        }
        if (pending.sourceLocation == _uiState.value.storageLocation && pending.sourcePath == currentPath) {
            _uiState.update { it.copy(message = app.tr(R.string.files_paste_into_self)) }
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
                            message = if (pending.isMove) {
                                app.tr(R.string.files_move_success)
                            } else {
                                app.tr(R.string.files_copy_success)
                            },
                        )
                    }
                }
                .onFailure { ex ->
                    _uiState.update { state ->
                        state.copy(
                            message = app.tr(R.string.files_paste_failed, ex.message ?: app.tr(R.string.error_unknown)),
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
                    "${app.tr(R.string.storage_usb)} /"
                } else {
                    "${app.tr(R.string.storage_usb)} / ${names.joinToString(" / ")}"
                }
            }
        }
    }
}