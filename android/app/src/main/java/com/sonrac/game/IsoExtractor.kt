package com.sonrac.game

import android.content.Context
import android.net.Uri
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.channels.FileChannel

/**
 * Xbox 360 XDVDFS ISO Extractor for SonRac.
 * Reads game dumps (XGD2 / XGD3 / Standard ISOs) and extracts default.xex and game assets.
 */
object IsoExtractor {

    private const val XDVDFS_SECTOR_SIZE = 2048L
    private val KNOWN_ROOT_OFFSETS = listOf(
        0x10000L,        // Standard XDVDFS partition
        0x2080000L,      // Standard XGD2 ISO offset
        0xFD90000L       // XGD3 ISO partition offset
    )

    fun extractIso(
        context: Context,
        isoUri: Uri,
        destDir: File,
        progressCallback: (progressPercent: Int, status: String) -> Unit
    ): Boolean {
        destDir.mkdirs()

        val pfd = context.contentResolver.openFileDescriptor(isoUri, "r") ?: return false
        val fis = FileInputStream(pfd.fileDescriptor)
        val channel = fis.channel

        try {
            var volumeDescriptorOffset = -1L
            for (offset in KNOWN_ROOT_OFFSETS) {
                if (offset + 0x800 <= channel.size()) {
                    channel.position(offset)
                    val magicBuf = ByteBuffer.allocate(20)
                    channel.read(magicBuf)
                    val magicStr = String(magicBuf.array(), Charsets.US_ASCII)
                    if (magicStr.startsWith("MICROSOFT*XBOX*MEDIA")) {
                        volumeDescriptorOffset = offset
                        break
                    }
                }
            }

            if (volumeDescriptorOffset == -1L) {
                val scanLimit = minOf(channel.size(), 64L * 1024 * 1024)
                var pos = 0L
                val buf = ByteBuffer.allocate(XDVDFS_SECTOR_SIZE.toInt())
                while (pos < scanLimit) {
                    channel.position(pos)
                    buf.clear()
                    val read = channel.read(buf)
                    if (read < 20) break
                    if (String(buf.array().copyOfRange(0, 20), Charsets.US_ASCII).startsWith("MICROSOFT*XBOX*MEDIA")) {
                        volumeDescriptorOffset = pos
                        break
                    }
                    pos += XDVDFS_SECTOR_SIZE
                }
            }

            if (volumeDescriptorOffset == -1L) {
                progressCallback(0, "Error: No se encontro particion valida de Xbox 360 en este ISO.")
                return false
            }

            progressCallback(10, "Particion XDVDFS detectada en offset 0x${volumeDescriptorOffset.toString(16)}. Leyendo tabla de archivos...")

            // Read Root Directory Descriptor (Offset 0x14: Root dir sector (uint32), Offset 0x18: Root dir size (uint32))
            channel.position(volumeDescriptorOffset + 0x14)
            val descBuf = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN)
            channel.read(descBuf)
            descBuf.flip()
            val rootSector = descBuf.int.toLong() and 0xFFFFFFFFL
            val rootSize = descBuf.int.toLong() and 0xFFFFFFFFL

            val rootOffset = volumeDescriptorOffset + (rootSector * XDVDFS_SECTOR_SIZE)
            progressCallback(25, "Extrayendo archivos del juego...")

            extractDirectory(channel, volumeDescriptorOffset, rootOffset, rootSize, destDir, progressCallback)

            progressCallback(100, "¡Extraccion completada con exito! Archivos listos.")
            return true

        } catch (e: Exception) {
            progressCallback(0, "Fallo al procesar ISO: ${e.message}")
            return false
        } finally {
            channel.close()
            fis.close()
            pfd.close()
        }
    }

    private fun extractDirectory(
        channel: FileChannel,
        partitionBase: Long,
        dirOffset: Long,
        dirSize: Long,
        destDir: File,
        progress: (Int, String) -> Unit
    ) {
        if (dirSize <= 0) return
        val dirBuf = ByteBuffer.allocate(dirSize.toInt()).order(ByteOrder.LITTLE_ENDIAN)
        channel.position(dirOffset)
        channel.read(dirBuf)
        val dirData = dirBuf.array()

        var pos = 0
        while (pos + 14 <= dirData.size) {
            val buf = ByteBuffer.wrap(dirData, pos, dirData.size - pos).order(ByteOrder.LITTLE_ENDIAN)
            val leftSubTree = buf.short.toInt() and 0xFFFF
            val rightSubTree = buf.short.toInt() and 0xFFFF
            val sector = buf.int.toLong() and 0xFFFFFFFFL
            val fileSize = buf.int.toLong() and 0xFFFFFFFFL
            val attributes = buf.get().toInt() and 0xFF
            val nameLen = buf.get().toInt() and 0xFF

            if (nameLen == 0 && sector == 0L) break
            if (pos + 14 + nameLen > dirData.size) break

            val nameBytes = ByteArray(nameLen)
            buf.get(nameBytes)
            val name = String(nameBytes, Charsets.UTF_8)
            val isDirectory = (attributes and 0x10) != 0

            val targetFile = File(destDir, name)

            if (isDirectory) {
                targetFile.mkdirs()
                val subDirOffset = partitionBase + (sector * XDVDFS_SECTOR_SIZE)
                extractDirectory(channel, partitionBase, subDirOffset, fileSize, targetFile, progress)
            } else {
                progress(50, "Extrayendo: $name (${fileSize / 1024} KB)")
                val fileOffset = partitionBase + (sector * XDVDFS_SECTOR_SIZE)
                channel.position(fileOffset)

                FileOutputStream(targetFile).use { fos ->
                    var remaining = fileSize
                    val chunk = ByteBuffer.allocate(64 * 1024)
                    while (remaining > 0) {
                        chunk.clear()
                        val toRead = minOf(remaining, chunk.capacity().toLong()).toInt()
                        chunk.limit(toRead)
                        val bytesRead = channel.read(chunk)
                        if (bytesRead <= 0) break
                        fos.write(chunk.array(), 0, bytesRead)
                        remaining -= bytesRead
                    }
                }
            }

            // Entries in XDVDFS directory table are padded to 4-byte boundaries
            val entrySize = (14 + nameLen + 3) and -4
            pos += entrySize
        }
    }
}
