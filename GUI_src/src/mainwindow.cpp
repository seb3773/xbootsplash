#include "mainwindow.h"
#include "preview_widget.h"
#include "fullscreen_preview.h"
#include "config_panel.h"
#include "build_engine.h"
#include "package_manager.h"
#include "installer_engine.h"
#include "password_dialog.h"
#include "silent_boot_dialog.h"
#include "package_inspector_dialog.h"
#include "about_dialog.h"
#include "user_guide_dialog.h"
#include "vt_runner_helper.h"
#include "app_icons.h"
#include "version.h"
#include "log_banner.h"
#include "../libs/message-log-widget/tqtmessagelogwidget.h"

#include <ntqmenubar.h>
#include <ntqpopupmenu.h>
#include <ntqfiledialog.h>
#include <ntqmessagebox.h>
#include <ntqapplication.h>
#include <ntqfileinfo.h>
#include <ntqdir.h>
#include <ntqtooltip.h>
#include <unistd.h>

MainWindow::MainWindow(TQWidget *parent, const char *name)
    : TQMainWindow(parent, name),
      m_btnTestLive(0),
      m_currentProjectFilePath(""),
      m_lastBuiltBinary(""),
      m_isDirty(true),
      m_isProjectDirty(false),
      m_skipPlymouthDialog(false),
      m_skipBuildDialog(false),
      m_busyTimer(NULL),
      m_busyFrame(0),
      m_busyBaseText(""),
      m_fileMenu(NULL),
      m_viewMenu(NULL),
      m_toolsMenu(NULL),
      m_helpMenu(NULL),
      m_actionOpenFramesDir(-1),
      m_actionOpenSingleImage(-1),
      m_actionTestLive(-1),
      m_actionPlayPause(-1)
{
    m_projectRoot = findProjectRoot();

    updateWindowTitle();
    setAppWindowIcon(this);
    resize(1280, 820);

    m_buildEngine = new SplashBuildEngine(this);
    m_packageMgr = new SplashPackageManager(this);
    m_installerEngine = new SplashInstallerEngine(this);

    connect(m_buildEngine, SIGNAL(buildStarted()), this, SLOT(onBuildStarted()));
    connect(m_buildEngine, SIGNAL(buildStatusChanged(const TQString&)),
            this, SLOT(onBuildStatusChanged(const TQString&)));
    connect(m_buildEngine, SIGNAL(buildFinished(bool, const TQString&, unsigned long)),
            this, SLOT(onBuildFinished(bool, const TQString&, unsigned long)));
    connect(m_buildEngine, SIGNAL(logMessage(const TQString&)), this, SLOT(onLogMessage(const TQString&)));

    connect(m_installerEngine, SIGNAL(installStarted()), this, SLOT(onInstallStarted()));
    connect(m_installerEngine, SIGNAL(installFinished(bool, const TQString&)),
            this, SLOT(onInstallFinished(bool, const TQString&)));
    connect(m_installerEngine, SIGNAL(uninstallStarted()), this, SLOT(onUninstallStarted()));
    connect(m_installerEngine, SIGNAL(uninstallFinished(bool, const TQString&)),
            this, SLOT(onUninstallFinished(bool, const TQString&)));
    connect(m_installerEngine, SIGNAL(logMessage(const TQString&)), this, SLOT(onLogMessage(const TQString&)));

    setupUI();
    setupMenus();
    setupToolBars();
    updateStatusBar();

    updateProjectBinaryState();
}

MainWindow::~MainWindow() {
}

TQString MainWindow::findProjectRoot() const {
    TQString cur = TQDir::currentDirPath();
    if (TQFile::exists(cur + "/engine_src/generate_splash.c") || TQFile::exists(cur + "/generate_splash.c")) return cur;
    if (TQFile::exists(cur + "/../engine_src/generate_splash.c") || TQFile::exists(cur + "/../generate_splash.c")) {
        TQDir d(cur + "/..");
        return d.canonicalPath();
    }
    if (TQFile::exists(cur + "/../../engine_src/generate_splash.c") || TQFile::exists(cur + "/../../generate_splash.c")) {
        TQDir d(cur + "/../..");
        return d.canonicalPath();
    }
    return cur;
}

void MainWindow::setupUI() {
    TQSplitter *mainSplitter = new TQSplitter(TQt::Horizontal, this);
    setCentralWidget(mainSplitter);

    m_fullscreen = new SplashFullscreenPreview(this);

    // 1. Right pane: Vertical splitter with Preview on top, Log console on bottom
    TQSplitter *rightSplitter = new TQSplitter(TQt::Vertical, mainSplitter);

    m_preview = new SplashPreviewWidget(rightSplitter);
    m_preview->show();

    m_logWidget = new TQtMessageLogWidget(rightSplitter);
    m_logWidget->setHistorySize(2000);
    m_logWidget->setMinimumHeight(140);

    TQFont logFont("Monospace", 9);
    logFont.setStyleHint(TQFont::TypeWriter);
    m_logWidget->setFont(logFont);

    const TQColor &baseCol = m_logWidget->viewport()->colorGroup().base();
    bool isDark = (baseCol.red() * 299 + baseCol.green() * 587 + baseCol.blue() * 114) / 1000 < 128;
    TQColor doneColor = isDark ? TQColor(80, 220, 100) : TQColor(20, 130, 45);
    TQColor failColor = isDark ? TQColor(255, 85, 85) : TQColor(210, 30, 30);
    TQColor headerColor = isDark ? TQColor(100, 180, 255) : TQColor(15, 95, 185);
    TQColor warnColor = isDark ? TQColor(255, 180, 50) : TQColor(200, 110, 0);

    m_logWidget->setupStyle(TQRegExp(".*(✓|\\[OK\\]).*"), TQColor(), doneColor);
    m_logWidget->setupStyle(TQRegExp(".*(✗ FAILED|x FAILED|\\[ERROR\\]).*"), TQColor(), failColor);
    m_logWidget->setupStyle(TQRegExp("^----\\s*\\[.*\\].*"), TQColor(), headerColor);
    m_logWidget->setupStyle(TQRegExp(".*(WARNING|⚠).*"), TQColor(), warnColor);

    m_logWidget->show();

    // 2. Left pane: Config panel
    m_configPanel = new SplashConfigPanel(m_preview, mainSplitter);
    m_configPanel->setMinimumWidth(380);
    m_configPanel->show();

    // Move configPanel to the left (first position in mainSplitter)
    mainSplitter->moveToFirst(m_configPanel);

    // Configure resize behavior: sidebar keeps its size on maximize, canvas stretches
    mainSplitter->setResizeMode(m_configPanel, TQSplitter::KeepSize);
    mainSplitter->setResizeMode(rightSplitter, TQSplitter::Stretch);

    rightSplitter->setResizeMode(m_preview, TQSplitter::Stretch);
    rightSplitter->setResizeMode(m_logWidget, TQSplitter::KeepSize);

    // Initial splitter sizes (490px sidebar [~29% wider], 790px canvas)
    TQValueList<int> mainSizes;
    mainSizes << 490 << 790;
    mainSplitter->setSizes(mainSizes);

    TQValueList<int> rightSizes;
    rightSizes << 560 << 200;
    rightSplitter->setSizes(rightSizes);

    // Connect preview signals
    connect(m_preview, SIGNAL(frameChanged(int, int)), this, SLOT(onPreviewFrameChanged(int, int)));
    connect(m_preview, SIGNAL(playbackStateChanged(bool)), this, SLOT(onPreviewPlaybackChanged(bool)));
    connect(m_preview, SIGNAL(framesLoaded(int, int, int)), this, SLOT(onPreviewFramesLoaded(int, int, int)));

    onLogMessage("Welcome to XBootsplash Studio.");
    onLogMessage("Detected project root: " + m_projectRoot);
    TQTimer::singleShot(50, this, SLOT(checkStartupEnvironment()));
}

