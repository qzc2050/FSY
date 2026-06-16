package com.raydose.netshield.data

import android.content.Context
import android.net.Uri
import android.os.Environment
import android.os.ParcelFileDescriptor
import com.raydose.netshield.model.FileListItem
import com.raydose.netshield.model.FileStorageLocation
import androidx.documentfile.provider.DocumentFile
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.net.URLConnection
import java.util.Locale

class FileManagerRepository(context: Context) {
    private val appContext = context.applicationContext
    private val settingsRepository = HostSettingsRepository(appContext)

    fun resolveRoot(location: FileStorageLocation): Pair<String, String>? {
        return when (location) {
            FileStorageLocation.Local -> localRoot().let { it.absolutePath to it.absolutePath }
            FileStorageLocation.Usb -> rootUsbRoot() ?: usbRoot()
        }
    }

    fun listItems(location: FileStorageLocation, directoryPath: String, query: String): List<FileListItem> {
        val keyword = query.trim().lowercase(Locale.getDefault())
        val items = when (location) {
            FileStorageLocation.Local -> listLocalItems(directoryPath, keyword)
            FileStorageLocation.Usb -> listUsbItems(directoryPath, keyword)
        }
        return items.sortedWith(
            compareByDescending<FileListItem> { it.isDirectory }
                .thenBy { it.name.lowercase(Locale.getDefault()) },
        )
    }

    fun copyItem(
        sourceLocation: FileStorageLocation,
        sourcePath: String,
        targetLocation: FileStorageLocation,
        targetDirectoryPath: String,
    ): Result<String> = runCatching {
        when {
            sourceLocation == FileStorageLocation.Local && targetLocation == FileStorageLocation.Local -> {
                val source = requireLocalFile(sourcePath)
                val targetDir = requireLocalDirectory(targetDirectoryPath)
                val target = uniqueTargetFile(targetDir, source.name)
                if (source.isDirectory) copyDirectoryToLocal(source, target) else copyLocalFileToLocal(source, target)
                target.absolutePath
            }
            sourceLocation == FileStorageLocation.Local && targetLocation == FileStorageLocation.Usb -> {
                val source = requireLocalFile(sourcePath)
                if (isRootUsbPath(targetDirectoryPath)) {
                    copyLocalEntryToRootUsb(source, targetDirectoryPath)
                } else {
                    val targetDir = requireUsbDirectory(targetDirectoryPath)
                    val target = copyLocalEntryToUsb(source, targetDir)
                    target.uri.toString()
                }
            }
            sourceLocation == FileStorageLocation.Usb && targetLocation == FileStorageLocation.Local -> {
                val targetDir = requireLocalDirectory(targetDirectoryPath)
                if (isRootUsbPath(sourcePath)) {
                    copyRootUsbEntryToLocal(sourcePath, targetDir).absolutePath
                } else {
                    val source = requireUsbDocument(sourcePath)
                    val target = copyUsbEntryToLocal(source, targetDir)
                    target.absolutePath
                }
            }
            else -> {
                if (isRootUsbPath(sourcePath) && isRootUsbPath(targetDirectoryPath)) {
                    copyRootUsbEntryToRootUsb(sourcePath, targetDirectoryPath)
                } else if (isRootUsbPath(sourcePath)) {
                    val targetDir = requireUsbDirectory(targetDirectoryPath)
                    val staged = copyRootUsbEntryToLocal(sourcePath, createTempStagingDir())
                    val result = copyLocalEntryToUsb(staged, targetDir).uri.toString()
                    deleteRecursively(staged)
                    result
                } else if (isRootUsbPath(targetDirectoryPath)) {
                    val source = requireUsbDocument(sourcePath)
                    val staged = copyUsbEntryToLocal(source, createTempStagingDir())
                    val result = copyLocalEntryToRootUsb(staged, targetDirectoryPath)
                    deleteRecursively(staged)
                    result
                } else {
                    val source = requireUsbDocument(sourcePath)
                    val targetDir = requireUsbDirectory(targetDirectoryPath)
                    val target = copyUsbEntryToUsb(source, targetDir)
                    target.uri.toString()
                }
            }
        }
    }

