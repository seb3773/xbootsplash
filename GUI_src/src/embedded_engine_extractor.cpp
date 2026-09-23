#include "embedded_engine_extractor.h"
#include "embedded_engine.h"

#include "zx0_decompress.h"

#include <ntqdir.h>
#include <ntqfile.h>
#include <ntqfileinfo.h>
#include <ntqstringlist.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool ensureDirExists(const TQString &dirPath) {
    TQDir d(dirPath);
    if (d.exists()) return true;

    TQStringList parts = TQStringList::split('/', dirPath);
    TQString cur = dirPath.startsWith("/") ? "/" : "";
    for (TQStringList::Iterator it = parts.begin(); it != parts.end(); ++it) {
        if ((*it).isEmpty()) continue;
        cur += *it + "/";
        TQDir sub(cur);
        if (!sub.exists()) {
            if (!sub.mkdir(cur)) return false;
        }
    }
    return true;
}

TQString EmbeddedEngineExtractor::createEphemeralBuildDir(TQString *errorMsg) {
    uid_t uid = getuid();
    pid_t pid = getpid();
    time_t ts = time(NULL);

    TQString baseDir = TQString("/run/user/%1").arg((unsigned int)uid);
    TQDir base(baseDir);
    if (!base.exists()) {
        baseDir = "/tmp";
    }

    TQString buildDir = TQString("%1/xbs_build_%2_%3")
                        .arg(baseDir)
                        .arg((unsigned int)pid)
                        .arg((unsigned long)ts);

    if (!ensureDirExists(buildDir)) {
        if (errorMsg) *errorMsg = "Failed to create ephemeral RAM build directory: " + buildDir;
        return TQString();
    }
    return buildDir;
}

bool EmbeddedEngineExtractor::extractTo(const TQString &targetDir, TQString *errorMsg) {
    if (targetDir.isEmpty()) {
        if (errorMsg) *errorMsg = "Target extraction directory is empty";
        return false;
    }

    if (!ensureDirExists(targetDir)) {
        if (errorMsg) *errorMsg = "Cannot create target directory: " + targetDir;
        return false;
    }

    unsigned char *decomp_buf = (unsigned char *)malloc(embedded_engine_uncompressed_size);
    if (!decomp_buf) {
        if (errorMsg) *errorMsg = "Out of memory allocating engine decompression buffer";
        return false;
    }

    int dec_len = zx0_decompress_to(embedded_engine_zx0, (int)embedded_engine_compressed_size,
                                    decomp_buf, (int)embedded_engine_uncompressed_size);
    if (dec_len != (int)embedded_engine_uncompressed_size) {
        if (errorMsg) *errorMsg = TQString("Engine decompression failed: expected %1 bytes, got %2")
                                  .arg(embedded_engine_uncompressed_size).arg(dec_len);
        free(decomp_buf);
        return false;
    }

    size_t offset = 0;
    while (offset + sizeof(uint16_t) <= embedded_engine_uncompressed_size) {
        uint16_t path_len = 0;
        memcpy(&path_len, decomp_buf + offset, sizeof(path_len));
        offset += sizeof(path_len);

        if (path_len == 0) {
            // End of archive sentinel
            break;
        }

        if (offset + path_len + sizeof(uint32_t) > embedded_engine_uncompressed_size) {
            if (errorMsg) *errorMsg = "Corrupted embedded archive (path overflow)";
            free(decomp_buf);
            return false;
        }

        char rel_path[1024];
        if (path_len >= sizeof(rel_path)) {
            if (errorMsg) *errorMsg = "Embedded file path exceeds buffer limit";
            free(decomp_buf);
            return false;
        }
        memcpy(rel_path, decomp_buf + offset, path_len);
        rel_path[path_len] = '\0';
        offset += path_len;

        uint32_t file_len = 0;
        memcpy(&file_len, decomp_buf + offset, sizeof(file_len));
        offset += sizeof(file_len);

        if (offset + file_len > embedded_engine_uncompressed_size) {
            if (errorMsg) *errorMsg = "Corrupted embedded archive (file data overflow)";
            free(decomp_buf);
            return false;
        }

        const unsigned char *file_data = decomp_buf + offset;
        offset += file_len;

        TQString fullPath = targetDir + "/" + TQString::fromLatin1(rel_path);
        TQFileInfo fi(fullPath);
        if (!ensureDirExists(fi.dirPath())) {
            if (errorMsg) *errorMsg = "Failed to create directory: " + fi.dirPath();
            free(decomp_buf);
            return false;
        }

        TQFile outFile(fullPath);
        if (!outFile.open(IO_WriteOnly | IO_Truncate)) {
            if (errorMsg) *errorMsg = "Failed to write file: " + fullPath;
            free(decomp_buf);
            return false;
        }

        if (file_len > 0) {
            TQ_LONG written = outFile.writeBlock((const char *)file_data, (TQ_ULONG)file_len);
            if (written != (TQ_LONG)file_len) {
                if (errorMsg) *errorMsg = "Incomplete write for: " + fullPath;
                outFile.close();
                free(decomp_buf);
                return false;
            }
        }
        outFile.close();
    }

    free(decomp_buf);
    return true;
}

void EmbeddedEngineExtractor::cleanupBuildDir(const TQString &targetDir) {
    if (targetDir.isEmpty()) return;

    // Safety guard: only delete if path is in /run/user/ or /tmp/ and has xbs_build_
    if ((targetDir.startsWith("/run/user/") || targetDir.startsWith("/tmp/")) &&
        targetDir.contains("xbs_build_")) {
        TQString cmd = TQString("rm -rf \"%1\"").arg(targetDir);
        system(cmd.latin1());
    }
}
