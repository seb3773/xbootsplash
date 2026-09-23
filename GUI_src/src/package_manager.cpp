#include "package_manager.h"

#include <ntqfile.h>
#include <ntqtextstream.h>
#include <ntqdir.h>
#include <ntqfileinfo.h>
#include <ntqstringlist.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>

SplashPackageManager::SplashPackageManager(TQObject *parent, const char *name)
    : TQObject(parent, name)
{
}

SplashPackageManager::~SplashPackageManager() {
}

static TQString detectCompressionFromBinary(const TQString &binPath, int displayMode = 0) {
    if (!TQFile::exists(binPath)) return "";
    TQFile fBin(binPath);
    if (!fBin.open(IO_ReadOnly)) return "";
    TQByteArray binData = fBin.readAll();
    fBin.close();

    const uint8_t *raw = (const uint8_t *)binData.data();
    int sz = binData.size();
    int compVal = -1;
    for (int i = 0; i + 24 <= sz; ++i) {
        uint32_t magic = (uint32_t)raw[i] |
                         ((uint32_t)raw[i+1] << 8) |
                         ((uint32_t)raw[i+2] << 16) |
                         ((uint32_t)raw[i+3] << 24);
        if (magic == 0xA7F3B219) {
            compVal = (int)((uint16_t)raw[i+14] | ((uint16_t)raw[i+15] << 8));
            break;
        }
    }

    bool hasZx0Anim = false;
    const char needle[] = "Failed to allocate frames buffer";
    const int needleLen = (int)sizeof(needle) - 1;
    for (int i = 0; i + needleLen <= sz; ++i) {
        if (memcmp(raw + i, needle, needleLen) == 0) {
            hasZx0Anim = true;
            break;
        }
    }

    if (compVal == 6) {
        return "ZX0 (Palette 8-bit)";
    } else if (compVal == 5) {
        return "LZSS (Palette 8-bit)";
    } else if (hasZx0Anim) {
        if (compVal == 1) return "ZX0 Super-pack (RLE Direct)";
        else if (compVal == 0) return "ZX0 Super-pack (RLE XOR)";
        else if (compVal == 2) return "ZX0 Super-pack (Sparse XOR)";
        else return "ZX0 (Super-pack)";
    } else if (compVal == 0) {
        return "RLE XOR (Delta)";
    } else if (compVal == 1) {
        return "RLE Direct (Delta)";
    } else if (compVal == 2) {
        return "Sparse XOR (Delta)";
    } else if (compVal == 3) {
        return "Raw XOR (Delta)";
    } else if (displayMode >= 2) {
        return "LZSS (Palette 8-bit)";
    }
    return "RLE (Delta)";
}

