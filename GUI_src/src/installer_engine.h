#ifndef SPLASH_INSTALLER_ENGINE_H
#define SPLASH_INSTALLER_ENGINE_H

#include <ntqobject.h>
#include <ntqstring.h>
#include <ntqprocess.h>

class SplashInstallerEngine : public TQObject {
    TQ_OBJECT

public:
    enum InstallTarget {
        TargetBoot = 1,
        TargetShutdown = 2,
        TargetBoth = 3
    };

    explicit SplashInstallerEngine(TQObject *parent = 0, const char *name = 0);
    virtual ~SplashInstallerEngine();

    bool isRunning() const { return m_isRunning; }
    bool checkKernelCmdline(bool &outQuiet, bool &outSplash) const;
    bool checkSilentBootConfig(int &outConfigured, int &outTotalRequired, bool &outKernelQuiet) const;
    bool checkPlymouthInstalled(bool &outInstalled, bool &outActive) const;
    bool checkGccInstalled(TQString &outGccPath) const;
    bool isSplashInstalled(TQString &outInstalledBinary) const;

    bool startInstall(const TQString &binaryPath, InstallTarget target, bool useDrm, const TQString &sudoPassword = "");
    bool startUninstall(const TQString &sudoPassword = "");
    void cancel();

signals:
    void logMessage(const TQString &msg);
    void installStarted();
    void installFinished(bool success, const TQString &message);
    void uninstallStarted();
    void uninstallFinished(bool success, const TQString &message);

private slots:
    void onProcessReadyReadStdout();
    void onProcessReadyReadStderr();
    void onProcessExited();

private:
    void log(const TQString &msg);
    TQString detectElevationTool() const;
    bool writeTempScript(const TQString &scriptContent, TQString &outPath);
    void cleanupTempScript();

    enum Action {
        ActionNone,
        ActionInstall,
        ActionUninstall
    };

    bool m_isRunning;
    Action m_currentAction;
    TQProcess *m_process;
    TQString m_tempScriptPath;
    TQString m_binaryPath;
    TQString m_sudoPassword;
};

#endif // SPLASH_INSTALLER_ENGINE_H