    fun moveItem(
        sourceLocation: FileStorageLocation,
        sourcePath: String,
        targetLocation: FileStorageLocation,
        targetDirectoryPath: String,
    ): Result<String> = runCatching {
        when {
            sourceLocation == FileStorageLocation.Local && targetLocation == FileStorageLocation.Local -> {
                val source = requireLocalFile(sourcePath)
                val targetDir = requireLocalDirectory(targetDirectoryPath)
                val target = uniqueTargetFile(targetDir, source.name)
                if (!source.renameTo(target)) {
                    if (source.isDirectory) copyDirectoryToLocal(source, target) else copyLocalFileToLocal(source, target)
                    deleteRecursively(source)
                }
                target.absolutePath
            }
            else -> {
                val copied = copyItem(sourceLocation, sourcePath, targetLocation, targetDirectoryPath).getOrThrow()
                deleteItem(sourceLocation, sourcePath).getOrThrow()
                copied
            }
        }
    }

    fun deleteItem(location: FileStorageLocation, path: String): Result<Unit> = runCatching {
        when (location) {
            FileStorageLocation.Local -> {
                val file = File(path)
                if (!file.exists()) return@runCatching
                deleteRecursively(file)
            }
            FileStorageLocation.Usb -> {
                if (isRootUsbPath(path)) {
                    deleteRootPath(path)
                } else {
                    val file = requireUsbDocument(path)
                    require(file.delete()) { "删除失败: ${file.name ?: path}" }
                }
            }
        }
    }

    fun createFolder(location: FileStorageLocation, parentDirectoryPath: String, folderName: String): Result<String> = runCatching {
        val safeName = folderName.trim()
        require(safeName.isNotEmpty()) { "文件夹名称不能为空" }
        when (location) {
            FileStorageLocation.Local -> {
                val parent = requireLocalDirectory(parentDirectoryPath)
                val target = uniqueTargetFile(parent, safeName)
                require(target.mkdirs()) { "创建文件夹失败" }
                target.absolutePath
            }
            FileStorageLocation.Usb -> {
                if (isRootUsbPath(parentDirectoryPath)) {
                    createRootUsbFolder(parentDirectoryPath, safeName)
                } else {
                    val parent = requireUsbDirectory(parentDirectoryPath)
                    val target = parent.createDirectory(uniqueUsbName(parent, safeName, null))
                    requireNotNull(target) { "创建文件夹失败" }
                    target.uri.toString()
                }
            }
        }
    }

    fun renameItem(location: FileStorageLocation, path: String, newName: String): Result<String> = runCatching {
        val safeName = newName.trim()
        require(safeName.isNotEmpty()) { "名称不能为空" }
        when (location) {
            FileStorageLocation.Local -> {
                val source = requireLocalFile(path)
                val parent = source.parentFile ?: error("父目录不存在")
                require(parent.canWrite()) { "当前目录不可写" }
                val target = if (safeName == source.name) source else uniqueTargetFile(parent, safeName)
                if (target.absolutePath != source.absolutePath) {
                    require(source.renameTo(target)) { "重命名失败" }
                }
                target.absolutePath
            }
            FileStorageLocation.Usb -> {
                if (isRootUsbPath(path)) {
                    renameRootUsbPath(path, safeName)
                } else {
                    val source = requireUsbDocument(path)
                    val newSafeName = uniqueUsbName(requireUsbParent(path), safeName, source.uri.toString())
                    require(source.renameTo(newSafeName)) { "重命名失败" }
                    requireUsbDocument(path).uri.toString()
                }
            }
        }
    }

    fun saveUsbTreeUri(uri: String) {
        settingsRepository.saveUsbTreeUri(uri)
    }

    fun loadUsbTreeUri(): String = settingsRepository.loadUsbTreeUri()

    fun listDirectoryEntries(location: FileStorageLocation, directoryPath: String): List<FileListItem> =
        listItems(location, directoryPath, "").filter { it.isDirectory }