void MainWindow::setupMenus() {
    // File Menu
    m_fileMenu = new TQPopupMenu(this);
    m_fileMenu->insertItem(iconNew(), "&New Project", this, SLOT(onNewProject()), TQt::CTRL + TQt::Key_N);
    m_fileMenu->insertItem(iconOpen(), "&Open Project...", this, SLOT(onOpenProject()), TQt::CTRL + TQt::Key_O);
    m_fileMenu->insertItem(iconSave(), "&Save Project", this, SLOT(onSaveProject()), TQt::CTRL + TQt::Key_S);
    m_fileMenu->insertItem(iconSaveAs(), "Save Project &As...", this, SLOT(onSaveProjectAs()), TQt::CTRL + TQt::SHIFT + TQt::Key_S);
    m_fileMenu->insertSeparator();
    m_actionOpenFramesDir = m_fileMenu->insertItem(iconFrames(), "Open Frames &Directory...", this, SLOT(onOpenFramesDir()));
    m_actionOpenSingleImage = m_fileMenu->insertItem(iconImage(), "Open &Static Image...", this, SLOT(onOpenSingleImage()));
    m_fileMenu->insertSeparator();
    m_fileMenu->insertItem(iconPackageImport(), "&Import .xbs Package...", this, SLOT(onImportPackage()));
    m_fileMenu->insertItem(iconPackageExport(), "&Export .xbs Package...", this, SLOT(onExportPackage()));
    m_fileMenu->insertSeparator();
    m_fileMenu->insertItem(iconQuit(), "&Quit", this, SLOT(close()), TQt::CTRL + TQt::Key_Q);
    menuBar()->insertItem("&File", m_fileMenu);

    connect(m_fileMenu, SIGNAL(aboutToShow()), this, SLOT(updateFileMenuState()));
    if (m_configPanel) {
        connect(m_configPanel, SIGNAL(configurationChanged()), this, SLOT(onConfigurationChanged()));
    }
    updateFileMenuState();

    // View Menu
    m_viewMenu = new TQPopupMenu(this);
    m_actionPlayPause = m_viewMenu->insertItem(iconPlay(), "Play / Pause", this, SLOT(onTogglePlayback()), TQt::Key_Space);
    m_viewMenu->insertItem(iconStop(), "Stop / Reset", this, SLOT(onStopPlayback()), TQt::Key_Escape);
    m_viewMenu->insertItem(iconPrev(), "First Frame", this, SLOT(onFirstFrame()), TQt::Key_Home);
    m_viewMenu->insertItem(iconRewind(), "Previous Frame (Rewind)", this, SLOT(onRewindFrame()), TQt::Key_Left);
    m_viewMenu->insertItem(iconFastForward(), "Next Frame (Fast Forward)", this, SLOT(onFastForwardFrame()), TQt::Key_Right);
    m_viewMenu->insertItem(iconNext(), "Last Frame", this, SLOT(onLastFrame()), TQt::Key_End);
    m_viewMenu->insertSeparator();
    m_viewMenu->insertItem(iconFullscreen(), "&Fullscreen Preview", this, SLOT(onLaunchFullscreen()), TQt::Key_F11);
    m_viewMenu->insertItem(iconGuides(), "Toggle Crosshair && Guides", this, SLOT(onToggleCrosshair()));
    menuBar()->insertItem("&View", m_viewMenu);

    // Tools Menu
    m_toolsMenu = new TQPopupMenu(this);
    m_toolsMenu->insertItem(iconBuild(), "&Build Bootsplash Binary", this, SLOT(onStartBuild()), TQt::Key_F5);
    m_actionTestLive = m_toolsMenu->insertItem(iconLivePlay(), "&Test Splash on Hardware (VT)...", this, SLOT(onTestBinaryLive()), TQt::Key_F6);
    m_toolsMenu->insertSeparator();
    m_toolsMenu->insertItem(iconDeb(), "Build &Debian Package (.deb)...", this, SLOT(onBuildDebianPackage()));
    m_toolsMenu->insertItem(iconInstall(), "&Install to System...", this, SLOT(onInstallSplash()));
    m_toolsMenu->insertItem(iconUninstall(), "&Uninstall Current Bootsplash", this, SLOT(onUninstallSplash()));
    m_toolsMenu->insertSeparator();
    m_toolsMenu->insertItem(iconGrub(), "Silent &Boot Configuration (GRUB)...", this, SLOT(onSilentBootConfig()));
    menuBar()->insertItem("&Tools", m_toolsMenu);

    // Help Menu
    m_helpMenu = new TQPopupMenu(this);
    m_helpMenu->insertItem(iconQuickHelp(), "&Quick Start && User Guide...", this, SLOT(onShowUserGuide()), TQt::Key_F1);
    m_helpMenu->insertSeparator();
    m_helpMenu->insertItem(iconAbout(), "&About XBootsplash Studio...", this, SLOT(onAbout()));
    menuBar()->insertItem("&Help", m_helpMenu);
}