bool SplashPackageManager::exportPackage(const TQString &binaryPath,
                                         const PackageMetadata &meta,
                                         const TQString &outputDir,
                                         TQString &outPackagePath,
                                         TQString &outError)
{
    if (!TQFile::exists(binaryPath)) {
        outError = "The specified binary file does not exist.";
        return false;
    }

    // Create temporary folder
    char tmpTemplate[] = "/tmp/xbs_pkg_gui_XXXXXX";
    char *tmpDir = mkdtemp(tmpTemplate);
    if (!tmpDir) {
        outError = "Unable to create temporary packaging directory.";
        return false;
    }

    TQString tmpPath(tmpDir);

    // 1. Copy binary (both with its original name AND as splash_bin for installer)
    TQFileInfo fiBin(binaryPath);
    TQString binDest = tmpPath + "/" + fiBin.fileName();
    TQString splashBinDest = tmpPath + "/splash_bin";
    TQString cpCmd = "cp \"" + binaryPath + "\" \"" + binDest + "\" && cp \"" + binDest + "\" \"" + splashBinDest + "\"";
    if (system(cpCmd.latin1()) != 0) {
        outError = "Failed to copy binary to temporary directory.";
        system(TQString("rm -rf \"%1\"").arg(tmpPath).latin1());
        return false;
    }

    // 2. Compute SHA256
    TQString shaCmd = TQString("sha256sum \"%1\" | cut -d' ' -f1 > \"%2/sha.txt\"").arg(binaryPath).arg(tmpPath);
    system(shaCmd.latin1());
    TQString sha256 = "";
    TQFile shaFile(tmpPath + "/sha.txt");
    if (shaFile.open(IO_ReadOnly)) {
        TQTextStream ts(&shaFile);
        sha256 = ts.readLine().stripWhiteSpace();
        shaFile.close();
        unlink((tmpPath + "/sha.txt").latin1());
    }

    // 3. Extract FRAME_CRC for anti-tampering verification
    TQString frameCrc = meta.frameCrc;
    if (frameCrc.isEmpty()) {
        // First try to extract from binary embedded watermark struct (.rodata.cfg: 0xA7F3B219 + 20 bytes)
        TQFile fBin(binaryPath);
        if (fBin.open(IO_ReadOnly)) {
            TQByteArray binData = fBin.readAll();
            fBin.close();
            const uint8_t *raw = (const uint8_t *)binData.data();
            int sz = binData.size();
            for (int i = 0; i + 24 <= sz; ++i) {
                uint32_t magic = (uint32_t)raw[i] |
                                 ((uint32_t)raw[i+1] << 8) |
                                 ((uint32_t)raw[i+2] << 16) |
                                 ((uint32_t)raw[i+3] << 24);
                if (magic == 0xA7F3B219) {
                    uint32_t crcVal = (uint32_t)raw[i+20] |
                                      ((uint32_t)raw[i+21] << 8) |
                                      ((uint32_t)raw[i+22] << 16) |
                                      ((uint32_t)raw[i+23] << 24);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "0x%08X", crcVal);
                    frameCrc = buf;
                    break;
                }
            }
        }
    }
    if (frameCrc.isEmpty()) {
        // Fallback: check frames_delta.h in project directory or cwd
        TQString hPath = fiBin.dirPath() + "/frames_delta.h";
        if (!TQFile::exists(hPath)) hPath = "frames_delta.h";
        TQFile fH(hPath);
        if (fH.open(IO_ReadOnly)) {
            TQTextStream tsH(&fH);
            while (!tsH.atEnd()) {
                TQString line = tsH.readLine().stripWhiteSpace();
                if (line.startsWith("#define FRAME_CRC")) {
                    TQStringList parts = TQStringList::split(' ', line);
                    if (parts.count() >= 3) {
                        frameCrc = parts[2].stripWhiteSpace();
                    }
                    break;
                }
            }
            fH.close();
        }
    }

    // 4. Write metadata.conf
    TQFile metaFile(tmpPath + "/metadata.conf");
    if (!metaFile.open(IO_WriteOnly)) {
        outError = "Unable to write metadata.conf.";
        system(TQString("rm -rf \"%1\"").arg(tmpPath).latin1());
        return false;
    }

    TQTextStream ts(&metaFile);
    ts << "# XBootsplash Package Metadata\n";
    ts << "XBS_PKG_VERSION=\"1.0\"\n";
    ts << "SPLASH_NAME=\"" << (meta.splashName.isEmpty() ? "splash" : meta.splashName) << "\"\n";
    if (!meta.author.isEmpty()) {
        ts << "AUTHOR=\"" << meta.author << "\"\n";
    }
    if (!meta.notes.isEmpty()) {
        ts << "NOTES=\"" << meta.notes << "\"\n";
    }
    ts << "BACKEND=\"" << (meta.backend.isEmpty() ? "fbdev" : meta.backend) << "\"\n";
    ts << "RESOLUTION=\"" << (meta.resolution.isEmpty() ? "1920x1080" : meta.resolution) << "\"\n";
    ts << "DISPLAY_MODE=\"" << meta.displayMode << "\"\n";
    ts << "BG_COLOR=\"" << meta.bgColor << "\"\n";
    ts << "FRAME_W=\"" << meta.frameW << "\"\n";
    ts << "FRAME_H=\"" << meta.frameH << "\"\n";
    ts << "NFRAMES=\"" << meta.nframes << "\"\n";
    ts << "FRAME_DELAY=\"" << meta.frameDelay << "\"\n";
    ts << "FPS=\"" << (meta.frameDelay > 0 ? 1000 / meta.frameDelay : 30) << "\"\n";
    ts << "FRAME_OFFSET_X=\"" << meta.offsetX << "\"\n";
    ts << "FRAME_OFFSET_Y=\"" << meta.offsetY << "\"\n";
    ts << "BG_OFFSET_X=\"" << meta.bgOffsetX << "\"\n";
    ts << "BG_OFFSET_Y=\"" << meta.bgOffsetY << "\"\n";
    ts << "BINARY_SHA256=\"" << sha256 << "\"\n";
    ts << "BINARY_NAME=\"" << fiBin.fileName() << "\"\n";
    ts << "BINARY_SIZE=\"" << (unsigned long)fiBin.size() << "\"\n";
    TQString compStr = meta.compression;
    if (compStr.isEmpty()) {
        compStr = detectCompressionFromBinary(binaryPath, meta.displayMode);
    }
    if (!compStr.isEmpty()) {
        ts << "COMPRESSION=\"" << compStr << "\"\n";
    }
    if (!frameCrc.isEmpty()) {
        ts << "FRAME_CRC=\"" << frameCrc << "\"\n";
    }
    metaFile.close();

    // 5. Copy visual preview into package (prefer PNG for static images, GIF for animations)
    TQString previewSrcGif = binaryPath + "_preview.gif";
    TQString previewSrcPng = binaryPath + "_preview.png";
    if (meta.displayMode >= 2) {
        // Static image modes (2, 3, 4): prefer PNG
        if (TQFile::exists(previewSrcPng) && TQFileInfo(previewSrcPng).size() > 0) {
            system(TQString("cp \"%1\" \"%2/preview.png\"").arg(previewSrcPng).arg(tmpPath).latin1());
        } else if (TQFile::exists(previewSrcGif) && TQFileInfo(previewSrcGif).size() > 0) {
            system(TQString("cp \"%1\" \"%2/preview.gif\"").arg(previewSrcGif).arg(tmpPath).latin1());
        }
    } else {
        // Animation modes (0, 1): prefer GIF
        if (TQFile::exists(previewSrcGif) && TQFileInfo(previewSrcGif).size() > 0) {
            system(TQString("cp \"%1\" \"%2/preview.gif\"").arg(previewSrcGif).arg(tmpPath).latin1());
        } else if (TQFile::exists(previewSrcPng) && TQFileInfo(previewSrcPng).size() > 0) {
            system(TQString("cp \"%1\" \"%2/preview.png\"").arg(previewSrcPng).arg(tmpPath).latin1());
        }
    }

    // 6. Package into tar.gz (.xbs archive)
    TQString finalPkg;
    if (outputDir.endsWith(".xbs")) {
        finalPkg = outputDir;
        TQFileInfo fiOut(finalPkg);
        TQDir outD(fiOut.dirPath());
        if (!outD.exists()) outD.mkdir(fiOut.dirPath());
    } else {
        TQDir outD(outputDir);
        if (!outD.exists()) {
            outD.mkdir(outputDir);
        }

        TQString pkgName = TQString("%1_%2_%3.xbs")
                           .arg(meta.splashName)
                           .arg(meta.backend)
                           .arg(meta.resolution);

        finalPkg = outputDir + "/" + pkgName;
    }

    TQString tarCmd = TQString("tar -czf \"%1\" -C \"%2\" .").arg(finalPkg).arg(tmpPath);

    int ret = system(tarCmd.latin1());
    system(TQString("rm -rf \"%1\"").arg(tmpPath).latin1());

    if (ret != 0) {
        outError = "tar command failed while creating .xbs archive.";
        return false;
    }

    outPackagePath = finalPkg;
    return true;
}