    fun writeTextFile(
        location: FileStorageLocation,
        directoryPath: String,
        fileName: String,
        content: String,
    ): Result<String> = runCatching {
        val safeName = fileName.trim().ifBlank { "probe_export.csv" }
        val bytes = content.toByteArray(Charsets.UTF_8)
        require(bytes.isNotEmpty()) { "导出内容为空" }
        when (location) {
            FileStorageLocation.Local -> {
                val dir = requireLocalDirectory(directoryPath)
                val target = uniqueTargetFile(dir, safeName)
                writeBytesToLocalFile(target, bytes)
                require(target.length() > 0L) { "写入后文件为空" }
                target.absolutePath
            }
            FileStorageLocation.Usb -> {
                val savedPath = if (isRootUsbPath(directoryPath)) {
                    writeTextToRootUsb(directoryPath, safeName, bytes)
                } else {
                    val dir = requireUsbDirectory(directoryPath)
                    writeBytesToUsbDirectory(dir, safeName, bytes)
                }
                flushUsbStorage(directoryPath)
                savedPath
            }
        }
    }

    private fun writeBytesToLocalFile(target: File, bytes: ByteArray) {
        target.parentFile?.let { parent -> if (!parent.exists()) parent.mkdirs() }
        FileOutputStream(target).use { output ->
            output.write(bytes)
            output.flush()
            output.fd.sync()
        }
    }

    private fun writeBytesToUsbDirectory(dir: DocumentFile, fileName: String, bytes: ByteArray): String {
        val created = dir.createFile("text/csv", uniqueUsbName(dir, fileName, null))
            ?: error("创建文件失败")
        val temp = File.createTempFile("probe_export_", ".csv", appContext.cacheDir)
        try {
            writeBytesToLocalFile(temp, bytes)
            require(temp.length() > 0L) { "临时文件为空" }
            val pfd = appContext.contentResolver.openFileDescriptor(created.uri, "wt")
                ?: error("打开输出流失败")
            pfd.use { descriptor ->
                FileOutputStream(descriptor.fileDescriptor).use { stream ->
                    FileInputStream(temp).use { input -> input.copyTo(stream) }
                    stream.flush()
                    descriptor.fileDescriptor.sync()
                }
            }
            if (created.length() <= 0L) {
                error("写入后文件为空")
            }
            return created.uri.toString()
        } finally {
            temp.delete()
        }
    }

    /** 将 U 盘缓存刷入物理设备，避免导出后立即拔盘看到 0 字节。 */
    private fun flushUsbStorage(directoryPath: String) {
        if (!RootShell.isAvailable()) return
        if (isRootUsbPath(directoryPath)) {
            val mount = usbMountFromPath(directoryPath)
            RootShell.run("sync ${RootShell.quote(mount)} 2>/dev/null; sync")
        } else {
            RootShell.run("sync")
        }
    }

    private fun usbMountFromPath(directoryPath: String): String {
        if (!directoryPath.startsWith("/storage/")) return directoryPath
        val volume = directoryPath.removePrefix("/storage/").substringBefore('/')
        return if (volume.isBlank()) directoryPath else "/storage/$volume"
    }

    private fun localRoot(): File {
        val candidate = Environment.getExternalStorageDirectory()
        if (candidate != null && candidate.exists() && candidate.isDirectory && candidate.canRead()) {
            return candidate
        }
        return appContext.filesDir
    }

    private fun usbRoot(): Pair<String, String>? {
        val uriString = settingsRepository.loadUsbTreeUri().ifBlank { return null }
        val document = DocumentFile.fromTreeUri(appContext, Uri.parse(uriString)) ?: return null
        if (!document.exists() || !document.isDirectory || !document.canRead()) return null
        return uriString to (document.name ?: "U盘存储")
    }

    private fun rootUsbRoot(): Pair<String, String>? {
        if (!RootShell.isAvailable()) return null
        val result = RootShell.run(
            "for d in /storage/*; do name=\"\$(basename \"\$d\")\"; if [ -d \"\$d\" ] && [ \"\$name\" != \"self\" ] && [ \"\$name\" != \"emulated\" ]; then printf '%s\n' \"\$d\"; fi; done | head -n 1",
        )
        if (!result.isSuccess || result.stdout.isBlank()) return null
        val path = result.stdout.lineSequence().first().trim()
        val dir = File(path)
        if (!dir.exists() || !dir.isDirectory || !dir.canRead()) return null
        return path to (dir.name.ifBlank { "U盘" })
    }