void MainWindow::setupToolBars() {
    TQToolBar *tb = new TQToolBar(this, "Main Toolbar");
    tb->setLabel("Playback controls & tools");

    // Order: prev rewind play stop fastforward next
    m_btnPrev = new TQToolButton(tb);
    m_btnPrev->setIconSet(TQIconSet(iconPrev()));
    m_btnPrev->setTextLabel("First Frame (Home)", true);
    TQToolTip::add(m_btnPrev, "Jump to First Frame (Home)");
    connect(m_btnPrev, SIGNAL(clicked()), this, SLOT(onFirstFrame()));

    m_btnRewind = new TQToolButton(tb);
    m_btnRewind->setIconSet(TQIconSet(iconRewind()));
    m_btnRewind->setTextLabel("Previous Frame (Left)", true);
    TQToolTip::add(m_btnRewind, "Rewind / Previous Frame (Left)");
    connect(m_btnRewind, SIGNAL(clicked()), this, SLOT(onRewindFrame()));

    m_btnPlay = new TQToolButton(tb);
    m_btnPlay->setIconSet(TQIconSet(iconPlay()));
    m_btnPlay->setTextLabel("Play (Space)", true);
    TQToolTip::add(m_btnPlay, "Play / Pause (Space)");
    connect(m_btnPlay, SIGNAL(clicked()), this, SLOT(onTogglePlayback()));

    m_btnStop = new TQToolButton(tb);
    m_btnStop->setIconSet(TQIconSet(iconStop()));
    m_btnStop->setTextLabel("Stop / Reset (Esc)", true);
    TQToolTip::add(m_btnStop, "Stop Playback (Esc)");
    connect(m_btnStop, SIGNAL(clicked()), this, SLOT(onStopPlayback()));

    m_btnFastForward = new TQToolButton(tb);
    m_btnFastForward->setIconSet(TQIconSet(iconFastForward()));
    m_btnFastForward->setTextLabel("Next Frame (Right)", true);
    TQToolTip::add(m_btnFastForward, "Fast Forward / Next Frame (Right)");
    connect(m_btnFastForward, SIGNAL(clicked()), this, SLOT(onFastForwardFrame()));

    m_btnNext = new TQToolButton(tb);
    m_btnNext->setIconSet(TQIconSet(iconNext()));
    m_btnNext->setTextLabel("Last Frame (End)", true);
    TQToolTip::add(m_btnNext, "Jump to Last Frame (End)");
    connect(m_btnNext, SIGNAL(clicked()), this, SLOT(onLastFrame()));

    // Horizontal margin between playback controls and slider
    TQWidget *spacerSliderLeft = new TQWidget(tb);
    spacerSliderLeft->setFixedWidth(14);

    // Scrub slider
    m_scrubSlider = new TQSlider(0, 0, 1, 0, TQt::Horizontal, tb);
    m_scrubSlider->setFixedWidth(200);
    connect(m_scrubSlider, SIGNAL(valueChanged(int)), this, SLOT(onScrubFrame(int)));

    TQWidget *spacerSliderRight = new TQWidget(tb);
    spacerSliderRight->setFixedWidth(8);

    m_frameCounterLabel = new TQLabel(" 0 / 0 ", tb);
    m_frameCounterLabel->setFixedWidth(120);

    tb->addSeparator();

    m_btnFullscreen = new TQToolButton(tb);
    m_btnFullscreen->setIconSet(TQIconSet(iconFullscreen()));
    m_btnFullscreen->setTextLabel("Fullscreen (F11)", true);
    TQToolTip::add(m_btnFullscreen, "Fullscreen Preview (F11)");
    connect(m_btnFullscreen, SIGNAL(clicked()), this, SLOT(onLaunchFullscreen()));

    m_btnCrosshair = new TQToolButton(tb);
    m_btnCrosshair->setIconSet(TQIconSet(iconGuides()));
    m_btnCrosshair->setTextLabel("Toggle Guides", true);
    TQToolTip::add(m_btnCrosshair, "Toggle Guides & Crosshair");
    connect(m_btnCrosshair, SIGNAL(clicked()), this, SLOT(onToggleCrosshair()));

    tb->addSeparator();

    m_btnBuild = new TQToolButton(tb);
    m_btnBuild->setIconSet(TQIconSet(iconBuild()));
    m_btnBuild->setUsesTextLabel(true);
    m_btnBuild->setTextPosition(TQToolButton::BesideIcon);
    m_btnBuild->setTextLabel("Build (F5)", true);
    TQToolTip::add(m_btnBuild, "Compile Standalone Bootsplash Binary (F5)");
    connect(m_btnBuild, SIGNAL(clicked()), this, SLOT(onStartBuild()));

    m_btnTestLive = new TQToolButton(tb);
    m_btnTestLive->setIconSet(TQIconSet(iconLivePlay()));
    m_btnTestLive->setUsesTextLabel(true);
    m_btnTestLive->setTextPosition(TQToolButton::BesideIcon);
    m_btnTestLive->setTextLabel("Test Live", true);
    TQToolTip::add(m_btnTestLive, "Build bootsplash binary first to test on Hardware VT (F6 disabled)");
    m_btnTestLive->setEnabled(false);
    connect(m_btnTestLive, SIGNAL(clicked()), this, SLOT(onTestBinaryLive()));

    m_btnPackage = new TQToolButton(tb);
    m_btnPackage->setIconSet(TQIconSet(iconPackage()));
    m_btnPackage->setUsesTextLabel(true);
    m_btnPackage->setTextPosition(TQToolButton::BesideIcon);
    m_btnPackage->setTextLabel("Package...", true);
    TQToolTip::add(m_btnPackage, "Export .xbs Distribution Package");
    connect(m_btnPackage, SIGNAL(clicked()), this, SLOT(onExportPackage()));

    m_btnInstall = new TQToolButton(tb);
    m_btnInstall->setIconSet(TQIconSet(iconInstall()));
    m_btnInstall->setUsesTextLabel(true);
    m_btnInstall->setTextPosition(TQToolButton::BesideIcon);
    m_btnInstall->setTextLabel("Install...", true);
    TQToolTip::add(m_btnInstall, "Install Bootsplash to System Initramfs");
    connect(m_btnInstall, SIGNAL(clicked()), this, SLOT(onInstallSplash()));
}

void MainWindow::updateStatusBar() {
    if (m_busyTimer && m_busyTimer->isActive()) {
        return;
    }

    int configuredGrub = 0;
    int totalRequired = 6;
    bool kernelQuiet = false;
    bool hasGrub = m_installerEngine->checkSilentBootConfig(configuredGrub, totalRequired, kernelQuiet);

    TQString installedBin;
    bool isInstalled = m_installerEngine->isSplashInstalled(installedBin);

    TQString grubStatus;
    if (hasGrub) {
        if (configuredGrub >= totalRequired) {
            grubStatus = "Silent boot [OK]";
        } else {
            grubStatus = TQString("Silent boot [%1/%2]").arg(configuredGrub).arg(totalRequired);
        }
    } else {
        grubStatus = kernelQuiet ? "quiet [OK]" : "quiet [MISSING]";
    }

    TQString installStatus = isInstalled ? ("Installed: " + installedBin) : "Not installed";
    TQString targetBin = m_lastBuiltBinary.isEmpty() ? "None (Not built)" : TQFileInfo(m_lastBuiltBinary).fileName();

    statusBar()->message(TQString("Target: %1 | System: %2 | Grub: %3").arg(targetBin).arg(installStatus).arg(grubStatus));
}

static const char *s_busySpinnerFrames[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};
static const int s_numBusySpinnerFrames = 10;

void MainWindow::startBusyStatus(const TQString &baseText) {
    m_busyBaseText = baseText;
    m_busyFrame = 0;
    m_busyStartTime = TQTime::currentTime();
    if (!m_busyTimer) {
        m_busyTimer = new TQTimer(this);
        connect(m_busyTimer, SIGNAL(timeout()), this, SLOT(onBusyTimerTick()));
    }
    TQString spinner = TQString::fromUtf8(s_busySpinnerFrames[m_busyFrame]);
    statusBar()->message(TQString("%1  %2").arg(m_busyBaseText).arg(spinner));
    if (!m_busyTimer->isActive()) {
        m_busyTimer->start(80);
    }
}

void MainWindow::updateBusyStatus(const TQString &newBaseText) {
    m_busyBaseText = newBaseText;
    TQString spinner = TQString::fromUtf8(s_busySpinnerFrames[m_busyFrame]);
    int elapsedSec = m_busyStartTime.msecsTo(TQTime::currentTime()) / 1000;
    if (elapsedSec >= 2) {
        statusBar()->message(TQString("%1  %2  (%3s)").arg(m_busyBaseText).arg(spinner).arg(elapsedSec));
    } else {
        statusBar()->message(TQString("%1  %2").arg(m_busyBaseText).arg(spinner));
    }
}

void MainWindow::stopBusyStatus() {
    if (m_busyTimer && m_busyTimer->isActive()) {
        m_busyTimer->stop();
    }
    updateStatusBar();
}

void MainWindow::onBusyTimerTick() {
    m_busyFrame = (m_busyFrame + 1) % s_numBusySpinnerFrames;
    TQString spinner = TQString::fromUtf8(s_busySpinnerFrames[m_busyFrame]);
    int elapsedSec = m_busyStartTime.msecsTo(TQTime::currentTime()) / 1000;
    if (elapsedSec >= 2) {
        statusBar()->message(TQString("%1  %2  (%3s)").arg(m_busyBaseText).arg(spinner).arg(elapsedSec));
    } else {
        statusBar()->message(TQString("%1  %2").arg(m_busyBaseText).arg(spinner));
    }
}

void MainWindow::onBuildStatusChanged(const TQString &status) {
    updateBusyStatus(status);
}

