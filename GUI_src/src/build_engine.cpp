#include "build_engine.h"
#include "log_banner.h"
#include "embedded_engine_extractor.h"

#include <ntqdir.h>
#include <ntqfileinfo.h>
#include <ntqfile.h>
#include <stdio.h>
#include <sys/stat.h>

SplashBuildEngine::SplashBuildEngine(TQObject *parent, const char *name)
    : TQObject(parent, name),
      m_isBuilding(false),
      m_currentStep(StepNone),
      m_process(0),
      m_tempBuildDir("")
{
}

SplashBuildEngine::~SplashBuildEngine() {
    cancelBuild();
}

bool SplashBuildEngine::copyFile(const TQString &src, const TQString &dst) {
    TQFile fIn(src);
    if (!fIn.open(IO_ReadOnly)) return false;
    TQFile fOut(dst);
    if (!fOut.open(IO_WriteOnly | IO_Truncate)) {
        fIn.close();
        return false;
    }
    char buf[65536];
    TQ_LONG rd = 0;
    while ((rd = fIn.readBlock(buf, sizeof(buf))) > 0) {
        if (fOut.writeBlock(buf, rd) != rd) {
            fIn.close();
            fOut.close();
            return false;
        }
    }
    fIn.close();
    fOut.close();
    chmod(dst.latin1(), 0755);
    return true;
}

void SplashBuildEngine::log(const TQString &msg) {
    emit logMessage(msg);
}

void SplashBuildEngine::fail(const TQString &reason) {
    log(TQString("[ERROR] %1").arg(reason));
    log(SplashLog::failedBanner());
    m_isBuilding = false;
    m_currentStep = StepNone;
    if (m_process) {
        delete m_process;
        m_process = 0;
    }
    EmbeddedEngineExtractor::cleanupBuildDir(m_tempBuildDir);
    m_tempBuildDir = "";
    emit buildFinished(false, "", 0);
}

void SplashBuildEngine::cancelBuild() {
    if (m_isBuilding) {
        if (m_process && m_process->isRunning()) {
            m_process->tryTerminate();
        }
        EmbeddedEngineExtractor::cleanupBuildDir(m_tempBuildDir);
        m_tempBuildDir = "";
        log("[ERROR] Build cancelled by user.");
        log(SplashLog::failedBanner());
        m_isBuilding = false;
        m_currentStep = StepNone;
        emit buildFinished(false, "", 0);
    }
}

void SplashBuildEngine::startBuild(const BuildParams &params) {
    if (m_isBuilding) return;

    m_params = params;
    m_isBuilding = true;
    m_currentStep = StepNone;

    emit buildStarted();
    log(SplashLog::banner("BUILD"));
    log(TQString("Display mode : %1").arg(m_params.displayMode));
    log(TQString("Source       : %1").arg(m_params.framesPath));
    log(TQString("Target binary: %1").arg(m_params.binaryName));
    log(TQString("Backend      : %1").arg(m_params.useDrm ? "DRM/KMS" : "Framebuffer fbdev"));
    TQString superCompDesc = "Disabled (fast)";
    if (m_params.superCompression == 2) superCompDesc = "UPKR (maximum compression)";
    else if (m_params.superCompression == 1 || m_params.useZx0) superCompDesc = "ZX0 (super-compression)";
    log(TQString("Super Compress: %1").arg(superCompDesc));
    if (m_params.displayMode <= 2) {
        log(TQString("Min Boot Loops: %1").arg(m_params.minBootLoops > 0 ? TQString("%1 cycle(s)").arg(m_params.minBootLoops) : TQString("Disabled (instant exit)")));
    }

    // Validation
    if (m_params.framesPath.isEmpty() || !TQFile::exists(m_params.framesPath)) {
        fail("Source frames/image path invalid or not found.");
        return;
    }

    if ((m_params.displayMode == 1 || m_params.displayMode == 2) &&
        (m_params.bgImagePath.isEmpty() || !TQFile::exists(m_params.bgImagePath))) {
        fail("Background image is required and must exist for Display Mode 1 and 2.");
        return;
    }

    if (m_params.binaryName.isEmpty()) {
        fail("Target binary name is empty.");
        return;
    }

    // Allocate ephemeral RAM build directory
    TQString err;
    m_tempBuildDir = EmbeddedEngineExtractor::createEphemeralBuildDir(&err);
    if (m_tempBuildDir.isEmpty()) {
        fail(TQString("Could not allocate RAM build space: %1").arg(err));
        return;
    }
    log(TQString("Ephemeral RAM build directory: %1").arg(m_tempBuildDir));

    if (!EmbeddedEngineExtractor::extractTo(m_tempBuildDir, &err)) {
        fail(TQString("Failed to unpack embedded engine: %1").arg(err));
        return;
    }

    m_currentStep = StepCompileGenerator;
    runNextStep();
}