    private fun listLocalItems(directoryPath: String, keyword: String): List<FileListItem> {
        val dir = File(directoryPath)
        if (!dir.exists() || !dir.isDirectory || !dir.canRead()) return emptyList()
        return dir.listFiles()
            .orEmpty()
            .asSequence()
            .filter { file -> keyword.isEmpty() || file.name.lowercase(Locale.getDefault()).contains(keyword) }
            .map { file ->
                FileListItem(
                    path = file.absolutePath,
                    name = file.name,
                    isDirectory = file.isDirectory,
                    sizeBytes = if (file.isFile) file.length().coerceAtLeast(0L) else 0L,
                    modifiedAt = file.lastModified().coerceAtLeast(0L),
                    storageLocation = FileStorageLocation.Local,
                )
            }
            .toList()
    }

    private fun listUsbItems(directoryPath: String, keyword: String): List<FileListItem> {
        return runCatching {
            if (isRootUsbPath(directoryPath)) {
                listRootUsbItems(directoryPath, keyword)
            } else {
                val dir = openUsbDirectory(directoryPath, requireWritable = false)
                    ?: return emptyList()
                dir.listFiles()
                    .asSequence()
                    .filter { file -> keyword.isEmpty() || file.name.orEmpty().lowercase(Locale.getDefault()).contains(keyword) }
                    .map { file ->
                        FileListItem(
                            path = file.uri.toString(),
                            name = file.name.orEmpty().ifBlank { "未命名" },
                            isDirectory = file.isDirectory,
                            sizeBytes = if (file.isFile) file.length().coerceAtLeast(0L) else 0L,
                            modifiedAt = file.lastModified().coerceAtLeast(0L),
                            storageLocation = FileStorageLocation.Usb,
                        )
                    }
                    .toList()
            }
        }.getOrDefault(emptyList())
    }

    private fun listRootUsbItems(directoryPath: String, keyword: String): List<FileListItem> {
        val command = "find ${RootShell.quote(directoryPath)} -mindepth 1 -maxdepth 1 -print"
        val result = RootShell.run(command)
        if (!result.isSuccess || result.stdout.isBlank()) return emptyList()
        return result.stdout.lineSequence()
            .map { it.trim() }
            .filter { it.isNotBlank() }
            .mapNotNull { path ->
                val meta = rootPathMeta(path) ?: return@mapNotNull null
                if (keyword.isNotEmpty() && !meta.name.lowercase(Locale.getDefault()).contains(keyword)) return@mapNotNull null
                FileListItem(
                    path = path,
                    name = meta.name,
                    isDirectory = meta.isDirectory,
                    sizeBytes = meta.sizeBytes,
                    modifiedAt = meta.modifiedAt,
                    storageLocation = FileStorageLocation.Usb,
                )
            }
            .toList()
    }

    private data class RootPathMeta(
        val name: String,
        val isDirectory: Boolean,
        val sizeBytes: Long,
        val modifiedAt: Long,
    )

    private fun rootPathMeta(path: String): RootPathMeta? {
        val command = "if [ -d ${RootShell.quote(path)} ]; then t=d; else t=f; fi; size=\$(stat -c %s ${RootShell.quote(path)} 2>/dev/null || echo 0); mod=\$(stat -c %Y ${RootShell.quote(path)} 2>/dev/null || echo 0); name=\$(basename ${RootShell.quote(path)}); printf '%s\\t%s\\t%s\\t%s' \"\$t\" \"\$size\" \"\$mod\" \"\$name\""
        val result = RootShell.run(command)
        if (!result.isSuccess || result.stdout.isBlank()) return null
        val parts = result.stdout.split('\t')
        if (parts.size < 4) return null
        return RootPathMeta(
            name = parts[3],
            isDirectory = parts[0] == "d",
            sizeBytes = parts[1].toLongOrNull() ?: 0L,
            modifiedAt = parts[2].toLongOrNull() ?: 0L,
        )
    }