TQString MainWindow::currentProjectBinaryPath() const {
    if (!m_configPanel) return TQString::null;

    // 1. A project must have valid frames or a static image loaded
    TQString frames = m_configPanel->framesPath().stripWhiteSpace();
    if (frames.isEmpty()) {
        return TQString::null;
    }
    TQString fullFrames = frames;
    if (TQFileInfo(fullFrames).isRelative()) {
        fullFrames = m_projectRoot + "/" + fullFrames;
    }
    if (!TQFile::exists(frames) && !TQFile::exists(fullFrames)) {
        return TQString::null;
    }

    // 2. If m_lastBuiltBinary is set, exists on disk and is executable, prefer it
    if (!m_lastBuiltBinary.isEmpty() && TQFile::exists(m_lastBuiltBinary)) {
        TQFileInfo fi(m_lastBuiltBinary);
        if (fi.isFile() && fi.isExecutable()) {
            return m_lastBuiltBinary;
        }
    }

    // 3. Fallback: Check if the target executable matching binaryName exists on disk in projectRoot
    TQString bName = m_configPanel->binaryName().stripWhiteSpace();
    if (!bName.isEmpty()) {
        TQStringList candidates;
        candidates << (m_projectRoot + "/" + bName);
        if (!bName.startsWith("xbs_")) {
            candidates << (m_projectRoot + "/xbs_" + bName);
        }
        for (TQStringList::ConstIterator it = candidates.begin(); it != candidates.end(); ++it) {
            if (TQFile::exists(*it)) {
                TQFileInfo fi(*it);
                if (fi.isFile() && fi.isExecutable()) {
                    return *it;
                }
            }
        }
    }

    return TQString::null;
}

void MainWindow::updateProjectBinaryState() {
    TQString bin = currentProjectBinaryPath();
    bool exists = !bin.isEmpty() && TQFile::exists(bin);

    if (exists && m_lastBuiltBinary.isEmpty()) {
        m_lastBuiltBinary = bin;
    }

    if (m_btnTestLive) {
        m_btnTestLive->setEnabled(exists);
        if (exists) {
            TQToolTip::add(m_btnTestLive, TQString("Test '%1' on Virtual Terminal (Hardware VT) (F6)").arg(TQFileInfo(bin).fileName()));
        } else {
            TQToolTip::add(m_btnTestLive, "Build bootsplash binary first to test on Hardware VT (F6 disabled)");
        }
    }

    if (m_toolsMenu && m_actionTestLive != -1) {
        m_toolsMenu->setItemEnabled(m_actionTestLive, exists);
    }

    updateStatusBar();
}

void MainWindow::loadProject(const TQString &filePath) {
    if (m_configPanel && m_configPanel->loadFromProjectFile(filePath, m_projectRoot)) {
        m_currentProjectFilePath = filePath;
        m_isProjectDirty = false;
        m_isDirty = true;
        m_lastBuiltBinary = TQString::null;
        updateFileMenuState();
        updateProjectBinaryState();
        updateWindowTitle();
        onLogMessage(TQString("Project loaded: %1").arg(filePath));
    }
}

bool MainWindow::saveProject(const TQString &filePath) {
    if (m_configPanel && m_configPanel->saveToProjectFile(filePath, m_projectRoot)) {
        m_currentProjectFilePath = filePath;
        m_isProjectDirty = false;
        updateWindowTitle();
        onLogMessage(TQString("Project saved: %1").arg(filePath));
        return true;
    }
    return false;
}

void MainWindow::updateWindowTitle() {
    TQString projName = m_currentProjectFilePath.isEmpty() ? "Untitled.xbsp" : TQFileInfo(m_currentProjectFilePath).fileName();
    TQString dirtyFlag = m_isProjectDirty ? " *" : "";
    setCaption(TQString("%1 v%2 - [%3%4]").arg(XBOOTSPLASH_GUI_NAME).arg(XBOOTSPLASH_GUI_VERSION).arg(projName).arg(dirtyFlag));
}

bool MainWindow::maybeSavePrompt() {
    if (!m_isProjectDirty) return true;

    TQString projName = m_currentProjectFilePath.isEmpty() ? "Untitled.xbsp" : TQFileInfo(m_currentProjectFilePath).fileName();
    int res = showWarning(this, "Unsaved Changes",
                          TQString("The current project '%1' has unsaved changes.\n\n"
                                  "Do you want to save your changes before proceeding?").arg(projName),
                          "Save Project", "Discard Changes", "Cancel", 0, 2);
    if (res == 0) {
        return onSaveProject();
    } else if (res == 1) {
        return true; // Discard changes
    }
    return false; // Cancel
}

void MainWindow::onNewProject() {
    if (!maybeSavePrompt()) return;

    m_currentProjectFilePath = TQString::null;
    if (m_configPanel) {
        m_configPanel->resetToDefaults();
    }
    m_isProjectDirty = false;
    m_isDirty = true;
    m_lastBuiltBinary = TQString::null;

    updateFileMenuState();
    updateProjectBinaryState();
    updateWindowTitle();
    onLogMessage("New project created (Untitled.xbsp).");
}

void MainWindow::onOpenProject() {
    if (!maybeSavePrompt()) return;

    TQString initialDir = m_projectRoot;
    if (!m_currentProjectFilePath.isEmpty()) {
        initialDir = TQFileInfo(m_currentProjectFilePath).dirPath(true);
    }
    TQString path = TQFileDialog::getOpenFileName(initialDir, "XBootsplash Projects (*.xbsp)", this, "open_project", "Open XBootsplash Project");
    if (path.isEmpty()) return;

    if (m_configPanel && m_configPanel->loadFromProjectFile(path, m_projectRoot)) {
        m_currentProjectFilePath = path;
        m_isProjectDirty = false;
        m_isDirty = true;
        m_lastBuiltBinary = TQString::null;

        updateFileMenuState();
        updateProjectBinaryState();
        updateWindowTitle();
        onLogMessage(TQString("Project loaded successfully: %1").arg(path));
    } else {
        TQMessageBox::critical(this, "Load Project Error",
                              TQString("Unable to load project file:\n%1").arg(path));
    }
}

bool MainWindow::onSaveProject() {
    if (m_currentProjectFilePath.isEmpty()) {
        return onSaveProjectAs();
    }

    if (m_configPanel && m_configPanel->saveToProjectFile(m_currentProjectFilePath, m_projectRoot)) {
        m_isProjectDirty = false;
        updateWindowTitle();
        onLogMessage(TQString("Project saved: %1").arg(m_currentProjectFilePath));
        return true;
    }

    TQMessageBox::critical(this, "Save Project Error",
                          TQString("Failed to save project file:\n%1").arg(m_currentProjectFilePath));
    return false;
}

bool MainWindow::onSaveProjectAs() {
    TQString defaultName;
    if (!m_currentProjectFilePath.isEmpty()) {
        defaultName = m_currentProjectFilePath;
    } else {
        TQString base = m_configPanel ? m_configPanel->binaryName() : "project";
        if (base.isEmpty() || base == "xbootsplash") {
            TQString fp = m_configPanel ? m_configPanel->framesPath() : "";
            if (!fp.isEmpty()) {
                base = "xbs_" + TQFileInfo(fp).baseName(true);
            }
        }
        defaultName = m_projectRoot + "/" + base + ".xbsp";
    }

    TQString path = TQFileDialog::getSaveFileName(defaultName, "XBootsplash Projects (*.xbsp)", this, "save_project", "Save Project As");
    if (path.isEmpty()) return false;

    if (!path.endsWith(".xbsp")) {
        path += ".xbsp";
    }

    if (m_configPanel && m_configPanel->saveToProjectFile(path, m_projectRoot)) {
        m_currentProjectFilePath = path;
        m_isProjectDirty = false;
        updateWindowTitle();
        onLogMessage(TQString("Project saved as: %1").arg(path));
        return true;
    }

    TQMessageBox::critical(this, "Save Project Error",
                          TQString("Failed to save project file:\n%1").arg(path));
    return false;
}

