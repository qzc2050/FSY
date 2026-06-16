package com.raydose.netshield.data

import android.content.Context
import android.net.Uri
import android.os.Environment
import com.raydose.netshield.model.AlbumSettings
import com.raydose.netshield.model.FileStorageLocation
import java.io.File
import java.util.Locale

class AlbumImageRepository(context: Context) {
    private val appContext = context.applicationContext

    fun albumStorageDirectory(): File {
        val dir = File(appContext.filesDir, "album")
        if (!dir.exists()) {
            dir.mkdirs()
        }
        return dir
    }

    fun defaultLocalPicturesDirectory(): File {
        val publicDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES)
        if (publicDir != null && publicDir.exists() && publicDir.isDirectory) {
            return publicDir
        }
        return File(Environment.getExternalStorageDirectory(), "Pictures")
    }

    fun resolveInitialBrowsePath(rootPath: String, preferredDirectory: String): String {
        val root = File(rootPath)
        val candidates = listOf(
            preferredDirectory,
            defaultLocalPicturesDirectory().absolutePath,
            rootPath,
        )
        return candidates.firstOrNull { candidate ->
            candidate.isNotBlank() && isReadableDirectoryUnderRoot(candidate, root)
        } ?: rootPath
    }

    fun buildPathStack(
        rootPath: String,
        rootLabel: String,
        targetDirectoryPath: String,
    ): List<Pair<String, String>> {
        val rootFile = runCatching { File(rootPath).canonicalFile }.getOrDefault(File(rootPath))
        val targetFile = runCatching { File(targetDirectoryPath).canonicalFile }.getOrDefault(File(targetDirectoryPath))
        if (!targetFile.exists() || !targetFile.isDirectory) {
            return listOf(rootPath to rootLabel)
        }
        if (targetFile == rootFile) {
            return listOf(rootPath to rootLabel)
        }
        if (!targetFile.absolutePath.startsWith(rootFile.absolutePath)) {
            return listOf(rootPath to rootLabel)
        }
        val relative = targetFile.absolutePath
            .removePrefix(rootFile.absolutePath)
            .trimStart(File.separatorChar)
        if (relative.isBlank()) {
            return listOf(rootPath to rootLabel)
        }
        val stack = mutableListOf(rootPath to rootLabel)
        var current = rootFile
        relative.split('/').filter { it.isNotBlank() }.forEach { segment ->
            current = File(current, segment)
            stack += current.absolutePath to segment
        }
        return stack
    }

    fun isSelectedImageAvailable(uriString: String): Boolean {
        if (uriString.isBlank()) return false
        return runCatching {
            when {
                uriString.startsWith("file:") -> {
                    val path = Uri.parse(uriString).path ?: return@runCatching false
                    File(path).let { it.isFile && it.exists() && it.canRead() }
                }
                uriString.startsWith("/") -> {
                    File(uriString).let { it.isFile && it.exists() && it.canRead() }
                }
                uriString.startsWith("content:") -> {
                    appContext.contentResolver.openInputStream(Uri.parse(uriString))?.use { true } ?: false
                }
                else -> false
            }
        }.getOrDefault(false)
    }

    fun importImage(
        fileManagerRepository: FileManagerRepository,
        sourceLocation: FileStorageLocation,
        sourcePath: String,
    ): Result<String> = runCatching {
        val targetDir = albumStorageDirectory()
        val copiedPath = fileManagerRepository.copyItem(
            sourceLocation = sourceLocation,
            sourcePath = sourcePath,
            targetLocation = FileStorageLocation.Local,
            targetDirectoryPath = targetDir.absolutePath,
        ).getOrThrow()
        File(copiedPath).toURI().toString()
    }

    fun settingsForSelectedImage(
        current: AlbumSettings,
        importedUri: String,
        pickerStorage: FileStorageLocation,
        pickerDirectory: String,
        sourcePath: String,
    ): AlbumSettings {
        return current.copy(
            selectedImageUri = importedUri,
            lastPickerStorage = pickerStorage,
            lastPickerDirectory = pickerDirectory,
            lastSelectedSourcePath = sourcePath,
        )
    }

    companion object {
        val ImageExtensions = setOf("jpg", "jpeg", "png", "webp", "gif", "bmp")
    }
}

fun isAlbumImageFile(name: String): Boolean {
    val ext = name.substringAfterLast('.', "").lowercase(Locale.US)
    return ext in AlbumImageRepository.ImageExtensions
}

private fun isReadableDirectoryUnderRoot(directoryPath: String, root: File): Boolean {
    val directory = File(directoryPath)
    if (!directory.exists() || !directory.isDirectory || !directory.canRead()) return false
    val rootCanonical = runCatching { root.canonicalFile.absolutePath }.getOrDefault(root.absolutePath)
    val dirCanonical = runCatching { directory.canonicalFile.absolutePath }.getOrDefault(directory.absolutePath)
    return dirCanonical == rootCanonical || dirCanonical.startsWith("$rootCanonical${File.separator}")
}