    private fun requireLocalFile(path: String): File {
        val file = File(path)
        require(file.exists()) { "源文件不存在" }
        return file
    }

    private fun requireLocalDirectory(path: String): File {
        val file = File(path)
        require(file.exists() && file.isDirectory && file.canWrite()) { "目标目录不可写" }
        return file
    }

    private fun requireUsbDirectory(path: String): DocumentFile =
        openUsbDirectory(path, requireWritable = true)
            ?: error("U盘目录不可写")

    private fun openUsbDirectory(path: String, requireWritable: Boolean): DocumentFile? {
        if (isRootUsbPath(path)) return null
        val file = DocumentFile.fromTreeUri(appContext, Uri.parse(path))
            ?: DocumentFile.fromSingleUri(appContext, Uri.parse(path))
            ?: return null
        if (!file.exists() || !file.isDirectory) return null
        if (requireWritable) {
            if (!file.canWrite()) return null
        } else if (!file.canRead()) {
            return null
        }
        return file
    }

    private fun requireUsbDocument(path: String): DocumentFile {
        require(!isRootUsbPath(path)) { "root U盘文件不应走SAF" }
        val file = DocumentFile.fromSingleUri(appContext, Uri.parse(path))
            ?: DocumentFile.fromTreeUri(appContext, Uri.parse(path))
        requireNotNull(file) { "U盘文件不可访问" }
        require(file.exists()) { "源文件不存在" }
        return file
    }

    private fun requireUsbParent(path: String): DocumentFile {
        val treeRoot = settingsRepository.loadUsbTreeUri().ifBlank { error("未授权U盘目录") }
        val root = requireUsbDirectory(treeRoot)
        return findUsbParent(root, path) ?: error("无法定位父目录")
    }

    private fun findUsbParent(parent: DocumentFile, childUri: String): DocumentFile? {
        parent.listFiles().forEach { child ->
            if (child.uri.toString() == childUri) return parent
            if (child.isDirectory) {
                val nested = findUsbParent(child, childUri)
                if (nested != null) return nested
            }
        }
        return null
    }

    private fun copyDirectoryToLocal(source: File, target: File) {
        if (!target.exists() && !target.mkdirs()) error("创建目录失败: ${target.absolutePath}")
        source.listFiles().orEmpty().forEach { child ->
            val childTarget = File(target, child.name)
            if (child.isDirectory) copyDirectoryToLocal(child, childTarget) else copyLocalFileToLocal(child, childTarget)
        }
    }

    private fun copyLocalFileToLocal(source: File, target: File) {
        target.parentFile?.let { if (!it.exists()) it.mkdirs() }
        FileInputStream(source).use { input ->
            FileOutputStream(target).use { output -> input.copyTo(output) }
        }
    }

    private fun copyLocalEntryToUsb(source: File, targetDir: DocumentFile): DocumentFile {
        return if (source.isDirectory) {
            val createdDir = targetDir.createDirectory(uniqueUsbName(targetDir, source.name, null))
                ?: error("创建U盘目录失败")
            source.listFiles().orEmpty().forEach { child -> copyLocalEntryToUsb(child, createdDir) }
            createdDir
        } else {
            val mimeType = URLConnection.guessContentTypeFromName(source.name) ?: "application/octet-stream"
            val createdFile = targetDir.createFile(mimeType, uniqueUsbName(targetDir, source.name, null))
                ?: error("创建U盘文件失败")
            appContext.contentResolver.openOutputStream(createdFile.uri)?.use { output ->
                FileInputStream(source).use { input -> input.copyTo(output) }
            } ?: error("打开U盘输出流失败")
            createdFile
        }
    }

    private fun writeTextToRootUsb(
        directoryPath: String,
        fileName: String,
        bytes: ByteArray,
    ): String {
        val temp = File.createTempFile("probe_export_", ".csv", appContext.cacheDir)
        try {
            writeBytesToLocalFile(temp, bytes)
            require(temp.length() > 0L) { "临时文件为空" }
            val target = uniqueRootTargetPath(directoryPath, fileName)
            val result = RootShell.run(
                "cp ${RootShell.quote(temp.absolutePath)} ${RootShell.quote(target)}",
            )
            require(result.isSuccess) { result.stderr.ifBlank { "写入U盘失败" } }
            val sizeCheck = RootShell.run("stat -c %s ${RootShell.quote(target)}")
            val size = sizeCheck.stdout.trim().toLongOrNull() ?: 0L
            require(size > 0L) { "写入后文件为空" }
            return target
        } finally {
            temp.delete()
        }
    }