void MainWindow::updateFileMenuState() {
    if (!m_fileMenu || !m_configPanel) return;
    int mode = m_configPanel->displayMode();
    bool hasFramesDir = (mode < 3); // Modes 0, 1, 2 have animation frames
    bool hasStaticImage = (mode >= 1); // Modes 1, 2 have background image; Modes 3, 4 have static image
    if (m_actionOpenFramesDir != -1) {
        m_fileMenu->setItemEnabled(m_actionOpenFramesDir, hasFramesDir);
    }
    if (m_actionOpenSingleImage != -1) {
        m_fileMenu->setItemEnabled(m_actionOpenSingleImage, hasStaticImage);
    }
}

void MainWindow::onConfigurationChanged() {
    m_isDirty = true;
    m_isProjectDirty = true;
    m_lastBuiltBinary = TQString::null;
    updateFileMenuState();
    updateProjectBinaryState();
    updateWindowTitle();
}

void MainWindow::onOpenFramesDir() {
    if (!m_configPanel || m_configPanel->displayMode() >= 3) return;
    TQString dir = TQFileDialog::getExistingDirectory(m_projectRoot, this, "select_dir", "Select PNG Frames Directory", true);
    if (!dir.isEmpty()) {
        while (dir.endsWith("/") && dir.length() > 1) {
            dir.truncate(dir.length() - 1);
        }
        m_configPanel->setFramesPath(dir, true);
        TQFileInfo fi(dir);
        TQString bName = fi.baseName(true);
        if (bName.isEmpty()) bName = fi.fileName();
        m_configPanel->setBinaryName("xbs_" + bName);
    }
}

void MainWindow::onOpenSingleImage() {
    if (!m_configPanel) return;
    int mode = m_configPanel->displayMode();
    if (mode < 1) return; // Mode 0 has no static or background image

    TQString title = (mode >= 3) ? "Select Static Image" : "Select Background Image";
    TQString file = TQFileDialog::getOpenFileName(m_projectRoot, "Images (*.png *.PNG *.jpg *.jpeg)", this, "open_static_image", title);
    if (!file.isEmpty()) {
        if (mode >= 3) {
            m_configPanel->setFramesPath(file);
            TQFileInfo fi(file);
            m_configPanel->setBinaryName("xbs_" + fi.baseName(true));
        } else {
            // Modes 1 and 2: Background image
            m_configPanel->setBgImagePath(file);
        }
    }
}

void MainWindow::onImportPackage() {
    TQString pkg = TQFileDialog::getOpenFileName(m_projectRoot + "/packages", "XBootsplash Packages (*.xbs)", this);
    if (pkg.isEmpty()) return;

    PackageInspectorDialog dlg(pkg, m_packageMgr, m_installerEngine, m_projectRoot, this);
    connect(&dlg, SIGNAL(statusMessage(const TQString&)), this, SLOT(onLogMessage(const TQString&)));
    dlg.exec();
    updateStatusBar();
}

void MainWindow::onExportPackage() {
    TQString bin = currentProjectBinaryPath();
    if (bin.isEmpty() || !TQFile::exists(bin)) {
        showWarning(this, "Binary Not Built",
                    "The bootsplash binary for the current project has not been built yet.\n\n"
                    "Please click 'Build (F5)' first to generate the binary before packaging.");
        return;
    }

    SplashPackageManager::PackageMetadata meta;
    meta.splashName = m_configPanel->binaryName();
    if (meta.splashName.startsWith("xbs_")) meta.splashName = meta.splashName.mid(4);
    meta.author = m_configPanel->author();
    meta.notes = m_configPanel->notes();
    meta.backend = m_configPanel->useDrm() ? "drm" : "fbdev";
    meta.resolution = m_configPanel->targetResolution();
    meta.displayMode = m_configPanel->displayMode();
    char hex[8];
    snprintf(hex, sizeof(hex), "%02X%02X%02X",
             m_preview->bgColor().red(), m_preview->bgColor().green(), m_preview->bgColor().blue());
    meta.bgColor = hex;
    TQSize objSz = m_preview->objectSize();
    meta.frameW = objSz.width();
    meta.frameH = objSz.height();
    meta.nframes = m_preview->frameCount();
    meta.frameDelay = m_preview->frameDelay();
    meta.offsetX = m_preview->offsetX();
    meta.offsetY = m_preview->offsetY();
    meta.bgOffsetX = m_preview->bgOffsetX();
    meta.bgOffsetY = m_preview->bgOffsetY();

    int superComp = m_configPanel->superCompression();
    TQString compMethod = m_configPanel->compressionMethod();
    if (meta.displayMode >= 2) {
        if (superComp == 2) meta.compression = "UPKR (Palette 8-bit)";
        else if (superComp == 1) meta.compression = "ZX0 (Palette 8-bit)";
        else meta.compression = "LZSS (Palette 8-bit)";
    } else {
        TQString base = "RLE (Auto)";
        if (compMethod == "rle_xor") base = "RLE XOR";
        else if (compMethod == "rle_direct") base = "RLE Direct";
        else if (compMethod == "sparse") base = "Sparse XOR";
        else if (compMethod == "raw") base = "Raw XOR";

        if (superComp == 2) meta.compression = TQString("UPKR Super-pack (%1)").arg(base);
        else if (superComp == 1) meta.compression = TQString("ZX0 Super-pack (%1)").arg(base);
        else meta.compression = TQString("%1 (Delta)").arg(base);
    }

    TQString defaultOutDir = m_projectRoot + "/packages";
    if (access(defaultOutDir.latin1(), W_OK) != 0) {
        defaultOutDir = TQDir::homeDirPath() + "/xbootsplash_packages";
    }
    TQDir outD(defaultOutDir);
    if (!outD.exists()) outD.mkdir(defaultOutDir);

    TQString suggestedFile = defaultOutDir + "/" + TQString("%1_%2_%3.xbs")
                             .arg(meta.splashName)
                             .arg(meta.backend)
                             .arg(meta.resolution);

    TQString savePkg = TQFileDialog::getSaveFileName(suggestedFile,
                                                    "XBootsplash Package (*.xbs)",
                                                    this, "save_pkg_dlg",
                                                    "Save .xbs Package");
    if (savePkg.isEmpty()) return;

    onLogMessage(SplashLog::banner("PACKAGE"));
    onLogMessage(TQString("Target binary   : %1").arg(bin));
    onLogMessage(TQString("Export package  : %1").arg(TQFileInfo(savePkg).fileName()));

    // Ensure preview visual exists (generate from live preview widget if not present)
    TQString expectedPreview = (meta.displayMode >= 2) ? (bin + "_preview.png") : (bin + "_preview.gif");
    if ((!TQFile::exists(expectedPreview) || TQFileInfo(expectedPreview).size() == 0) && m_preview) {
        startBusyStatus(TQString("Generating %1 visual preview...").arg(meta.displayMode >= 2 ? "PNG" : "GIF"));
        tqApp->processEvents();
        onLogMessage(TQString("Generating %1 visual preview...").arg(meta.displayMode >= 2 ? "PNG" : "GIF"));
        m_preview->exportPreviewVisual(expectedPreview);
    }

    startBusyStatus("Compressing and creating .xbs package...");
    tqApp->processEvents();

    TQString outPkg, err;
    bool exportOk = m_packageMgr->exportPackage(bin, meta, savePkg, outPkg, err);
    stopBusyStatus();

    if (exportOk) {
        onLogMessage(TQString("✔ Package created: %1").arg(outPkg));
        onLogMessage(SplashLog::doneBanner());
        TQMessageBox::information(this, "Package Exported",
                                  TQString("Package created successfully:\n%1").arg(outPkg));
    } else {
        onLogMessage(TQString("[ERROR] Package export failed: %1").arg(err));
        onLogMessage(SplashLog::failedBanner());
        TQMessageBox::critical(this, "Export Error", err);
    }
}