void SplashBuildEngine::runNextStep() {
    if (m_process) {
        delete m_process;
        m_process = 0;
    }

    m_process = new TQProcess(this);
    m_process->setWorkingDirectory(m_tempBuildDir);
    connect(m_process, SIGNAL(readyReadStdout()), this, SLOT(onProcessReadyReadStdout()));
    connect(m_process, SIGNAL(readyReadStderr()), this, SLOT(onProcessReadyReadStderr()));
    connect(m_process, SIGNAL(processExited()), this, SLOT(onProcessExited()));

    TQStringList args;
    args << "/bin/sh" << "-c";

    if (m_currentStep == StepCompileGenerator) {
        emit buildStatusChanged("Compiling splash generator...");
        log("[1/3] Compiling splash generator (generate_splash)...");
        args << "make generator || gcc -O2 -Iengine_src -I. -o generate_splash engine_src/generate_splash.c -lpng -lm";
        m_process->setArguments(args);
        if (!m_process->start()) {
            fail("Unable to execute gcc.");
        }
    } else if (m_currentStep == StepRunGenerator) {
        if (m_params.superCompression == 2) {
            emit buildStatusChanged("Compressing frames (UPKR super-compression)...");
            log(TQString("[2/3] Generating splash data (frames_delta.h) [UPKR compression enabled]..."));
        } else if (m_params.superCompression == 1 || m_params.useZx0) {
            emit buildStatusChanged("Compressing frames (ZX0 super-compression)...");
            log(TQString("[2/3] Generating splash data (frames_delta.h) [ZX0 compression enabled]..."));
        } else {
            emit buildStatusChanged("Compressing frames & delta data...");
            log(TQString("[2/3] Generating splash data (frames_delta.h)..."));
        }

        char hexColor[8];
        snprintf(hexColor, sizeof(hexColor), "%02X%02X%02X",
                 m_params.bgColor.red(), m_params.bgColor.green(), m_params.bgColor.blue());

        TQString cmd = TQString("./generate_splash -m %1 -x %2 -y %3 -c %4")
                       .arg(m_params.displayMode)
                       .arg(m_params.offsetX)
                       .arg(m_params.offsetY)
                       .arg(hexColor);

        if (m_params.displayMode == 1 || m_params.displayMode == 2) {
            cmd += TQString(" -b \"%1\"").arg(m_params.bgImagePath);
        }
        if (m_params.displayMode == 1) {
            cmd += TQString(" -X %1 -Y %2").arg(m_params.bgOffsetX).arg(m_params.bgOffsetY);
        }
        if (m_params.displayMode == 2 || m_params.displayMode == 4) {
            if (!m_params.targetResolution.isEmpty()) {
                cmd += TQString(" -r %1").arg(m_params.targetResolution);
            }
        }
        if (m_params.displayMode <= 2) {
            cmd += TQString(" -d %1 -l %2").arg(m_params.frameDelay).arg(m_params.loopMode);
            if (m_params.loopMode == 2) {
                cmd += TQString(" -L %1").arg(m_params.loopStart);
            }
            if (m_params.minBootLoops > 0) {
                cmd += TQString(" -M %1").arg(m_params.minBootLoops);
            }
            if (m_params.invertFrames) {
                cmd += " -I";
            }
            cmd += TQString(" -z %1").arg(m_params.compressionMethod);
        }

        if (m_params.superCompression == 2) {
            cmd += " -U";
        } else if (m_params.superCompression == 1 || m_params.useZx0) {
            cmd += " -Z";
        }

        cmd += TQString(" \"%1\" > frames_delta.h").arg(m_params.framesPath);

        args << cmd;
        m_process->setArguments(args);
        if (!m_process->start()) {
            fail("Unable to execute generate_splash.");
        }
    } else if (m_currentStep == StepCompileBinary) {
        emit buildStatusChanged(TQString("Compiling standalone binary %1 (%2)...")
            .arg(m_params.binaryName)
            .arg(m_params.useDrm ? "DRM/KMS" : "fbdev"));
        log(TQString("[3/3] Compiling standalone binary %1 (%2)...")
            .arg(m_params.binaryName)
            .arg(m_params.useDrm ? "DRM/KMS" : "fbdev nolibc"));

        TQString cmd;
        if (m_params.useDrm) {
            cmd = TQString("make clean && make drm TARGET=\"%1\"").arg(m_params.binaryName);
            cmd += TQString(" && if [ -f \"%1_drm\" ]; then mv \"%1_drm\" \"%1\"; fi").arg(m_params.binaryName);
        } else {
            cmd = TQString("make clean && make fbdev TARGET=\"%1\"").arg(m_params.binaryName);
        }

        args << cmd;
        m_process->setArguments(args);
        if (!m_process->start()) {
            fail("Unable to execute make.");
        }
    }
}