    private fun copyLocalEntryToRootUsb(source: File, targetDirectoryPath: String): String {
        val target = uniqueRootTargetPath(targetDirectoryPath, source.name)
        val command = "cp -r ${RootShell.quote(source.absolutePath)} ${RootShell.quote(target)}"
        val result = RootShell.run(command)
        require(result.isSuccess) { result.stderr.ifBlank { "复制到U盘失败" } }
        return target
    }

    private fun copyUsbEntryToLocal(source: DocumentFile, targetDir: File): File {
        return if (source.isDirectory) {
            val createdDir = uniqueTargetFile(targetDir, source.name ?: "未命名目录")
            require(createdDir.mkdirs()) { "创建本地目录失败" }
            source.listFiles().forEach { child -> copyUsbEntryToLocal(child, createdDir) }
            createdDir
        } else {
            val createdFile = uniqueTargetFile(targetDir, source.name ?: "未命名文件")
            appContext.contentResolver.openInputStream(source.uri)?.use { input ->
                FileOutputStream(createdFile).use { output -> input.copyTo(output) }
            } ?: error("打开U盘输入流失败")
            createdFile
        }
    }

    private fun copyRootUsbEntryToLocal(sourcePath: String, targetDir: File): File {
        val sourceName = File(sourcePath).name
        val target = uniqueTargetFile(targetDir, sourceName)
        val command = "cp -r ${RootShell.quote(sourcePath)} ${RootShell.quote(target.absolutePath)}"
        val result = RootShell.run(command)
        require(result.isSuccess) { result.stderr.ifBlank { "从U盘复制失败" } }
        return target
    }

    private fun copyUsbEntryToUsb(source: DocumentFile, targetDir: DocumentFile): DocumentFile {
        return if (source.isDirectory) {
            val createdDir = targetDir.createDirectory(uniqueUsbName(targetDir, source.name ?: "未命名目录", null))
                ?: error("创建U盘目录失败")
            source.listFiles().forEach { child -> copyUsbEntryToUsb(child, createdDir) }
            createdDir
        } else {
            val mimeType = source.type ?: "application/octet-stream"
            val createdFile = targetDir.createFile(mimeType, uniqueUsbName(targetDir, source.name ?: "未命名文件", null))
                ?: error("创建U盘文件失败")
            val input = appContext.contentResolver.openInputStream(source.uri) ?: error("打开U盘输入流失败")
            val output = appContext.contentResolver.openOutputStream(createdFile.uri) ?: error("打开U盘输出流失败")
            input.use { inputStream ->
                output.use { outputStream -> inputStream.copyTo(outputStream) }
            }
            createdFile
        }
    }

    private fun copyRootUsbEntryToRootUsb(sourcePath: String, targetDirectoryPath: String): String {
        val target = uniqueRootTargetPath(targetDirectoryPath, File(sourcePath).name)
        val result = RootShell.run("cp -r ${RootShell.quote(sourcePath)} ${RootShell.quote(target)}")
        require(result.isSuccess) { result.stderr.ifBlank { "U盘复制失败" } }
        return target
    }

    private fun createRootUsbFolder(parentDirectoryPath: String, folderName: String): String {
        val target = uniqueRootTargetPath(parentDirectoryPath, folderName)
        val result = RootShell.run("mkdir -p ${RootShell.quote(target)}")
        require(result.isSuccess) { result.stderr.ifBlank { "创建文件夹失败" } }
        return target
    }

    private fun renameRootUsbPath(path: String, newName: String): String {
        val parent = File(path).parentFile?.absolutePath ?: error("父目录不存在")
        val target = uniqueRootTargetPath(parent, newName, selfPath = path)
        if (target == path) return path
        val result = RootShell.run("mv ${RootShell.quote(path)} ${RootShell.quote(target)}")
        require(result.isSuccess) { result.stderr.ifBlank { "重命名失败" } }
        return target
    }