void MainWindow::onBuildDebianPackage() {
    TQString bin = currentProjectBinaryPath();
    if (bin.isEmpty() || !TQFile::exists(bin)) {
        showWarning(this, "Binary Not Built",
                    "The bootsplash binary for the current project has not been built yet.\n\n"
                    "Please click 'Build (F5)' first to generate the binary before packaging.");
        return;
    }

    SplashPackageManager::PackageMetadata meta;
    meta.splashName = m_configPanel->binaryName();
    if (meta.splashName.startsWith("xbs_")) meta.splashName = meta.splashName.mid(4);
    meta.author = m_configPanel->author();
    meta.notes = m_configPanel->notes();
    meta.backend = m_configPanel->useDrm() ? "drm" : "fbdev";
    meta.resolution = m_configPanel->targetResolution();
    meta.displayMode = m_configPanel->displayMode();

    TQString defaultOutDir = m_projectRoot + "/packages";
    if (access(defaultOutDir.latin1(), W_OK) != 0) {
        defaultOutDir = TQDir::homeDirPath() + "/xbootsplash_packages";
    }
    TQDir outD(defaultOutDir);
    if (!outD.exists()) outD.mkdir(defaultOutDir);

    TQString arch = "amd64";
    FILE *fp = popen("dpkg --print-architecture 2>/dev/null", "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            TQString a = TQString::fromLocal8Bit(buf).stripWhiteSpace();
            if (!a.isEmpty()) arch = a;
        }
        pclose(fp);
    }

    TQString suggestedFile = defaultOutDir + "/" + TQString("xbootsplash-theme-%1_1.0_%2.deb")
                             .arg(meta.splashName.lower())
                             .arg(arch);

    TQString saveDeb = TQFileDialog::getSaveFileName(suggestedFile,
                                                    "Debian Package (*.deb)",
                                                    this, "save_deb_dlg",
                                                    "Save Debian Package (.deb)");
    if (saveDeb.isEmpty()) return;

    onLogMessage(SplashLog::banner("DEB EXPORT"));
    onLogMessage(TQString("Target binary   : %1").arg(bin));
    onLogMessage(TQString("Output package  : %1").arg(TQFileInfo(saveDeb).fileName()));

    startBusyStatus("Building Debian package (.deb)...");
    tqApp->processEvents();

    TQString outDeb, err;
    bool ok = m_packageMgr->exportDebianPackage(bin, meta, saveDeb, (meta.displayMode != 1), outDeb, err);
    stopBusyStatus();

    if (ok) {
        TQFileInfo fiDeb(outDeb);
        unsigned long szKb = (fiDeb.size() + 1023) / 1024;
        onLogMessage(TQString("✔ Debian package created: %1 (%2 KB)").arg(outDeb).arg(szKb));
        onLogMessage(SplashLog::doneBanner());
        updateStatusBar();

        TQMessageBox::information(
            this, "Debian Package Created",
            TQString("<p><b>Debian Package successfully created!</b></p>"
                    "<p><b>File:</b> %1<br><b>Size:</b> %2 KB (%3 bytes)</p>"
                    "<p>To install this package on any Debian / Q4OS / Ubuntu system, run:<br>"
                    "<tt><b>sudo apt install \"%4\"</b></tt></p>"
                    "<p>To remove it later:<br>"
                    "<tt><b>sudo apt remove xbootsplash-theme-%5</b></tt></p>")
            .arg(fiDeb.fileName())
            .arg(szKb)
            .arg(fiDeb.size())
            .arg(outDeb)
            .arg(meta.splashName.lower()));
    } else {
        onLogMessage(TQString("[ERROR] Debian package build failed: %1").arg(err));
        onLogMessage(SplashLog::failedBanner());
        updateStatusBar();
        TQMessageBox::critical(this, "Packaging Error",
                              TQString("Failed to build Debian package:\n\n%1").arg(err));
    }
}

void MainWindow::onTogglePlayback() {
    m_preview->togglePlay();
}

void MainWindow::onStopPlayback() {
    m_preview->stop();
}

void MainWindow::onFirstFrame() {
    m_preview->firstFrame();
}

void MainWindow::onRewindFrame() {
    m_preview->prevFrame();
}

void MainWindow::onFastForwardFrame() {
    m_preview->nextFrame();
}

void MainWindow::onLastFrame() {
    m_preview->lastFrame();
}

void MainWindow::onPrevFrame() {
    onRewindFrame();
}

void MainWindow::onNextFrame() {
    onFastForwardFrame();
}

void MainWindow::onScrubFrame(int frame) {
    m_preview->seekFrame(frame);
}

void MainWindow::onLaunchFullscreen() {
    m_preview->pause();
    m_fullscreen->setupAnimation(
        m_preview->displayFrames(),
        m_preview->frameDelay(),
        m_preview->offsetX(),
        m_preview->offsetY(),
        m_preview->bgOffsetX(),
        m_preview->bgOffsetY(),
        m_preview->displayMode(),
        m_preview->bgColor(),
        m_preview->bgPixmap(),
        m_preview->loopMode(),
        m_preview->loopStart()
    );
    m_fullscreen->startPreview();
}

void MainWindow::onToggleCrosshair() {
    m_preview->setShowCrosshair(!m_preview->showCrosshair());
}

void MainWindow::onStartBuild() {
    if (m_buildEngine->isBuilding()) {
        showWarning(this, "Build in Progress", "A build is already in progress.");
        return;
    }

    TQString gccPath;
    if (!m_installerEngine->checkGccInstalled(gccPath)) {
        showWarning(this, "GCC Not Found",
                    "The GCC compiler is not installed on this system.\n\n"
                    "You cannot build standalone bootsplash binaries without GCC.\n"
                    "You can only install pre-compiled .xbs packages.");
        return;
    }

    SplashBuildEngine::BuildParams p;
    p.displayMode = m_configPanel->displayMode();
    p.framesPath = m_configPanel->framesPath();
    p.bgImagePath = m_configPanel->bgImagePath();
    p.offsetX = m_configPanel->offsetX();
    p.offsetY = m_configPanel->offsetY();
    p.bgOffsetX = m_configPanel->bgOffsetX();
    p.bgOffsetY = m_configPanel->bgOffsetY();
    p.frameDelay = m_configPanel->frameDelay();
    p.loopMode = m_configPanel->loopMode();
    p.loopStart = m_configPanel->loopStart();
    p.minBootLoops = m_configPanel->minBootLoops();
    p.invertFrames = m_configPanel->invertFrames();
    p.bgColor = m_configPanel->bgColor();
    p.targetResolution = m_configPanel->targetResolution();
    p.binaryName = m_configPanel->binaryName();
    p.useDrm = m_configPanel->useDrm();
    p.compressionMethod = m_configPanel->compressionMethod();
    p.useZx0 = m_configPanel->useZx0();
    p.superCompression = m_configPanel->superCompression();
    p.projectRoot = m_projectRoot;

    if ((p.displayMode == 1 || p.displayMode == 2) &&
        (p.bgImagePath.isEmpty() || !TQFile::exists(p.bgImagePath))) {
        showWarning(this, "Missing Background Image",
                    "Display Mode 1 and 2 require a valid background image.\n"
                    "Please select a background image in section 1 (Source & Display Mode).");
        return;
    }

    m_buildEngine->startBuild(p);
}