bool SplashPackageManager::readPackageMetadata(const TQString &packagePath,
                                               PackageMetadata &outMeta,
                                               TQString &outPreviewPath,
                                               TQString &outBinaryName,
                                               TQString &outError,
                                               TQString *outTmpDir)
{
    if (!TQFile::exists(packagePath)) {
        outError = "Package file not found.";
        return false;
    }

    char tmpTemplate[] = "/tmp/xbs_inspect_XXXXXX";
    char *tmpDir = mkdtemp(tmpTemplate);
    if (!tmpDir) {
        outError = "Unable to create temporary inspection directory.";
        return false;
    }

    TQString tmpPath(tmpDir);

    // Unpack archive cleanly
    TQString tarCmd = TQString("tar -xzf \"%1\" -C \"%2\"").arg(packagePath).arg(tmpPath);
    if (system(tarCmd.latin1()) != 0) {
        outError = "Failed to extract package archive.";
        system(TQString("rm -rf \"%1\"").arg(tmpPath).latin1());
        return false;
    }

    // Locate metadata.conf (root or subdirectory)
    TQString metaPath = tmpPath + "/metadata.conf";
    if (!TQFile::exists(metaPath)) {
        TQDir td(tmpPath);
        TQStringList subdirs = td.entryList(TQDir::Dirs);
        for (TQStringList::Iterator sit = subdirs.begin(); sit != subdirs.end(); ++sit) {
            if (*sit == "." || *sit == "..") continue;
            TQString candidate = tmpPath + "/" + (*sit) + "/metadata.conf";
            if (TQFile::exists(candidate)) {
                metaPath = candidate;
                break;
            }
        }
    }

    TQFile f(metaPath);
    if (!f.open(IO_ReadOnly)) {
        outError = "Unable to read metadata.conf from package.";
        system(TQString("rm -rf \"%1\"").arg(tmpPath).latin1());
        return false;
    }

    TQTextStream ts(&f);
    while (!ts.atEnd()) {
        TQString line = ts.readLine().stripWhiteSpace();
        if (line.startsWith("#") || line.isEmpty()) continue;
        int eq = line.find('=');
        if (eq > 0) {
            TQString key = line.left(eq).stripWhiteSpace();
            TQString val = line.mid(eq + 1).stripWhiteSpace();
            if (val.startsWith("\"") && val.endsWith("\"")) {
                val = val.mid(1, val.length() - 2);
            }

            if (key == "XBS_PKG_VERSION") outMeta.pkgVersion = val;
            else if (key == "SPLASH_NAME") outMeta.splashName = val;
            else if (key == "AUTHOR") outMeta.author = val;
            else if (key == "NOTES") outMeta.notes = val;
            else if (key == "BACKEND") outMeta.backend = val;
            else if (key == "RESOLUTION") outMeta.resolution = val;
            else if (key == "DISPLAY_MODE") outMeta.displayMode = val.toInt();
            else if (key == "BG_COLOR") outMeta.bgColor = val;
            else if (key == "FRAME_W") outMeta.frameW = val.toInt();
            else if (key == "FRAME_H") outMeta.frameH = val.toInt();
            else if (key == "NFRAMES") outMeta.nframes = val.toInt();
            else if (key == "FRAME_DELAY") outMeta.frameDelay = val.toInt();
            else if (key == "FPS") outMeta.fps = val.toInt();
            else if (key == "FRAME_OFFSET_X") outMeta.offsetX = val.toInt();
            else if (key == "FRAME_OFFSET_Y") outMeta.offsetY = val.toInt();
            else if (key == "BG_OFFSET_X") outMeta.bgOffsetX = val.toInt();
            else if (key == "BG_OFFSET_Y") outMeta.bgOffsetY = val.toInt();
            else if (key == "BINARY_SHA256") outMeta.binarySha256 = val;
            else if (key == "BINARY_NAME") outMeta.binaryName = val;
            else if (key == "BINARY_SIZE") outMeta.binarySize = (unsigned long)val.toULong();
            else if (key == "COMPRESSION") outMeta.compression = val;
            else if (key == "FRAME_CRC") outMeta.frameCrc = val;
        }
    }
    f.close();

    // Check preview
    TQString prev = tmpPath + "/preview.gif";
    if (!TQFile::exists(prev) || TQFileInfo(prev).size() == 0) prev = tmpPath + "/preview.png";
    if (!TQFile::exists(prev) || TQFileInfo(prev).size() == 0) {
        TQDir td(tmpPath);
        TQStringList subdirs = td.entryList(TQDir::Dirs);
        for (TQStringList::Iterator sit = subdirs.begin(); sit != subdirs.end(); ++sit) {
            if (*sit == "." || *sit == "..") continue;
            TQString cGif = tmpPath + "/" + (*sit) + "/preview.gif";
            TQString cPng = tmpPath + "/" + (*sit) + "/preview.png";
            if (TQFile::exists(cGif) && TQFileInfo(cGif).size() > 0) { prev = cGif; break; }
            if (TQFile::exists(cPng) && TQFileInfo(cPng).size() > 0) { prev = cPng; break; }
        }
    }
    if (TQFile::exists(prev) && TQFileInfo(prev).size() > 0) {
        outPreviewPath = prev;
    } else {
        outPreviewPath = "";
    }

    // Identify binary name from extracted files
    outBinaryName = "";

    // Priority 1: explicitly declared in metadata
    if (!outMeta.binaryName.isEmpty() && TQFile::exists(tmpPath + "/" + outMeta.binaryName)) {
        outBinaryName = outMeta.binaryName;
    }
    // Priority 2: standard bootsplash binary matching splash name (xbs_<name>)
    else if (!outMeta.splashName.isEmpty() && TQFile::exists(tmpPath + "/xbs_" + outMeta.splashName)) {
        outBinaryName = "xbs_" + outMeta.splashName;
    }
    // Priority 3: standard splash_bin
    else if (TQFile::exists(tmpPath + "/splash_bin")) {
        outBinaryName = "splash_bin";
    }
    else {
        // Fallback: search directory for executable / non-metadata files
        TQDir extractDir(tmpPath);
        TQStringList fileList = extractDir.entryList(TQDir::Files);
        for (TQStringList::Iterator it = fileList.begin(); it != fileList.end(); ++it) {
            TQString item = *it;
            if (item != "metadata.conf" && item != "preview.gif" && item != "preview.png") {
                outBinaryName = item;
                break;
            }
        }

        if (outBinaryName.isEmpty()) {
            TQStringList subdirs = extractDir.entryList(TQDir::Dirs);
            for (TQStringList::Iterator sit = subdirs.begin(); sit != subdirs.end(); ++sit) {
                if (*sit == "." || *sit == "..") continue;
                TQDir subD(tmpPath + "/" + (*sit));
                TQStringList sfiles = subD.entryList(TQDir::Files);
                for (TQStringList::Iterator fit = sfiles.begin(); fit != sfiles.end(); ++fit) {
                    TQString item = *fit;
                    if (item != "metadata.conf" && item != "preview.gif" && item != "preview.png") {
                        outBinaryName = item;
                        break;
                    }
                }
                if (!outBinaryName.isEmpty()) break;
            }
        }
    }

    if (outMeta.binarySize == 0 && !outBinaryName.isEmpty()) {
        TQString candidateBin = tmpPath + "/" + outBinaryName;
        if (TQFile::exists(candidateBin)) {
            outMeta.binarySize = (unsigned long)TQFileInfo(candidateBin).size();
        } else {
            TQDir d(tmpPath);
            TQStringList subdirs = d.entryList(TQDir::Dirs);
            for (TQStringList::Iterator sit = subdirs.begin(); sit != subdirs.end(); ++sit) {
                if (*sit == "." || *sit == "..") continue;
                TQString subCandidate = tmpPath + "/" + (*sit) + "/" + outBinaryName;
                if (TQFile::exists(subCandidate)) {
                    outMeta.binarySize = (unsigned long)TQFileInfo(subCandidate).size();
                    break;
                }
            }
        }
    }

    if (outMeta.compression.isEmpty() && !outBinaryName.isEmpty()) {
        TQString candidateBin = tmpPath + "/" + outBinaryName;
        if (!TQFile::exists(candidateBin)) {
            TQDir d(tmpPath);
            TQStringList subdirs = d.entryList(TQDir::Dirs);
            for (TQStringList::Iterator sit = subdirs.begin(); sit != subdirs.end(); ++sit) {
                if (*sit == "." || *sit == "..") continue;
                TQString subCandidate = tmpPath + "/" + (*sit) + "/" + outBinaryName;
                if (TQFile::exists(subCandidate)) {
                    candidateBin = subCandidate;
                    break;
                }
            }
        }
        if (TQFile::exists(candidateBin)) {
            outMeta.compression = detectCompressionFromBinary(candidateBin, outMeta.displayMode);
        }
    }

    if (outTmpDir) {
        *outTmpDir = tmpPath;
    }

    return true;
}