    private fun deleteRootPath(path: String) {
        val result = RootShell.run("rm -rf ${RootShell.quote(path)}")
        require(result.isSuccess) { result.stderr.ifBlank { "删除失败" } }
    }

    private fun uniqueRootTargetPath(parentPath: String, originalName: String, selfPath: String? = null): String {
        val dot = originalName.lastIndexOf('.')
        val base = if (dot > 0) originalName.substring(0, dot) else originalName
        val ext = if (dot > 0) originalName.substring(dot) else ""
        var index = 0
        while (true) {
            val candidateName = if (index == 0) "$base$ext" else "${base}_$index$ext"
            val candidatePath = "$parentPath/$candidateName"
            if (selfPath != null && candidatePath == selfPath) return candidatePath
            val check = RootShell.run("if [ -e ${RootShell.quote(candidatePath)} ]; then echo 1; else echo 0; fi")
            if (check.stdout.trim() == "0") return candidatePath
            index += 1
        }
    }

    private fun isRootUsbPath(path: String): Boolean = path.startsWith("/storage/")

    private fun createTempStagingDir(): File {
        val dir = File(appContext.cacheDir, "usb_stage_${System.currentTimeMillis()}")
        require(dir.mkdirs()) { "创建临时目录失败" }
        return dir
    }

    private fun uniqueUsbName(parent: DocumentFile, originalName: String, selfUri: String?): String {
        val dot = originalName.lastIndexOf('.')
        val base = if (dot > 0) originalName.substring(0, dot) else originalName
        val ext = if (dot > 0) originalName.substring(dot) else ""
        val names = parent.listFiles()
            .filter { it.uri.toString() != selfUri }
            .mapNotNull { it.name }
            .toSet()
        var index = 0
        while (true) {
            val candidate = if (index == 0) "$base$ext" else "${base}_$index$ext"
            if (candidate !in names) return candidate
            index += 1
        }
    }

    private fun uniqueTargetFile(parent: File, originalName: String): File {
        val dot = originalName.lastIndexOf('.')
        val base = if (dot > 0) originalName.substring(0, dot) else originalName
        val ext = if (dot > 0) originalName.substring(dot) else ""
        var index = 0
        while (true) {
            val candidateName = if (index == 0) "$base$ext" else "${base}_$index$ext"
            val candidate = File(parent, candidateName)
            if (!candidate.exists()) return candidate
            index += 1
        }
    }

    private fun deleteRecursively(file: File) {
        if (file.isDirectory) {
            file.listFiles().orEmpty().forEach(::deleteRecursively)
        }
        if (!file.delete()) {
            error("删除失败: ${file.absolutePath}")
        }
    }

    /** 将所选 APK 复制到应用缓存目录，供 [ApkInstallHelper] 安装。 */
    fun stageApkForInstall(location: FileStorageLocation, path: String): Result<File> = runCatching {
        require(path.isNotBlank()) { "未选择安装包" }
        val cacheDir = File(appContext.cacheDir, "apk_update").apply { mkdirs() }
        cacheDir.listFiles()?.forEach(::deleteRecursively)
        val staged = when (location) {
            FileStorageLocation.Local -> {
                val source = requireLocalFile(path)
                require(source.isFile) { "请选择 APK 文件" }
                require(source.name.lowercase(Locale.US).endsWith(".apk")) { "请选择 APK 安装包" }
                val target = File(cacheDir, source.name)
                copyLocalFileToLocal(source, target)
                target
            }
            FileStorageLocation.Usb -> {
                val file = if (isRootUsbPath(path)) {
                    copyRootUsbEntryToLocal(path, cacheDir)
                } else {
                    copyUsbEntryToLocal(requireUsbDocument(path), cacheDir)
                }
                require(file.isFile && file.name.lowercase(Locale.US).endsWith(".apk")) {
                    "请选择 APK 安装包"
                }
                file
            }
        }
        staged
    }
}