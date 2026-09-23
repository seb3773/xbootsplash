#ifndef SPLASH_MAINWINDOW_H
#define SPLASH_MAINWINDOW_H

#include <ntqmainwindow.h>
#include <ntqaction.h>
#include <ntqtoolbar.h>
#include <ntqtoolbutton.h>
#include <ntqstatusbar.h>
#include <ntqsplitter.h>
#include <ntqslider.h>
#include <ntqlabel.h>
#include <ntqpushbutton.h>
#include <ntqdatetime.h>

class SplashPreviewWidget;
class SplashFullscreenPreview;
class SplashConfigPanel;
class SplashBuildEngine;
class SplashPackageManager;
class SplashInstallerEngine;
class TQtMessageLogWidget;
class TQPopupMenu;

class MainWindow : public TQMainWindow {
    TQ_OBJECT

public:
    explicit MainWindow(TQWidget *parent = 0, const char *name = 0);
    virtual ~MainWindow();

    SplashConfigPanel* configPanel() const { return m_configPanel; }
    SplashPreviewWidget* previewWidget() const { return m_preview; }
    TQString projectFilePath() const { return m_currentProjectFilePath; }
    TQString projectRoot() const { return m_projectRoot; }
    void loadProject(const TQString &filePath);
    bool saveProject(const TQString &filePath);
    SplashPackageManager* packageManager() const { return m_packageMgr; }
    SplashInstallerEngine* installerEngine() const { return m_installerEngine; }
    SplashBuildEngine* buildEngine() const { return m_buildEngine; }
    void startBuild() { onStartBuild(); }

    void startBusyStatus(const TQString &baseText);
    void updateBusyStatus(const TQString &newBaseText);
    void stopBusyStatus();

    TQString currentBinaryPath() const { return currentProjectBinaryPath(); }
    bool isTestLiveEnabled() const { return m_btnTestLive ? m_btnTestLive->isEnabled() : false; }
    void simulateBuildFinished(bool success, const TQString &binaryPath, unsigned long sizeBytes, bool showDialog = false) {
        onBuildFinished(success, binaryPath, sizeBytes, showDialog);
    }
    void setSkipPlymouthDialog(bool skip) { m_skipPlymouthDialog = skip; }
    void setSkipBuildDialog(bool skip) { m_skipBuildDialog = skip; }
    TQPopupMenu* fileMenu() const { return m_fileMenu; }
    TQPopupMenu* viewMenu() const { return m_viewMenu; }
    TQPopupMenu* toolsMenu() const { return m_toolsMenu; }
    TQPopupMenu* helpMenu() const { return m_helpMenu; }
    int actionOpenFramesDir() const { return m_actionOpenFramesDir; }
    int actionOpenSingleImage() const { return m_actionOpenSingleImage; }

public slots:
    void onSilentBootConfig();
    void onLogMessage(const TQString &msg);
    void checkStartupEnvironment(bool forcePlymouth = false);
    void updateFileMenuState();

protected:
    virtual void keyPressEvent(TQKeyEvent *e);
    virtual void closeEvent(TQCloseEvent *e);

private slots:
    void onNewProject();
    void onOpenProject();
    bool onSaveProject();
    bool onSaveProjectAs();
    void onOpenFramesDir();
    void onOpenSingleImage();
    void onConfigurationChanged();
    void onImportPackage();
    void onExportPackage();
    void onTogglePlayback();
    void onStopPlayback();
    void onFirstFrame();
    void onRewindFrame();
    void onFastForwardFrame();
    void onLastFrame();
    void onPrevFrame();
    void onNextFrame();
    void onScrubFrame(int frame);
    void onLaunchFullscreen();
    void onToggleCrosshair();
    void onStartBuild();
    void onTestBinaryLive();
    void onBuildDebianPackage();
    void onInstallSplash();
    void onUninstallSplash();
    void onShowUserGuide();
    void onAbout();

    bool launchVtTest(const TQString &binaryPath, const TQString &displayName = TQString::null);

    // Engine feedback
    void onBuildStarted();
    void onBuildFinished(bool success, const TQString &binaryPath, unsigned long sizeBytes, bool showDialog = true);
    void onInstallStarted();
    void onInstallFinished(bool success, const TQString &message);
    void onUninstallStarted();
    void onUninstallFinished(bool success, const TQString &message);
    void onPreviewFrameChanged(int current, int total);
    void onPreviewPlaybackChanged(bool isPlaying);
    void onPreviewFramesLoaded(int count, int width, int height);
    void onBuildStatusChanged(const TQString &status);
    void onBusyTimerTick();

private:
    void setupMenus();
    void setupToolBars();
    void setupUI();
    void updateStatusBar();
    TQString findProjectRoot() const;

    SplashPreviewWidget *m_preview;
    SplashFullscreenPreview *m_fullscreen;
    SplashConfigPanel *m_configPanel;
    SplashBuildEngine *m_buildEngine;
    SplashPackageManager *m_packageMgr;
    SplashInstallerEngine *m_installerEngine;
    TQtMessageLogWidget *m_logWidget;

    // Controls in toolbar
    TQToolButton *m_btnPrev;
    TQToolButton *m_btnRewind;
    TQToolButton *m_btnPlay;
    TQToolButton *m_btnStop;
    TQToolButton *m_btnFastForward;
    TQToolButton *m_btnNext;
    TQToolButton *m_btnFullscreen;
    TQToolButton *m_btnCrosshair;
    TQToolButton *m_btnBuild;
    TQToolButton *m_btnTestLive;
    TQToolButton *m_btnPackage;
    TQToolButton *m_btnInstall;

    TQSlider *m_scrubSlider;
    TQLabel *m_frameCounterLabel;
    TQLabel *m_statusLabel;
    TQLabel *m_installedLabel;

    TQString m_projectRoot;
    TQString m_currentProjectFilePath;
    TQString m_lastBuiltBinary;
    bool m_isDirty;
    bool m_isProjectDirty;
    bool m_skipPlymouthDialog;
    bool m_skipBuildDialog;

    // Status bar busy spinner animation
    class TQTimer *m_busyTimer;
    int m_busyFrame;
    TQString m_busyBaseText;
    TQTime m_busyStartTime;

    void updateWindowTitle();
    bool maybeSavePrompt();
    void updateProjectBinaryState();
    TQString currentProjectBinaryPath() const;

    TQPopupMenu *m_fileMenu;
    TQPopupMenu *m_viewMenu;
    TQPopupMenu *m_toolsMenu;
    TQPopupMenu *m_helpMenu;
    int m_actionOpenFramesDir;
    int m_actionOpenSingleImage;
    int m_actionTestLive;
    int m_actionPlayPause;
};

#endif // SPLASH_MAINWINDOW_H