bool SplashPackageManager::extractPackage(const TQString &packagePath,
                                          const TQString &destDir,
                                          TQString &outBinaryPath,
                                          TQString &outError)
{
    TQDir d(destDir);
    if (!d.exists()) d.mkdir(destDir);

    TQString tarCmd = TQString("tar -xzf \"%1\" -C \"%2\"").arg(packagePath).arg(destDir);
    if (system(tarCmd.latin1()) != 0) {
        outError = "Failed to extract .xbs archive.";
        return false;
    }

    // Locate extracted binary
    PackageMetadata meta;
    TQString prev, binName;
    readPackageMetadata(packagePath, meta, prev, binName, outError);
    if (!binName.isEmpty() && TQFile::exists(destDir + "/" + binName)) {
        outBinaryPath = destDir + "/" + binName;
        return true;
    }

    // Fallback: search destDir for binary
    TQDir destD(destDir);
    TQStringList entries = destD.entryList(TQDir::Files);
    for (TQStringList::Iterator it = entries.begin(); it != entries.end(); ++it) {
        TQString fn = *it;
        if (fn.startsWith("xbs_") || fn == "splash_bin") {
            outBinaryPath = destDir + "/" + fn;
            return true;
        }
    }

    outBinaryPath = destDir;
    return true;
}