void SplashBuildEngine::onProcessReadyReadStdout() {
    if (!m_process) return;
    TQByteArray data = m_process->readStdout();
    if (!data.isEmpty()) {
        TQString text = TQString::fromLocal8Bit(data.data(), data.size());
        TQStringList lines = TQStringList::split('\n', text);
        for (TQStringList::Iterator it = lines.begin(); it != lines.end(); ++it) {
            TQString l = (*it).stripWhiteSpace();
            if (!l.isEmpty()) log("  " + l);
        }
    }
}

void SplashBuildEngine::onProcessReadyReadStderr() {
    if (!m_process) return;
    TQByteArray data = m_process->readStderr();
    if (!data.isEmpty()) {
        TQString text = TQString::fromLocal8Bit(data.data(), data.size());
        TQStringList lines = TQStringList::split('\n', text);
        for (TQStringList::Iterator it = lines.begin(); it != lines.end(); ++it) {
            TQString l = (*it).stripWhiteSpace();
            if (!l.isEmpty()) {
                log("  " + l);
                if (m_currentStep == StepRunGenerator) {
                    if (l.contains("Super-packing animation deltas with UPKR")) {
                        emit buildStatusChanged("Super-packing animation frames with UPKR...");
                    } else if (l.contains("Compressing background image with UPKR")) {
                        emit buildStatusChanged("Compressing background image (UPKR)...");
                    } else if (l.contains("Compressing static image with UPKR")) {
                        emit buildStatusChanged("Compressing static image (UPKR)...");
                    } else if (l.contains("Super-packing animation deltas with ZX0")) {
                        emit buildStatusChanged("Super-packing animation frames with ZX0...");
                    } else if (l.contains("Compressing background image with ZX0")) {
                        emit buildStatusChanged("Compressing background image (ZX0)...");
                    } else if (l.contains("Compressing static image with ZX0")) {
                        emit buildStatusChanged("Compressing static image (ZX0)...");
                    } else if (l.contains("Compressing background image with LZSS")) {
                        emit buildStatusChanged("Compressing background image (LZSS)...");
                    } else if (l.contains("Compressing static image with LZSS")) {
                        emit buildStatusChanged("Compressing static image (LZSS)...");
                    } else if (l.contains("Finding optimal compression method")) {
                        emit buildStatusChanged("Finding optimal compression method...");
                    }
                }
            }
        }
    }
}

void SplashBuildEngine::onProcessExited() {
    if (!m_process) return;

    if (!m_process->normalExit() || m_process->exitStatus() != 0) {
        fail(TQString("Step %1 failed (exit code: %2).")
             .arg((int)m_currentStep)
             .arg(m_process->exitStatus()));
        return;
    }

    if (m_currentStep == StepCompileGenerator) {
        log("Generator compiled successfully.");
        m_currentStep = StepRunGenerator;
        runNextStep();
    } else if (m_currentStep == StepRunGenerator) {
        log("Header frames_delta.h generated successfully.");
        m_currentStep = StepCompileBinary;
        runNextStep();
    } else if (m_currentStep == StepCompileBinary) {
        TQString builtFile = m_tempBuildDir + "/" + m_params.binaryName;
        TQFileInfo fi(builtFile);

        if (!fi.exists() || fi.size() == 0) {
            fail("The final binary was not produced or is empty.");
            return;
        }

        unsigned long sz = (unsigned long)fi.size();
        double kb = sz / 1024.0;
        double mb = kb / 1024.0;

        TQString targetDir = m_params.projectRoot;
        if (targetDir.isEmpty() || !TQDir(targetDir).exists()) {
            targetDir = TQDir::currentDirPath();
        }
        TQString targetFile = targetDir + "/" + m_params.binaryName;
        if (TQFile::exists(targetFile)) {
            TQFile::remove(targetFile);
        }
        if (!copyFile(builtFile, targetFile)) {
            fail(TQString("Failed to copy compiled binary to destination: %1").arg(targetFile));
            return;
        }

        // Immediately purge RAM build tree
        EmbeddedEngineExtractor::cleanupBuildDir(m_tempBuildDir);
        m_tempBuildDir = "";

        log(TQString("✔ SUCCESS: Binary compiled: %1").arg(targetFile));
        if (mb >= 1.0) {
            log(TQString("  Size: %1 MB (%2 bytes)").arg(mb, 0, 'f', 2).arg(sz));
        } else {
            log(TQString("  Size: %1 KB (%2 bytes)").arg(kb, 0, 'f', 1).arg(sz));
        }

        if (sz > 15 * 1024 * 1024) {
            log("⚠ WARNING: Size exceeds 15 MB, which may fill the /boot partition!");
        }
        log(SplashLog::doneBanner());

        m_isBuilding = false;
        m_currentStep = StepDone;
        emit buildFinished(true, targetFile, sz);
    }
}

#include "build_engine.moc"