void MainWindow::onBuildStarted() {
    m_btnBuild->setEnabled(false);
    if (m_btnTestLive) m_btnTestLive->setEnabled(false);
    if (m_toolsMenu && m_actionTestLive != -1) m_toolsMenu->setItemEnabled(m_actionTestLive, false);
    startBusyStatus("Compiling splash generator...");
}

void MainWindow::onBuildFinished(bool success, const TQString &binaryPath, unsigned long sizeBytes, bool showDialog) {
    stopBusyStatus();
    m_btnBuild->setEnabled(true);
    if (success) {
        m_isDirty = false;
        m_lastBuiltBinary = binaryPath;
        updateProjectBinaryState();
        if (showDialog && !m_skipBuildDialog) {
            TQMessageBox::information(this, "Build Succeeded",
                                      TQString("Binary %1 generated successfully!\nSize: %2 KB\n\nYou can now test or install it.")
                                      .arg(binaryPath).arg(sizeBytes / 1024));
            // Ensure state remains intact after modal dialog dismissal
            m_isDirty = false;
            m_lastBuiltBinary = binaryPath;
            updateProjectBinaryState();
        }
    } else {
        m_isDirty = true;
        m_lastBuiltBinary = TQString::null;
        updateProjectBinaryState();
        if (showDialog && !m_skipBuildDialog) {
            TQMessageBox::critical(this, "Build Error",
                                   "The build process failed. Please check the log console below.");
        }
    }
}

void MainWindow::onTestBinaryLive() {
    TQString bin = currentProjectBinaryPath();
    if (bin.isEmpty() || !TQFile::exists(bin)) {
        showWarning(this, "Binary Not Built",
                    "The bootsplash binary for the current project has not been built yet.\n\n"
                    "Please click 'Build (F5)' first to generate the binary before testing on hardware.");
        return;
    }

    launchVtTest(bin, TQFileInfo(bin).fileName());
}

bool MainWindow::launchVtTest(const TQString &binaryPath, const TQString &displayName) {
    TQString disp = displayName.isEmpty() ? TQFileInfo(binaryPath).fileName() : displayName;
    onLogMessage(SplashLog::banner("LIVE TEST"));
    onLogMessage(TQString("Target binary : %1").arg(disp));
    onLogMessage("Executing direct hardware test on virtual terminal...");

    if (m_btnTestLive) m_btnTestLive->setEnabled(false);
    if (m_toolsMenu && m_actionTestLive != -1) m_toolsMenu->setItemEnabled(m_actionTestLive, false);

    // Block spurious configuration change signals while testing and dialogs are active
    if (m_configPanel) m_configPanel->blockSignals(true);

    TQString logOutput;
    bool ok = executeVtLiveTest(this, m_projectRoot, binaryPath, disp, logOutput);

    // Restore signals and process any pending X11 events queued during the VT switch
    if (m_configPanel) m_configPanel->blockSignals(false);
    if (tqApp) {
        tqApp->sendPostedEvents();
        tqApp->processEvents();
    }

    // Re-assert binary state: a live test is non-destructive and does not dirty the project
    m_lastBuiltBinary = binaryPath;
    m_isDirty = false;
    updateProjectBinaryState();

    if (ok) {
        onLogMessage("Hardware test finished cleanly. X11 session restored.");
        onLogMessage(SplashLog::doneBanner());
        statusBar()->message("Live VT hardware test completed successfully. X11 session restored.", 6000);
    } else {
        if (!logOutput.stripWhiteSpace().isEmpty()) {
            onLogMessage(TQString("[ERROR] Live test notice: %1").arg(logOutput.stripWhiteSpace()));
        } else {
            onLogMessage("[ERROR] Live test ended or failed.");
        }
        onLogMessage(SplashLog::failedBanner());
        statusBar()->message("Live VT hardware test ended.", 4000);
    }
    return ok;
}

void MainWindow::onInstallSplash() {
    if (m_installerEngine->isRunning()) {
        TQMessageBox::information(this, "Installation in Progress",
                                 "An initramfs operation is already in progress.");
        return;
    }

    TQString bin = currentProjectBinaryPath();
    if (bin.isEmpty() || !TQFile::exists(bin)) {
        showWarning(this, "Binary Not Built",
                    "The bootsplash binary for the current project has not been built yet.\n\n"
                    "Please click 'Build (F5)' first to generate the binary before installing.");
        return;
    }

    int choice = TQMessageBox::information(this, "Install Bootsplash",
                                          TQString("Binary to install: %1\n\nChoose install target:")
                                          .arg(bin),
                                          "Boot & Shutdown (Recommended)",
                                          "Boot Only",
                                          "Shutdown Only", 0, 3);

    SplashInstallerEngine::InstallTarget target = SplashInstallerEngine::TargetBoth;
    if (choice == 1) target = SplashInstallerEngine::TargetBoot;
    else if (choice == 2) target = SplashInstallerEngine::TargetShutdown;
    else if (choice == 3) return;

    TQString password = "";
    if (getuid() != 0) {
        if (!SplashPasswordDialog::getPassword(this, password)) {
            onLogMessage(SplashLog::banner("INSTALL"));
            onLogMessage("Installation cancelled: Administrator password not provided.");
            onLogMessage(SplashLog::failedBanner());
            return;
        }
    }

    m_installerEngine->startInstall(bin, target, m_configPanel->useDrm(), password);
}

void MainWindow::onUninstallSplash() {
    if (m_installerEngine->isRunning()) {
        TQMessageBox::information(this, "Operation in Progress",
                                 "An initramfs operation is already in progress.");
        return;
    }

    int res = showWarning(this, "Uninstall Bootsplash",
                          "Are you sure you want to uninstall the current bootsplash and restore standard configuration?",
                          "Yes, Uninstall", "Cancel", TQString::null, 0, 1);
    if (res != 0) return;

    TQString password = "";
    if (getuid() != 0) {
        if (!SplashPasswordDialog::getPassword(this, password)) {
            onLogMessage(SplashLog::banner("UNINSTALL"));
            onLogMessage("Uninstall cancelled: Administrator password not provided.");
            onLogMessage(SplashLog::failedBanner());
            return;
        }
    }

    m_installerEngine->startUninstall(password);
}

void MainWindow::onInstallStarted() {
    m_btnInstall->setEnabled(false);
    m_btnBuild->setEnabled(false);
    startBusyStatus("Installing bootsplash (updating initramfs)...");
}

void MainWindow::onInstallFinished(bool success, const TQString &message) {
    stopBusyStatus();
    m_btnInstall->setEnabled(true);
    m_btnBuild->setEnabled(true);

    if (success) {
        TQMessageBox::information(this, "Installation Complete",
                                 message + "\n\nThe bootsplash will activate on the next system boot / shutdown.");
    } else {
        TQMessageBox::critical(this, "Installation Failed",
                              message + "\n\nPlease check the log console below.");
    }
}