static TQString detectDebArchitecture() {
    FILE *fp = popen("dpkg --print-architecture 2>/dev/null", "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            TQString arch = TQString::fromLocal8Bit(buf).stripWhiteSpace();
            pclose(fp);
            if (!arch.isEmpty()) return arch;
        } else {
            pclose(fp);
        }
    }
    struct utsname un;
    if (uname(&un) == 0) {
        TQString m = un.machine;
        if (m == "x86_64") return "amd64";
        if (m == "aarch64") return "arm64";
        if (m == "i686" || m == "i386") return "i386";
        if (m.startsWith("arm")) return "armhf";
        return m;
    }
    return "amd64";
}

static TQString sanitizeDebName(const TQString &input) {
    TQString res;
    for (unsigned int i = 0; i < input.length(); ++i) {
        TQChar c = input[i].lower();
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            res += c;
        } else if (c == '_' || c == '-' || c == '.') {
            res += '-';
        }
    }
    while (res.startsWith("-")) res = res.mid(1);
    while (res.endsWith("-")) res.truncate(res.length() - 1);
    if (res.isEmpty()) res = "splash";
    return res;
}

bool SplashPackageManager::exportDebianPackage(const TQString &binaryPath,
                                               const PackageMetadata &meta,
                                               const TQString &outputFilePath,
                                               bool forShutdown,
                                               TQString &outDebPath,
                                               TQString &outError)
{
    if (!TQFile::exists(binaryPath)) {
        outError = "Source binary does not exist: " + binaryPath;
        return false;
    }

    TQFileInfo fiBin(binaryPath);
    TQString binName = fiBin.fileName();
    TQString baseSplashName = meta.splashName.isEmpty() ? binName : meta.splashName;
    if (baseSplashName.startsWith("xbs_")) baseSplashName = baseSplashName.mid(4);

    TQString debPkgName = "xbootsplash-theme-" + sanitizeDebName(baseSplashName);
    TQString arch = detectDebArchitecture();

    // Determine final output path
    TQString finalDebPath = outputFilePath;
    if (finalDebPath.isEmpty()) {
        finalDebPath = debPkgName + "_1.0_" + arch + ".deb";
    } else {
        TQFileInfo fiOut(finalDebPath);
        if (fiOut.isDir()) {
            finalDebPath = fiOut.absFilePath() + "/" + debPkgName + "_1.0_" + arch + ".deb";
        }
    }

    // Create temporary staging directory
    char tmpTemplate[] = "/tmp/xbs_deb_staging_XXXXXX";
    char *tmpDir = mkdtemp(tmpTemplate);
    if (!tmpDir) {
        outError = "Unable to create temporary staging directory.";
        return false;
    }
    TQString stgPath(tmpDir);

    // Create directory hierarchy
    system(("mkdir -p \"" + stgPath + "/DEBIAN\" \"" +
            stgPath + "/sbin\" \"" +
            stgPath + "/etc/initramfs-tools/hooks\" \"" +
            stgPath + "/etc/initramfs-tools/scripts/init-top\" \"" +
            stgPath + "/etc/initramfs-tools/scripts/init-bottom\" \"" +
            stgPath + "/etc/initramfs-tools/scripts/local-premount\"").latin1());
    if (forShutdown) {
        system(("mkdir -p \"" + stgPath + "/lib/systemd/system-shutdown\"").latin1());
    }

    // 1. Copy binary to /sbin/<binName>
    TQString destBin = stgPath + "/sbin/" + binName;
    system(("cp -f \"" + binaryPath + "\" \"" + destBin + "\" && chmod 755 \"" + destBin + "\"").latin1());

    bool useDrm = (meta.backend.lower() == "drm" || meta.backend.lower() == "drm/kms");

    // 2. Write /etc/initramfs-tools/hooks/<binName>
    TQString hookPath = stgPath + "/etc/initramfs-tools/hooks/" + binName;
    TQFile fHook(hookPath);
    if (fHook.open(IO_WriteOnly)) {
        TQTextStream ts(&fHook);
        ts << "#!/bin/sh\n"
           << "PREREQ=\"\"\n"
           << "prereqs() { echo \"$PREREQ\"; }\n"
           << "case \"$1\" in prereqs) prereqs; exit 0;; esac\n\n"
           << ". /usr/share/initramfs-tools/hook-functions\n\n"
           << "copy_exec /sbin/" << binName << " /sbin\n";
        if (useDrm) {
            ts << "mkdir -p \"${DESTDIR}/dev/dri\" 2>/dev/null || true\n";
        } else {
            ts << "if [ ! -e \"${DESTDIR}/dev/fb0\" ]; then\n"
               << "    mknod \"${DESTDIR}/dev/fb0\" c 29 0 2>/dev/null || true\n"
               << "fi\n";
        }
        fHook.close();
        chmod(hookPath.latin1(), 0755);
    }

    // 3. Write /etc/initramfs-tools/scripts/init-top/<binName>
    TQString topPath = stgPath + "/etc/initramfs-tools/scripts/init-top/" + binName;
    TQFile fTop(topPath);
    if (fTop.open(IO_WriteOnly)) {
        TQTextStream ts(&fTop);
        ts << "#!/bin/sh\n"
           << "PREREQ=\"" << (useDrm ? "udev" : "") << "\"\n"
           << "prereqs() { echo \"$PREREQ\"; }\n"
           << "case \"$1\" in prereqs) prereqs; exit 0;; esac\n\n"
           << ". /scripts/functions\n\n";
        if (!useDrm) {
            ts << "if [ ! -c /dev/fb0 ]; then\n"
               << "    if [ ! -d /sys/class/graphics/fb0 ]; then\n"
               << "        exit 0\n"
               << "    fi\n"
               << "    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do\n"
               << "        sleep 0.2\n"
               << "        [ -c /dev/fb0 ] && break\n"
               << "    done\n"
               << "    if [ ! -c /dev/fb0 ] && [ -d /sys/class/graphics/fb0 ]; then\n"
               << "        mknod /dev/fb0 c 29 0 2>/dev/null || true\n"
               << "    fi\n"
               << "    if [ ! -c /dev/fb0 ]; then\n"
               << "        exit 0\n"
               << "    fi\n"
               << "fi\n\n";
        }
        ts << "if [ -x /sbin/" << binName << " ]; then\n"
           << "    /sbin/" << binName << " &\n"
           << "    SPLASH_PID=$!\n"
           << "    sleep 0.1\n"
           << "    if [ -d \"/proc/$SPLASH_PID\" ]; then\n"
           << "        echo \"$SPLASH_PID\" > /run/" << binName << ".pid\n"
           << "        START_TIME=$(awk '{print $22}' /proc/$SPLASH_PID/stat 2>/dev/null)\n"
           << "        echo \"$START_TIME\" > /run/" << binName << ".start_time\n"
           << "    fi\n"
           << "fi\n";
        fTop.close();
        chmod(topPath.latin1(), 0755);
    }

    // 4. Write /etc/initramfs-tools/scripts/init-bottom/<binName>
    TQString botPath = stgPath + "/etc/initramfs-tools/scripts/init-bottom/" + binName;
    TQFile fBot(botPath);
    if (fBot.open(IO_WriteOnly)) {
        TQTextStream ts(&fBot);
        ts << "#!/bin/sh\n"
           << "PREREQ=\"\"\n"
           << "prereqs() { echo \"$PREREQ\"; }\n"
           << "case \"$1\" in prereqs) prereqs; exit 0;; esac\n\n"
           << ". /scripts/functions\n\n";
        if (useDrm) {
            ts << "if [ -x /sbin/" << binName << " ]; then\n"
               << "    /sbin/" << binName << " --restore-crtc 2>/dev/null || true\n"
               << "fi\n\n";
        }
        ts << "if [ -f /run/" << binName << ".pid ]; then\n"
           << "    PID=$(cat /run/" << binName << ".pid 2>/dev/null)\n"
           << "    if [ -n \"$PID\" ] && [ -d \"/proc/$PID\" ]; then\n"
           << "        COMM=$(cat /proc/$PID/comm 2>/dev/null)\n"
           << "        if [ \"$COMM\" = \"" << binName << "\" ]; then\n"
           << "            SAVED_START=$(cat /run/" << binName << ".start_time 2>/dev/null)\n"
           << "            CURRENT_START=$(awk '{print $22}' /proc/$PID/stat 2>/dev/null)\n"
           << "            if [ -n \"$SAVED_START\" ] && [ \"$SAVED_START\" = \"$CURRENT_START\" ]; then\n"
           << "                kill \"$PID\" 2>/dev/null || true\n"
           << "                for _i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do\n"
           << "                    [ -d \"/proc/$PID\" ] || break\n"
           << "                    sleep 0.1\n"
           << "                done\n"
           << "                if [ -d \"/proc/$PID\" ]; then\n"
           << "                    kill -9 \"$PID\" 2>/dev/null || true\n"
           << "                fi\n"
           << "            fi\n"
           << "        fi\n"
           << "    fi\n"
           << "    rm -f /run/" << binName << ".pid /run/" << binName << ".start_time\n"
           << "fi\n";
        if (!useDrm) {
            ts << "\nif [ -c /dev/fb0 ]; then\n"
               << "    dd if=/dev/zero of=/dev/fb0 2>/dev/null || true\n"
               << "fi\n";
        }
        fBot.close();
        chmod(botPath.latin1(), 0755);
    }

    // 5. Write /etc/initramfs-tools/scripts/local-premount/00_<binName>
    TQString resPath = stgPath + "/etc/initramfs-tools/scripts/local-premount/00_" + binName;
    TQFile fRes(resPath);
    if (fRes.open(IO_WriteOnly)) {
        TQTextStream ts(&fRes);
        ts << "#!/bin/sh\n"
           << "PREREQ=\"\"\n"
           << "prereqs() { echo \"$PREREQ\"; }\n"
           << "case \"$1\" in prereqs) prereqs; exit 0;; esac\n\n"
           << "[ -z \"${resume?}\" ] || [ ! -e /sys/power/resume ] && exit 0\n\n"
           << ". /scripts/functions\n"
           << ". /scripts/local\n\n"
           << "if ! local_device_setup \"${resume}\" \"suspend/resume device\" false; then\n"
           << "    exit 0\n"
           << "fi\n\n"
           << "if [ \"$(get_fstype \"${DEV}\")\" = \"suspend\" ]; then\n"
           << "    if [ -f /run/" << binName << ".pid ]; then\n"
           << "        PID=$(cat /run/" << binName << ".pid 2>/dev/null)\n"
           << "        if [ -n \"$PID\" ] && [ -d \"/proc/$PID\" ]; then\n"
           << "            COMM=$(cat /proc/$PID/comm 2>/dev/null)\n"
           << "            if [ \"$COMM\" = \"" << binName << "\" ]; then\n"
           << "                kill -CONT \"$PID\" 2>/dev/null || true\n"
           << "            fi\n"
           << "        fi\n"
           << "    fi\n"
           << "fi\n";
        fRes.close();
        chmod(resPath.latin1(), 0755);
    }

    // 6. If shutdown target
    if (forShutdown) {
        TQString sdPath = stgPath + "/lib/systemd/system-shutdown/" + binName;
        TQFile fSd(sdPath);
        if (fSd.open(IO_WriteOnly)) {
            TQTextStream ts(&fSd);
            ts << "#!/bin/sh\n"
               << "exec /sbin/" << binName << " \"$@\"\n";
            fSd.close();
            chmod(sdPath.latin1(), 0755);
        }
    }

    // 7. DEBIAN/control
    TQString authorStr = meta.author.isEmpty() ? "XBootsplash Community <xbootsplash@localhost>" : meta.author;
    if (!authorStr.contains("<")) {
        authorStr += " <" + sanitizeDebName(authorStr) + "@localhost>";
    }
    TQString notesStr = meta.notes.isEmpty() ? (baseSplashName + " boot theme") : meta.notes;

    TQString ctrlPath = stgPath + "/DEBIAN/control";
    TQFile fCtrl(ctrlPath);
    if (fCtrl.open(IO_WriteOnly)) {
        TQTextStream ts(&fCtrl);
        ts << "Package: " << debPkgName << "\n"
           << "Version: 1.0\n"
           << "Section: admin\n"
           << "Priority: optional\n"
           << "Architecture: " << arch << "\n"
           << "Maintainer: " << authorStr << "\n"
           << "Depends: initramfs-tools\n"
           << "Provides: xbootsplash-theme\n"
           << "Conflicts: xbootsplash-theme\n"
           << "Description: " << notesStr << "\n"
           << " Autonomous and lightweight bootsplash theme for Debian, Q4OS and Ubuntu.\n"
           << " Built with XBootsplash Studio.\n";
        fCtrl.close();
        chmod(ctrlPath.latin1(), 0644);
    }

    // 8. DEBIAN/postinst
    TQString postinstPath = stgPath + "/DEBIAN/postinst";
    TQFile fPost(postinstPath);
    if (fPost.open(IO_WriteOnly)) {
        TQTextStream ts(&fPost);
        ts << "#!/bin/sh\n"
           << "set -e\n\n"
           << "if [ -x /usr/sbin/update-initramfs ]; then\n"
           << "    echo \"Updating initramfs for XBootsplash (" << debPkgName << ")...\"\n"
           << "    update-initramfs -u\n"
           << "fi\n\n"
           << "exit 0\n";
        fPost.close();
        chmod(postinstPath.latin1(), 0755);
    }

    // 9. DEBIAN/prerm
    TQString prermPath = stgPath + "/DEBIAN/prerm";
    TQFile fPrerm(prermPath);
    if (fPrerm.open(IO_WriteOnly)) {
        TQTextStream ts(&fPrerm);
        ts << "#!/bin/sh\n"
           << "set -e\n\n"
           << "pkill -x \"" << binName << "\" 2>/dev/null || true\n"
           << "rm -f \"/run/" << binName << ".pid\" \"/run/" << binName << ".start_time\" 2>/dev/null || true\n\n"
           << "exit 0\n";
        fPrerm.close();
        chmod(prermPath.latin1(), 0755);
    }

    // 10. DEBIAN/postrm
    TQString postrmPath = stgPath + "/DEBIAN/postrm";
    TQFile fPostrm(postrmPath);
    if (fPostrm.open(IO_WriteOnly)) {
        TQTextStream ts(&fPostrm);
        ts << "#!/bin/sh\n"
           << "set -e\n\n"
           << "case \"$1\" in\n"
           << "    remove|purge)\n"
           << "        if [ -x /usr/sbin/update-initramfs ]; then\n"
           << "            echo \"Updating initramfs after XBootsplash removal...\"\n"
           << "            update-initramfs -u\n"
           << "        fi\n"
           << "        ;;\n"
           << "esac\n\n"
           << "exit 0\n";
        fPostrm.close();
        chmod(postrmPath.latin1(), 0755);
    }

    // Ensure parent dir of output package exists
    TQFileInfo fiFinal(finalDebPath);
    TQDir outD(fiFinal.dirPath());
    if (!outD.exists()) outD.mkdir(fiFinal.dirPath());

    // 11. Build package using dpkg-deb (preferred) or ar/tar fallback
    bool buildSuccess = false;
    if (system("which dpkg-deb >/dev/null 2>&1") == 0) {
        TQString cmd = TQString("dpkg-deb --root-owner-group -b \"%1\" \"%2\" >/dev/null 2>&1")
                       .arg(stgPath).arg(finalDebPath);
        buildSuccess = (system(cmd.latin1()) == 0);
    }
    if (!buildSuccess) {
        // Fallback using tar & ar
        TQString cmdFallback = "echo '2.0' > \"" + stgPath + "/debian-binary\" && " +
                               "tar --owner=0 --group=0 -czf \"" + stgPath + "/control.tar.gz\" -C \"" + stgPath + "/DEBIAN\" . && " +
                               "tar --owner=0 --group=0 -czf \"" + stgPath + "/data.tar.gz\" --exclude=./DEBIAN --exclude=./control.tar.gz --exclude=./debian-binary -C \"" + stgPath + "\" . && " +
                               "cd \"" + stgPath + "\" && ar -rc \"" + finalDebPath + "\" debian-binary control.tar.gz data.tar.gz";
        buildSuccess = (system(cmdFallback.latin1()) == 0);
    }

    // Cleanup staging directory
    system(TQString("rm -rf \"%1\"").arg(stgPath).latin1());

    if (!buildSuccess || !TQFile::exists(finalDebPath)) {
        outError = "Failed to build Debian package.";
        return false;
    }

    outDebPath = finalDebPath;
    return true;
}

bool SplashPackageManager::convertPackageToDeb(const TQString &packagePath,
                                               const TQString &outputFilePath,
                                               bool forShutdown,
                                               TQString &outDebPath,
                                               TQString &outError)
{
    if (!TQFile::exists(packagePath)) {
        outError = "Package file not found: " + packagePath;
        return false;
    }

    PackageMetadata meta;
    TQString previewPath, binName, tmpDir;
    if (!readPackageMetadata(packagePath, meta, previewPath, binName, outError, &tmpDir)) {
        return false;
    }

    TQString binPath = tmpDir + "/" + binName;
    if (!TQFile::exists(binPath)) {
        binPath = tmpDir + "/splash_bin";
    }
    if (!TQFile::exists(binPath)) {
        outError = "Binary not found inside package archive.";
        system(TQString("rm -rf \"%1\"").arg(tmpDir).latin1());
        return false;
    }

    bool res = exportDebianPackage(binPath, meta, outputFilePath, forShutdown, outDebPath, outError);
    system(TQString("rm -rf \"%1\"").arg(tmpDir).latin1());
    return res;
}

#include "package_manager.moc"
