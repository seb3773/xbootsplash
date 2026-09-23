#include "installer_engine.h"
#include "log_banner.h"

#include <ntqfile.h>
#include <ntqtextstream.h>
#include <ntqdir.h>
#include <ntqfileinfo.h>
#include <ntqstringlist.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

SplashInstallerEngine::SplashInstallerEngine(TQObject *parent, const char *name)
    : TQObject(parent, name),
      m_isRunning(false),
      m_currentAction(ActionNone),
      m_process(0),
      m_tempScriptPath(""),
      m_binaryPath("")
{
}

SplashInstallerEngine::~SplashInstallerEngine() {
    cancel();
    cleanupTempScript();
}

void SplashInstallerEngine::log(const TQString &msg) {
    emit logMessage(msg);
}

void SplashInstallerEngine::cancel() {
    if (m_process && m_process->isRunning()) {
        m_process->kill();
    }
    m_isRunning = false;
    m_currentAction = ActionNone;
    cleanupTempScript();
}

void SplashInstallerEngine::cleanupTempScript() {
    if (!m_tempScriptPath.isEmpty() && TQFile::exists(m_tempScriptPath)) {
        unlink(m_tempScriptPath.latin1());
        m_tempScriptPath = "";
    }
}

bool SplashInstallerEngine::checkKernelCmdline(bool &outQuiet, bool &outSplash) const {
    outQuiet = false;
    outSplash = false;

    TQFile f("/proc/cmdline");
    if (!f.open(IO_ReadOnly)) return false;

    TQTextStream ts(&f);
    TQString cmdline = ts.readLine();
    f.close();

    TQStringList tokens = TQStringList::split(' ', cmdline);
    for (TQStringList::Iterator it = tokens.begin(); it != tokens.end(); ++it) {
        if (*it == "quiet") outQuiet = true;
        if (*it == "splash") outSplash = true;
    }
    return true;
}