void MainWindow::onUninstallStarted() {
    m_btnInstall->setEnabled(false);
    m_btnBuild->setEnabled(false);
    startBusyStatus("Uninstalling bootsplash (updating initramfs)...");
}

void MainWindow::onUninstallFinished(bool success, const TQString &message) {
    stopBusyStatus();
    m_btnInstall->setEnabled(true);
    m_btnBuild->setEnabled(true);

    if (success) {
        TQMessageBox::information(this, "Uninstall Complete", message);
    } else {
        TQMessageBox::critical(this, "Uninstall Error",
                              message + "\n\nPlease check the log console below.");
    }
}

void MainWindow::onSilentBootConfig() {
    SilentBootDialog dlg(this);
    dlg.exec();
}

void MainWindow::onShowUserGuide() {
    UserGuideDialog dlg(this);
    dlg.exec();
}

void MainWindow::onAbout() {
    AboutDialog dlg(this);
    dlg.exec();
}

void MainWindow::onLogMessage(const TQString &msg) {
    if (m_logWidget) {
        m_logWidget->message(msg);
        m_logWidget->scrollToBottom();
    }
}

void MainWindow::checkStartupEnvironment(bool forcePlymouth) {
    // 1. GCC Detection
    TQString gccPath;
    bool hasGcc = m_installerEngine->checkGccInstalled(gccPath);
    if (hasGcc) {
        onLogMessage(TQString::fromUtf8("✓ [OK] GCC compiler detected (%1).").arg(gccPath));
    } else {
        onLogMessage("[ERROR] GCC compiler is not installed! You will only be able to install pre-compiled .xbs packages, not compile new splashes.");
    }

    // 2. Plymouth Detection
    bool plymouthInstalled = false, plymouthActive = false;
    if (forcePlymouth) {
        plymouthInstalled = true;
    } else {
        m_installerEngine->checkPlymouthInstalled(plymouthInstalled, plymouthActive);
    }

    if (!plymouthInstalled) {
        onLogMessage(TQString::fromUtf8("✓ [OK] Plymouth is not installed (no display conflict detected)."));
    } else {
        onLogMessage("[WARNING] Plymouth is detected on this system! Plymouth and XBootsplash are NOT compatible.");
        if (m_skipPlymouthDialog) {
            return;
        }

        TQString warnMsg =
            "<h3><font color=\"#cc0000\">Conflict Detected: Plymouth is installed!</font></h3>"
            "<p><b>Plymouth and XBootsplash are fundamentally incompatible.</b></p>"
            "<p>Both systems attempt to control the framebuffer (/dev/fb0) and DRM/KMS graphics at early boot. "
            "Running both simultaneously may cause severe boot issues (black screens, display freezes, or lockups).</p>"
            "<p><b>Coexistence of both systems is strictly at your own risk.</b></p>"
            "<p>It is strongly recommended to uninstall Plymouth before using XBootsplash:<br>"
            "&bull; <b>Debian / Ubuntu / Q4OS:</b> sudo apt remove --purge plymouth plymouth-themes<br>"
            "&bull; <b>Fedora / RHEL:</b> sudo dnf remove plymouth<br>"
            "&bull; <b>Arch Linux:</b> sudo pacman -Rns plymouth</p>"
            "<p>Do you wish to continue anyway, or quit the application?</p>";

        int choice = showWarning(this, "Plymouth Conflict Warning", warnMsg,
                                 "Continue", "Quit", TQString::null, 0, 1);
        if (choice == 1 || choice == -1) {
            onLogMessage("Application exit requested by user due to Plymouth conflict.");
            TQTimer::singleShot(0, tqApp, SLOT(quit()));
        } else {
            onLogMessage("[WARNING] User chose to continue despite Plymouth conflict risk.");
        }
    }
}

void MainWindow::onPreviewFrameChanged(int current, int total) {
    m_scrubSlider->blockSignals(true);
    m_scrubSlider->setValue(current - 1);
    m_scrubSlider->blockSignals(false);

    double sec = (current - 1) * m_preview->frameDelay() / 1000.0;
    m_frameCounterLabel->setText(TQString(" %1 / %2 (%3s) ").arg(current).arg(total).arg(sec, 0, 'f', 2));
}

void MainWindow::onPreviewPlaybackChanged(bool isPlaying) {
    if (isPlaying) {
        m_btnPlay->setIconSet(TQIconSet(iconPause()));
        m_btnPlay->setTextLabel("Pause (Space)", true);
        TQToolTip::add(m_btnPlay, "Pause (Space)");
        if (m_viewMenu && m_actionPlayPause != -1) {
            m_viewMenu->changeItem(m_actionPlayPause, TQIconSet(iconPause()), "Pause");
        }
    } else {
        m_btnPlay->setIconSet(TQIconSet(iconPlay()));
        m_btnPlay->setTextLabel("Play (Space)", true);
        TQToolTip::add(m_btnPlay, "Play / Pause (Space)");
        if (m_viewMenu && m_actionPlayPause != -1) {
            m_viewMenu->changeItem(m_actionPlayPause, TQIconSet(iconPlay()), "Play / Pause");
        }
    }
}

void MainWindow::onPreviewFramesLoaded(int count, int /* width */, int /* height */) {
    m_scrubSlider->setMaxValue(count > 0 ? count - 1 : 0);
    m_scrubSlider->setValue(0);
    onPreviewFrameChanged(count > 0 ? 1 : 0, count);

    bool hasAnim = (count > 1);
    m_btnPrev->setEnabled(hasAnim);
    m_btnRewind->setEnabled(hasAnim);
    m_btnPlay->setEnabled(hasAnim);
    m_btnStop->setEnabled(hasAnim);
    m_btnFastForward->setEnabled(hasAnim);
    m_btnNext->setEnabled(hasAnim);
    m_scrubSlider->setEnabled(hasAnim);
}

void MainWindow::keyPressEvent(TQKeyEvent *e) {
    if (e->key() == TQt::Key_F11) {
        onLaunchFullscreen();
    } else if (e->key() == TQt::Key_Space) {
        onTogglePlayback();
    } else if (e->key() == TQt::Key_F5) {
        onStartBuild();
    } else if (e->key() == TQt::Key_Left) {
        onRewindFrame();
    } else if (e->key() == TQt::Key_Right) {
        onFastForwardFrame();
    } else if (e->key() == TQt::Key_Home) {
        onFirstFrame();
    } else if (e->key() == TQt::Key_End) {
        onLastFrame();
    } else if (e->key() == TQt::Key_Escape) {
        if (m_preview && m_preview->isColorPicking()) {
            m_preview->stopColorPicking();
            m_configPanel->onColorPickingCancelled();
            return;
        }
        if (m_preview->isPlaying()) onStopPlayback();
    } else {
        TQMainWindow::keyPressEvent(e);
    }
}

void MainWindow::closeEvent(TQCloseEvent *e) {
    if (m_installerEngine && m_installerEngine->isRunning()) {
        showWarning(this, "Operation in Progress",
                    "An initramfs regeneration is currently running.\n"
                    "Please wait for it to complete to avoid interrupting the system update.");
        e->ignore();
        return;
    }

    if (!maybeSavePrompt()) {
        e->ignore();
        return;
    }

    if (m_preview) {
        m_preview->stop();
    }
    if (m_fullscreen) {
        m_fullscreen->close();
    }
    if (m_buildEngine && m_buildEngine->isBuilding()) {
        m_buildEngine->cancelBuild();
    }

    e->accept();
    TQMainWindow::closeEvent(e);

    if (tqApp) {
        tqApp->quit();
    }
}

#include "mainwindow.moc"
