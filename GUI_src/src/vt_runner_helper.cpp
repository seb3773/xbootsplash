/*
 * vt_runner_helper.cpp - Implementation of safe hardware VT bootsplash testing
 */

#include "vt_runner_helper.h"
#include "app_icons.h"
#include "password_dialog.h"

#include <ntqfile.h>
#include <ntqfileinfo.h>
#include <ntqtextstream.h>
#include <ntqapplication.h>
#include <ntqmessagebox.h>

#include "embedded_vt_runner.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

TQString findVtRunnerBinary(const TQString & /* projectRoot */) {
    if (xbs_vt_runner_len == 0) {
        return TQString::null;
    }

    uid_t uid = getuid();
    char targetPath[256];
    if (access(TQString("/run/user/%1").arg(uid).latin1(), W_OK) == 0) {
        snprintf(targetPath, sizeof(targetPath), "/run/user/%d/xbs_vt_runner", (int)uid);
    } else {
        snprintf(targetPath, sizeof(targetPath), "/tmp/xbs_vt_runner_%d", (int)uid);
    }

    bool needsWrite = true;
    TQFileInfo fi(targetPath);
    if (fi.exists() && fi.size() == (int)xbs_vt_runner_len && (access(targetPath, X_OK) == 0)) {
        needsWrite = false;
    }

    if (needsWrite) {
        unlink(targetPath);
        int fd = open(targetPath, O_WRONLY | O_CREAT | O_TRUNC, 0755);
        if (fd >= 0) {
            ssize_t written = write(fd, xbs_vt_runner, xbs_vt_runner_len);
            close(fd);
            chmod(targetPath, 0755);
            if (written == (ssize_t)xbs_vt_runner_len) {
                return TQString(targetPath);
            }
        }
    } else {
        return TQString(targetPath);
    }

    return TQString::null;
}

bool executeVtLiveTest(TQWidget *parent,
                       const TQString &projectRoot,
                       const TQString &binaryPath,
                       const TQString &displayName,
                       TQString &outLogOutput)
{
    outLogOutput = "";

    if (binaryPath.isEmpty() || !TQFile::exists(binaryPath)) {
        showWarning(parent, "Target Binary Not Found",
                    "The specified bootsplash binary was not found:\n" + binaryPath +
                    "\n\nPlease build the bootsplash binary first.");
        return false;
    }

    TQString runnerPath = findVtRunnerBinary(projectRoot);
    if (runnerPath.isEmpty()) {
        showWarning(parent, "VT Runner Error",
                    "Unable to extract embedded VT runner to RAM/tmpfs.");
        return false;
    }

    TQString disp = displayName.isEmpty() ? TQFileInfo(binaryPath).fileName() : displayName;

    // Preventive modal confirmation dialog
    TQString promptMsg =
        "The screen will temporarily switch to an isolated virtual terminal (VT)\n"
        "to run and verify the bootsplash binary directly on the hardware\n"
        "(framebuffer / DRM-KMS) at its actual native framerate.\n\n"
        "- Target binary  : " + disp + "\n"
        "- Max duration   : 10 seconds (guaranteed automatic return)\n"
        "- Instant exit   : press any key or Ctrl+C\n"
        "- Safety         : your desktop session (X11/Wayland) will be\n"
        "                   restored automatically to its initial state.\n\n"
        "Do you want to start the live hardware test now?";

    int choice = showWarning(parent,
                             "Live Hardware Test (Virtual Terminal)",
                             promptMsg,
                             "Start Test",
                             "Cancel",
                             TQString::null,
                             0, 1);

    if (choice != 0) {
        return false;
    }

    // Authentication check
    TQString sudoPassword;
    if (getuid() != 0) {
        if (!SplashPasswordDialog::getPassword(parent, sudoPassword)) {
            return false;
        }
    }

    TQApplication::setOverrideCursor(TQt::waitCursor);

    TQString logFile = "/tmp/xbs_vt_test.log";
    unlink(logFile.latin1());

    TQString cmd;
    if (getuid() == 0) {
        cmd = TQString("\"%1\" \"%2\" 10 > \"%3\" 2>&1").arg(runnerPath).arg(binaryPath).arg(logFile);
    } else {
        cmd = TQString("sudo -S -p '' \"%1\" \"%2\" 10 > \"%3\" 2>&1").arg(runnerPath).arg(binaryPath).arg(logFile);
    }

    FILE *fp = popen(cmd.latin1(), "w");
    int exitCode = -1;
    if (fp) {
        if (getuid() != 0 && !sudoPassword.isEmpty()) {
            TQCString pass = sudoPassword.local8Bit();
            fwrite(pass.data(), 1, pass.length(), fp);
            fwrite("\n", 1, 1, fp);
            fflush(fp);
        }
        int status = pclose(fp);
        exitCode = WEXITSTATUS(status);
    } else {
        exitCode = -1;
    }

    TQApplication::restoreOverrideCursor();

    // Read log output
    TQFile lf(logFile);
    if (lf.open(IO_ReadOnly)) {
        TQTextStream ts(&lf);
        outLogOutput = ts.read();
        lf.close();
        unlink(logFile.latin1());
    }

    if (exitCode == 0) {
        return true;
    }

    TQString errDetails = TQString("The live hardware test ended with exit code %1.\n").arg(exitCode);
    if (!outLogOutput.stripWhiteSpace().isEmpty()) {
        errDetails += "\nOutput details:\n" + outLogOutput.stripWhiteSpace();
    }
    showWarning(parent, "Live Test Notice", errDetails);
    return false;
}