bool SplashInstallerEngine::checkSilentBootConfig(int &outConfigured, int &outTotalRequired, bool &outKernelQuiet) const {
    outConfigured = 0;
    outTotalRequired = 6;
    outKernelQuiet = false;

    // 1. Check active kernel cmdline
    TQFile fProc("/proc/cmdline");
    if (fProc.open(IO_ReadOnly)) {
        TQTextStream ts(&fProc);
        TQString cmd = ts.readLine();
        fProc.close();
        TQStringList toks = TQStringList::split(' ', cmd.stripWhiteSpace());
        for (TQStringList::Iterator it = toks.begin(); it != toks.end(); ++it) {
            if (*it == "quiet") {
                outKernelQuiet = true;
                break;
            }
        }
    }

    // 2. Check /etc/default/grub (reads both GRUB_CMDLINE_LINUX_DEFAULT and GRUB_CMDLINE_LINUX)
    TQFile fGrub("/etc/default/grub");
    if (!fGrub.open(IO_ReadOnly)) {
        return false;
    }

    TQString grubContent = "";
    TQTextStream tsGrub(&fGrub);
    while (!tsGrub.atEnd()) {
        TQString line = tsGrub.readLine().stripWhiteSpace();
        if (line.startsWith("GRUB_CMDLINE_LINUX_DEFAULT=") || line.startsWith("GRUB_CMDLINE_LINUX=")) {
            int eq = line.find('=');
            TQString val = line.mid(eq + 1).stripWhiteSpace();
            if ((val.startsWith("\"") && val.endsWith("\"")) || (val.startsWith("'") && val.endsWith("'"))) {
                val = val.mid(1, val.length() - 2);
            }
            grubContent += " " + val;
        }
    }
    fGrub.close();

    TQStringList grubTokens = TQStringList::split(' ', grubContent.stripWhiteSpace());

    struct CheckParam {
        const char *prefix;
        bool isPrefix;
    } params[6] = {
        {"quiet", false},
        {"loglevel=", true},
        {"systemd.show_status=", true},
        {"rd.systemd.show_status=", true},
        {"rd.udev.log_level=", true},
        {"vt.cur_default=", true}
    };

    for (int i = 0; i < 6; ++i) {
        bool found = false;
        for (TQStringList::Iterator it = grubTokens.begin(); it != grubTokens.end(); ++it) {
            if (params[i].isPrefix) {
                if (it->startsWith(params[i].prefix)) {
                    found = true;
                    break;
                }
            } else {
                if (*it == params[i].prefix) {
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            outConfigured++;
        }
    }

    return true;
}

bool SplashInstallerEngine::checkPlymouthInstalled(bool &outInstalled, bool &outActive) const {
    outInstalled = TQFile::exists("/usr/sbin/plymouthd") || 
                   TQFile::exists("/sbin/plymouthd") ||
                   TQFile::exists("/usr/bin/plymouth") || 
                   TQFile::exists("/bin/plymouth") ||
                   (system("which plymouth >/dev/null 2>&1") == 0);
    outActive = false;
    if (outInstalled) {
        outActive = (system("pgrep -x plymouthd >/dev/null 2>&1") == 0);
    }
    return outInstalled;
}

bool SplashInstallerEngine::checkGccInstalled(TQString &outGccPath) const {
    outGccPath = "";
    const char *paths[] = {"/usr/bin/gcc", "/usr/local/bin/gcc", "/bin/gcc", 0};
    for (int i = 0; paths[i]; ++i) {
        if (TQFile::exists(paths[i])) {
            outGccPath = paths[i];
            return true;
        }
    }
    FILE *fp = popen("which gcc 2>/dev/null", "r");
    if (fp) {
        char buf[512];
        if (fgets(buf, sizeof(buf), fp)) {
            TQString s = TQString::fromLocal8Bit(buf).stripWhiteSpace();
            pclose(fp);
            if (!s.isEmpty() && TQFile::exists(s)) {
                outGccPath = s;
                return true;
            }
        } else {
            pclose(fp);
        }
    }
    return false;
}

bool SplashInstallerEngine::isSplashInstalled(TQString &outInstalledBinary) const {
    outInstalledBinary = "";

    // Check hooks directory
    TQDir hooksDir("/etc/initramfs-tools/hooks");
    if (hooksDir.exists()) {
        TQStringList entries = hooksDir.entryList("xbs_*", TQDir::Files);
        if (!entries.isEmpty()) {
            outInstalledBinary = entries.first();
            return true;
        }
    }

    // Check sbin
    TQDir sbinDir("/sbin");
    if (sbinDir.exists()) {
        TQStringList entries = sbinDir.entryList("xbs_*", TQDir::Files);
        if (!entries.isEmpty()) {
            outInstalledBinary = entries.first();
            return true;
        }
    }

    // Check shutdown
    TQDir shutdownDir("/lib/systemd/system-shutdown");
    if (shutdownDir.exists()) {
        TQStringList entries = shutdownDir.entryList("xbs_*", TQDir::Files);
        if (!entries.isEmpty()) {
            outInstalledBinary = entries.first() + " (shutdown)";
            return true;
        }
    }

    return false;
}

bool SplashInstallerEngine::writeTempScript(const TQString &scriptContent, TQString &outPath) {
    char tempPattern[] = "/tmp/xbs_helper_XXXXXX";
    int fd = mkstemp(tempPattern);
    if (fd < 0) return false;
    close(fd);

    outPath = TQString(tempPattern) + ".sh";
    rename(tempPattern, outPath.latin1());

    TQFile f(outPath);
    if (!f.open(IO_WriteOnly | IO_Truncate)) {
        return false;
    }
    TQTextStream ts(&f);
    ts << scriptContent;
    f.close();

    chmod(outPath.latin1(), 0755);
    return true;
}

bool SplashInstallerEngine::startInstall(const TQString &binaryPath, InstallTarget target, bool useDrm, const TQString &sudoPassword) {
    if (m_isRunning) {
        log("[ERROR] An operation is already in progress.");
        return false;
    }

    if (!TQFile::exists(binaryPath)) {
        log("[ERROR] Source binary does not exist: " + binaryPath);
        return false;
    }

    m_binaryPath = binaryPath;
    m_sudoPassword = sudoPassword;
    m_currentAction = ActionInstall;

    TQString targetStr = "both";
    if (target == TargetBoot) targetStr = "boot";
    else if (target == TargetShutdown) targetStr = "shutdown";

    TQString script =
        "#!/bin/bash\n"
        "set -e\n"
        "export LC_ALL=C\n\n"
        "SRC_BIN=\"$1\"\n"
        "TARGET_TYPE=\"$2\"\n"
        "USE_DRM=\"$3\"\n\n"
        "if [ -z \"$SRC_BIN\" ] || [ ! -f \"$SRC_BIN\" ]; then\n"
        "    echo \"[ERROR] Source binary does not exist: $SRC_BIN\" >&2\n"
        "    exit 1\n"
        "fi\n\n"
        "BIN_NAME=\"$(basename -- \"$SRC_BIN\")\"\n"
        "if [[ ! \"$BIN_NAME\" =~ ^[A-Za-z0-9_]+$ ]]; then\n"
        "    echo \"[ERROR] Invalid binary name: $BIN_NAME (only A-Z, a-z, 0-9, _)\" >&2\n"
        "    exit 1\n"
        "fi\n\n"
        "if [ -z \"$USE_DRM\" ]; then\n"
        "    if ldd \"$SRC_BIN\" 2>/dev/null | grep -q libdrm; then\n"
        "        USE_DRM=1\n"
        "    else\n"
        "        USE_DRM=0\n"
        "    fi\n"
        "fi\n\n"
        "echo \"==================================================\"\n"
        "echo \"  XBootsplash - Autonomous Installation\"\n"
        "echo \"==================================================\"\n"
        "echo \"Binary       : $BIN_NAME\"\n"
        "echo \"Source path  : $SRC_BIN\"\n"
        "echo \"Target       : $TARGET_TYPE\"\n"
        "echo \"Mode         : $( [ \"$USE_DRM\" = \"1\" ] && echo 'DRM/KMS' || echo 'Framebuffer fbdev' )\"\n"
        "echo \"[1/5] Purging any previous bootsplash installations...\"\n"
        "for p in /etc/initramfs-tools/hooks/xbs_* /etc/initramfs-tools/hooks/xbootsplash* \\\n"
        "         /etc/initramfs-tools/scripts/init-top/xbs_* /etc/initramfs-tools/scripts/init-top/xbootsplash* \\\n"
        "         /etc/initramfs-tools/scripts/init-bottom/xbs_* /etc/initramfs-tools/scripts/init-bottom/xbootsplash* \\\n"
        "         /etc/initramfs-tools/scripts/local-premount/00_xbs_* /etc/initramfs-tools/scripts/local-premount/00_xbootsplash* \\\n"
        "         /sbin/xbs_* /sbin/xbootsplash* \\\n"
        "         /lib/systemd/system-shutdown/xbs_* /lib/systemd/system-shutdown/xbootsplash*; do\n"
        "    if [ -e \"$p\" ]; then\n"
        "        PNAME=\"$(basename \"$p\")\"\n"
        "        if [ \"$PNAME\" != \"$BIN_NAME\" ]; then\n"
        "            echo \"  Removing previous component: $p\"\n"
        "            rm -f \"$p\" \"${p}.bak\" 2>/dev/null || true\n"
        "            rm -f \"/run/${PNAME}.pid\" \"/run/${PNAME}.start_time\" 2>/dev/null || true\n"
        "        fi\n"
        "    fi\n"
        "done\n\n"
        "if [ \"$TARGET_TYPE\" = \"boot\" ] || [ \"$TARGET_TYPE\" = \"both\" ]; then\n"
        "    echo \"[2/5] Installing binary to /sbin/$BIN_NAME...\"\n"
        "    mkdir -p /sbin\n"
        "    cp -f \"$SRC_BIN\" \"/sbin/$BIN_NAME\"\n"
        "    chmod 755 \"/sbin/$BIN_NAME\"\n\n"
        "    echo \"[3/5] Creating initramfs-tools hook...\"\n"
        "    mkdir -p /etc/initramfs-tools/hooks\n"
        "    HOOK_FILE=\"/etc/initramfs-tools/hooks/$BIN_NAME\"\n"
        "    cat > \"$HOOK_FILE\" << 'HOOK_EOF'\n"
        "#!/bin/sh\n"
        "PREREQ=\"\"\n"
        "prereqs() { echo \"$PREREQ\"; }\n"
        "case \"$1\" in prereqs) prereqs; exit 0;; esac\n\n"
        ". /usr/share/initramfs-tools/hook-functions\n\n"
        "copy_exec /sbin/__BIN_NAME__ /sbin\n"
        "HOOK_EOF\n"
        "    sed -i \"s/__BIN_NAME__/$BIN_NAME/g\" \"$HOOK_FILE\"\n\n"
        "    if [ \"$USE_DRM\" = \"1\" ]; then\n"
        "        cat >> \"$HOOK_FILE\" << 'HOOK_DRM'\n"
        "mkdir -p \"${DESTDIR}/dev/dri\" 2>/dev/null || true\n"
        "HOOK_DRM\n"
        "    else\n"
        "        cat >> \"$HOOK_FILE\" << 'HOOK_FBDEV'\n"
        "if [ ! -e \"${DESTDIR}/dev/fb0\" ]; then\n"
        "    mknod \"${DESTDIR}/dev/fb0\" c 29 0 2>/dev/null || true\n"
        "fi\n"
        "HOOK_FBDEV\n"
        "    fi\n"
        "    chmod 755 \"$HOOK_FILE\"\n\n"
        "    echo \"[4/5] Creating init-top, init-bottom, and hibernation resume scripts...\"\n"
        "    mkdir -p /etc/initramfs-tools/scripts/init-top\n"
        "    mkdir -p /etc/initramfs-tools/scripts/init-bottom\n"
        "    mkdir -p /etc/initramfs-tools/scripts/local-premount\n\n"
        "    TOP_FILE=\"/etc/initramfs-tools/scripts/init-top/$BIN_NAME\"\n"
        "    PREREQ_VAL=\"\"\n"
        "    [ \"$USE_DRM\" = \"1\" ] && PREREQ_VAL=\"udev\"\n"
        "    cat > \"$TOP_FILE\" << TOP_EOF\n"
        "#!/bin/sh\n"
        "PREREQ=\"$PREREQ_VAL\"\n"
        "prereqs() { echo \"\\$PREREQ\"; }\n"
        "case \"\\$1\" in prereqs) prereqs; exit 0;; esac\n\n"
        ". /scripts/functions\n\n"
        "TOP_EOF\n\n"
        "    if [ \"$USE_DRM\" = \"0\" ]; then\n"
        "        cat >> \"$TOP_FILE\" << 'TOP_FBDEV'\n"
        "if [ ! -c /dev/fb0 ]; then\n"
        "    if [ ! -d /sys/class/graphics/fb0 ]; then\n"
        "        exit 0\n"
        "    fi\n"
        "    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do\n"
        "        sleep 0.2\n"
        "        [ -c /dev/fb0 ] && break\n"
        "    done\n"
        "    if [ ! -c /dev/fb0 ] && [ -d /sys/class/graphics/fb0 ]; then\n"
        "        mknod /dev/fb0 c 29 0 2>/dev/null || true\n"
        "    fi\n"
        "    if [ ! -c /dev/fb0 ]; then\n"
        "        exit 0\n"
        "    fi\n"
        "fi\n"
        "TOP_FBDEV\n"
        "    fi\n\n"
        "    cat >> \"$TOP_FILE\" << TOP_RUN\n"
        "if [ -x /sbin/$BIN_NAME ]; then\n"
        "    /sbin/$BIN_NAME &\n"
        "    SPLASH_PID=\\$!\n"
        "    sleep 0.1\n"
        "    if [ -d \"/proc/\\$SPLASH_PID\" ]; then\n"
        "        echo \"\\$SPLASH_PID\" > /run/${BIN_NAME}.pid\n"
        "        START_TIME=\\$(awk '{print \\$22}' /proc/\\$SPLASH_PID/stat 2>/dev/null)\n"
        "        echo \"\\$START_TIME\" > /run/${BIN_NAME}.start_time\n"
        "    fi\n"
        "fi\n"
        "TOP_RUN\n"
        "    chmod 755 \"$TOP_FILE\"\n\n"
        "    BOTTOM_FILE=\"/etc/initramfs-tools/scripts/init-bottom/$BIN_NAME\"\n"
        "    cat > \"$BOTTOM_FILE\" << BOTTOM_EOF\n"
        "#!/bin/sh\n"
        "PREREQ=\"\"\n"
        "prereqs() { echo \"\\$PREREQ\"; }\n"
        "case \"\\$1\" in prereqs) prereqs; exit 0;; esac\n\n"
        ". /scripts/functions\n\n"
        "BOTTOM_EOF\n\n"
        "    if [ \"$USE_DRM\" = \"1\" ]; then\n"
        "        cat >> \"$BOTTOM_FILE\" << BOTTOM_DRM\n"
        "if [ -x /sbin/$BIN_NAME ]; then\n"
        "    /sbin/$BIN_NAME --restore-crtc 2>/dev/null || true\n"
        "fi\n"
        "BOTTOM_DRM\n"
        "    fi\n\n"
        "    cat >> \"$BOTTOM_FILE\" << BOTTOM_KILL\n"
        "if [ -f /run/${BIN_NAME}.pid ]; then\n"
        "    PID=\\$(cat /run/${BIN_NAME}.pid 2>/dev/null)\n"
        "    if [ -n \"\\$PID\" ] && [ -d \"/proc/\\$PID\" ]; then\n"
        "        COMM=\\$(cat /proc/\\$PID/comm 2>/dev/null)\n"
        "        if [ \"\\$COMM\" = \"$BIN_NAME\" ]; then\n"
        "            SAVED_START=\\$(cat /run/${BIN_NAME}.start_time 2>/dev/null)\n"
        "            CURRENT_START=\\$(awk '{print \\$22}' /proc/\\$PID/stat 2>/dev/null)\n"
        "            if [ -n \"\\$SAVED_START\" ] && [ \"\\$SAVED_START\" = \"\\$CURRENT_START\" ]; then\n"
        "                kill \"\\$PID\" 2>/dev/null || true\n"
        "                for _i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40; do\n"
        "                    [ -d \"/proc/\\$PID\" ] || break\n"
        "                    sleep 0.1\n"
        "                done\n"
        "                if [ -d \"/proc/\\$PID\" ]; then\n"
        "                    kill -9 \"\\$PID\" 2>/dev/null || true\n"
        "                fi\n"
        "            fi\n"
        "        fi\n"
        "    fi\n"
        "    rm -f /run/${BIN_NAME}.pid /run/${BIN_NAME}.start_time\n"
        "fi\n"
        "BOTTOM_KILL\n\n"
        "    if [ \"$USE_DRM\" = \"0\" ]; then\n"
        "        cat >> \"$BOTTOM_FILE\" << 'BOTTOM_FBDEV'\n"
        "if [ -c /dev/fb0 ]; then\n"
        "    dd if=/dev/zero of=/dev/fb0 2>/dev/null || true\n"
        "fi\n"
        "BOTTOM_FBDEV\n"
        "    fi\n"
        "    chmod 755 \"$BOTTOM_FILE\"\n\n"
        "    RESUME_FILE=\"/etc/initramfs-tools/scripts/local-premount/00_$BIN_NAME\"\n"
        "    cat > \"$RESUME_FILE\" << 'RESUME_EOF'\n"
        "#!/bin/sh\n"
        "# Early notification for __BIN_NAME__ when resuming from hibernation (swsusp)\n"
        "PREREQ=\"\"\n"
        "prereqs() { echo \"$PREREQ\"; }\n"
        "case \"$1\" in prereqs) prereqs; exit 0;; esac\n\n"
        "[ -z \"${resume?}\" ] || [ ! -e /sys/power/resume ] && exit 0\n\n"
        ". /scripts/functions\n"
        ". /scripts/local\n\n"
        "if ! local_device_setup \"${resume}\" \"suspend/resume device\" false; then\n"
        "    exit 0\n"
        "fi\n\n"
        "if [ \"$(get_fstype \"${DEV}\")\" = \"suspend\" ]; then\n"
        "    if [ -f /run/__BIN_NAME__.pid ]; then\n"
        "        PID=$(cat /run/__BIN_NAME__.pid 2>/dev/null)\n"
        "        if [ -n \"$PID\" ] && [ -d \"/proc/$PID\" ]; then\n"
        "            COMM=$(cat /proc/$PID/comm 2>/dev/null)\n"
        "            if [ \"$COMM\" = \"__BIN_NAME__\" ]; then\n"
        "                rm -f /run/xbs_resume_ready 2>/dev/null || true\n"
        "                kill -USR1 \"$PID\" 2>/dev/null || true\n"
        "                for _w in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30; do\n"
        "                    [ -f /run/xbs_resume_ready ] && break\n"
        "                    [ -d \"/proc/$PID\" ] || break\n"
        "                    sleep 0.1\n"
        "                done\n"
        "                rm -f /run/xbs_resume_ready 2>/dev/null || true\n"
        "            fi\n"
        "        fi\n"
        "    fi\n"
        "fi\n"
        "RESUME_EOF\n"
        "    sed -i \"s/__BIN_NAME__/$BIN_NAME/g\" \"$RESUME_FILE\"\n"
        "    chmod 755 \"$RESUME_FILE\"\n"
        "fi\n\n"
        "if [ \"$TARGET_TYPE\" = \"shutdown\" ] || [ \"$TARGET_TYPE\" = \"both\" ]; then\n"
        "    echo \"Installing shutdown splash script to /lib/systemd/system-shutdown...\"\n"
        "    mkdir -p /lib/systemd/system-shutdown\n"
        "    mkdir -p /sbin\n"
        "    cp -f \"$SRC_BIN\" \"/sbin/$BIN_NAME\"\n"
        "    chmod 755 \"/sbin/$BIN_NAME\"\n"
        "    cat > /lib/systemd/system-shutdown/bootsplash.shutdown << 'SHUTDOWN_EOF'\n"
        "#!/bin/sh\n"
        "# Bootsplash shutdown animation wrapper\n"
        "# Strict timeout ensures poweroff or reboot is NEVER blocked\n"
        "for bin in /sbin/xbs_* /sbin/xbootsplash*; do\n"
        "    if [ -x \"$bin\" ]; then\n"
        "        SHUTDOWN_MODE=1 timeout 3s \"$bin\" 2>/dev/null || true\n"
        "        break\n"
        "    fi\n"
        "done\n"
        "SHUTDOWN_EOF\n"
        "    chmod 755 /lib/systemd/system-shutdown/bootsplash.shutdown\n"
        "fi\n\n"
        "if [ \"$TARGET_TYPE\" = \"boot\" ] || [ \"$TARGET_TYPE\" = \"both\" ]; then\n"
        "    echo \"[5/5] Regenerating initramfs (update-initramfs -u)...\"\n"
        "    update-initramfs -u\n"
        "fi\n\n"
        "echo \"\"\n"
        "echo \"SUCCESS: Bootsplash installation completed successfully!\"\n"
        "exit 0\n";

    if (!writeTempScript(script, m_tempScriptPath)) {
        log(SplashLog::banner("INSTALL"));
        log("[ERROR] Unable to create temporary installation script.");
        log(SplashLog::failedBanner());
        m_isRunning = false;
        m_currentAction = ActionNone;
        return false;
    }

    if (m_process) {
        delete m_process;
        m_process = 0;
    }

    m_process = new TQProcess(this);
    connect(m_process, SIGNAL(readyReadStdout()), this, SLOT(onProcessReadyReadStdout()));
    connect(m_process, SIGNAL(readyReadStderr()), this, SLOT(onProcessReadyReadStderr()));
    connect(m_process, SIGNAL(processExited()), this, SLOT(onProcessExited()));

    TQStringList args;
    if (getuid() != 0) {
        args << "sudo" << "-S" << "-p" << "";
    }
    args << m_tempScriptPath;
    args << m_binaryPath;
    args << targetStr;
    args << (useDrm ? "1" : "0");

    m_process->setArguments(args);

    m_isRunning = true;
    emit installStarted();
    log(SplashLog::banner("INSTALL"));
    log(TQString("Target binary : %1").arg(m_binaryPath));
    log(TQString("Target mode   : %1").arg(targetStr));
    log(TQString("Backend       : %1").arg(useDrm ? "DRM/KMS" : "Framebuffer"));
    log(TQString("Starting autonomous installation (%1)...")
        .arg(getuid() == 0 ? "direct root" : "via sudo -S"));

    if (!m_process->start()) {
        log("[ERROR] Failed to start installation process.");
        log(SplashLog::failedBanner());
        cleanupTempScript();
        m_isRunning = false;
        m_currentAction = ActionNone;
        emit installFinished(false, "Failed to execute sudo.");
        return false;
    }

    if (getuid() != 0 && !m_sudoPassword.isEmpty()) {
        m_process->writeToStdin(m_sudoPassword + "\n");
    }
    return true;
}

bool SplashInstallerEngine::startUninstall(const TQString &sudoPassword) {
    if (m_isRunning) {
        log("[ERROR] An operation is already in progress.");
        return false;
    }

    m_sudoPassword = sudoPassword;
    m_currentAction = ActionUninstall;

    TQString script =
        "#!/bin/bash\n"
        "set -e\n"
        "export LC_ALL=C\n\n"
        "echo \"==================================================\"\n"
        "echo \"  XBootsplash - Autonomous Uninstallation\"\n"
        "echo \"==================================================\"\n\n"
        "REMOVED=0\n\n"
        "for bin in /sbin/xbs_* /sbin/xbootsplash*; do\n"
        "    if [ -f \"$bin\" ]; then\n"
        "        BNAME=\"$(basename \"$bin\")\"\n"
        "        echo \"Removing splash: $BNAME\"\n"
        "        rm -f \"/sbin/$BNAME\" \"/sbin/${BNAME}.bak\" 2>/dev/null || true\n"
        "        rm -f \"/etc/initramfs-tools/hooks/$BNAME\" \"/etc/initramfs-tools/hooks/${BNAME}.bak\" 2>/dev/null || true\n"
        "        rm -f \"/etc/initramfs-tools/scripts/init-top/$BNAME\" \"/etc/initramfs-tools/scripts/init-top/${BNAME}.bak\" 2>/dev/null || true\n"
        "        rm -f \"/etc/initramfs-tools/scripts/init-bottom/$BNAME\" \"/etc/initramfs-tools/scripts/init-bottom/${BNAME}.bak\" 2>/dev/null || true\n"
        "        rm -f \"/etc/initramfs-tools/scripts/local-premount/00_$BNAME\" \"/etc/initramfs-tools/scripts/local-premount/00_${BNAME}.bak\" 2>/dev/null || true\n"
        "        rm -f \"/run/${BNAME}.pid\" \"/run/${BNAME}.start_time\" \"/run/xbs_drm_crtc.info\" 2>/dev/null || true\n"
        "        REMOVED=$((REMOVED + 1))\n"
        "    fi\n"
        "done\n\n"
        "for h in /etc/initramfs-tools/hooks/xbs_* /etc/initramfs-tools/hooks/xbootsplash* \\\n"
        "         /etc/initramfs-tools/scripts/init-top/xbs_* /etc/initramfs-tools/scripts/init-top/xbootsplash* \\\n"
        "         /etc/initramfs-tools/scripts/init-bottom/xbs_* /etc/initramfs-tools/scripts/init-bottom/xbootsplash* \\\n"
        "         /etc/initramfs-tools/scripts/local-premount/00_xbs_* /etc/initramfs-tools/scripts/local-premount/00_xbootsplash*; do\n"
        "    if [ -f \"$h\" ]; then\n"
        "        rm -f \"$h\" \"${h}.bak\" 2>/dev/null || true\n"
        "    fi\n"
        "done\n\n"
        "for bin in /lib/systemd/system-shutdown/xbs_* /lib/systemd/system-shutdown/xbootsplash*; do\n"
        "    if [ -f \"$bin\" ]; then\n"
        "        echo \"Removing shutdown binary: $(basename \"$bin\")\"\n"
        "        rm -f \"$bin\" \"${bin}.bak\" 2>/dev/null || true\n"
        "        REMOVED=$((REMOVED + 1))\n"
        "    fi\n"
        "done\n\n"
        "if [ -f /lib/systemd/system-shutdown/bootsplash.shutdown ]; then\n"
        "    rm -f /lib/systemd/system-shutdown/bootsplash.shutdown\n"
        "    echo \"Removing /lib/systemd/system-shutdown/bootsplash.shutdown\"\n"
        "fi\n\n"
        "echo \"Regenerating initramfs without bootsplash (update-initramfs -u)...\"\n"
        "update-initramfs -u\n\n"
        "echo \"\"\n"
        "echo \"SUCCESS: Uninstallation completed successfully!\"\n"
        "exit 0\n";

    if (!writeTempScript(script, m_tempScriptPath)) {
        log(SplashLog::banner("UNINSTALL"));
        log("[ERROR] Unable to create temporary uninstallation script.");
        log(SplashLog::failedBanner());
        m_isRunning = false;
        m_currentAction = ActionNone;
        return false;
    }

    if (m_process) {
        delete m_process;
        m_process = 0;
    }

    m_process = new TQProcess(this);
    connect(m_process, SIGNAL(readyReadStdout()), this, SLOT(onProcessReadyReadStdout()));
    connect(m_process, SIGNAL(readyReadStderr()), this, SLOT(onProcessReadyReadStderr()));
    connect(m_process, SIGNAL(processExited()), this, SLOT(onProcessExited()));

    TQStringList args;
    if (getuid() != 0) {
        args << "sudo" << "-S" << "-p" << "";
    }
    args << m_tempScriptPath;

    m_process->setArguments(args);

    m_isRunning = true;
    emit uninstallStarted();
    log(SplashLog::banner("UNINSTALL"));
    log(TQString("Starting autonomous uninstallation (%1)...")
        .arg(getuid() == 0 ? "direct root" : "via sudo -S"));

    if (!m_process->start()) {
        log("[ERROR] Failed to start uninstallation process.");
        log(SplashLog::failedBanner());
        cleanupTempScript();
        m_isRunning = false;
        m_currentAction = ActionNone;
        emit uninstallFinished(false, "Failed to execute sudo.");
        return false;
    }

    if (getuid() != 0 && !m_sudoPassword.isEmpty()) {
        m_process->writeToStdin(m_sudoPassword + "\n");
    }
    return true;
}

void SplashInstallerEngine::onProcessReadyReadStdout() {
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

void SplashInstallerEngine::onProcessReadyReadStderr() {
    if (!m_process) return;
    TQByteArray data = m_process->readStderr();
    if (!data.isEmpty()) {
        TQString text = TQString::fromLocal8Bit(data.data(), data.size());
        TQStringList lines = TQStringList::split('\n', text);
        for (TQStringList::Iterator it = lines.begin(); it != lines.end(); ++it) {
            TQString l = (*it).stripWhiteSpace();
            if (!l.isEmpty()) log("  " + l);
        }
    }
}

void SplashInstallerEngine::onProcessExited() {
    if (!m_process) return;

    onProcessReadyReadStdout();
    onProcessReadyReadStderr();

    bool normal = m_process->normalExit();
    int code = m_process->exitStatus();
    cleanupTempScript();

    m_isRunning = false;
    m_sudoPassword = "";
    Action act = m_currentAction;
    m_currentAction = ActionNone;

    if (normal && code == 0) {
        if (act == ActionInstall) {
            log("✔ Bootsplash installation completed successfully.");
            log(SplashLog::doneBanner());
            emit installFinished(true, "The bootsplash was installed successfully into the initramfs!");
        } else if (act == ActionUninstall) {
            log("✔ Bootsplash uninstallation completed successfully.");
            log(SplashLog::doneBanner());
            emit uninstallFinished(true, "The bootsplash was uninstalled and initramfs updated.");
        }
    } else {
        TQString err = TQString("Operation failed (exit code: %1).").arg(code);
        log(TQString("[ERROR] ") + err);
        log(SplashLog::failedBanner());
        if (act == ActionInstall) {
            emit installFinished(false, err);
        } else if (act == ActionUninstall) {
            emit uninstallFinished(false, err);
        }
    }
}

#include "installer_engine.moc"
