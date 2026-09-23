#include <ntqapplication.h>
#include <ntqfileinfo.h>
#include <ntqfile.h>
#include <ntqpixmap.h>
#include <ntqpopupmenu.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "mainwindow.h"
#include "config_panel.h"
#include "preview_widget.h"
#include "password_dialog.h"
#include "silent_boot_dialog.h"
#include "package_inspector_dialog.h"
#include "about_dialog.h"
#include "user_guide_dialog.h"
#include "app_icons.h"
#include "version.h"
#include "log_banner.h"
#include "build_engine.h"

static void sigHandler(int sig) {
    (void)sig;
    if (tqApp) {
        tqApp->quit();
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    TQApplication app(argc, argv);

    MainWindow mw;
    app.setMainWidget(&mw);

    int mode = -1;
    TQString framesPath = "";
    TQString bgPath = "";
    int ox = -9999, oy = -9999;
    int bgOx = -9999, bgOy = -9999;
    int selectElem = -1;
    TQString screenshotPath = "";

    TQString targetRes = "";
    bool showIntruders = false;
    bool testDrmDialog = false;
    bool testSilentBootDialog = false;
    bool testAboutDialog = false;
    bool testUserGuide = false;
    bool testPlymouthDialog = false;
    bool testAuthDialog = false;
    bool testAuthCache = false;
    bool testMode1Selection = false;
    bool testPipette = false;
    bool testCancelPipette = false;
    bool testProjectIo = false;
    bool testLogBanners = false;
    bool testVtPostState = false;
    bool testMenuIcons = false;
    bool testBuildEngine = false;
    bool testPingPong = false;
    bool testDebExport = false;
    bool expandPos = false;
    bool expandColor = false;
    bool expandTime = false;
    bool expandBuild = false;
    int pipetteX = -1, pipetteY = -1;
    bool pipetteClick = false;
    bool quitImmediately = false;
    TQString projectFile = "";
    TQString saveProjectPath = "";
    TQString testBusyText = "";
    TQString testInspectorPkg = "";
    int superCompressArg = -1;

    for (int i = 1; i < argc; ++i) {
        TQString arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            mode = TQString(argv[++i]).toInt();
        } else if (arg == "--res" && i + 1 < argc) {
            targetRes = argv[++i];
        } else if (arg == "--quit") {
            quitImmediately = true;
        } else if (arg == "--project" && i + 1 < argc) {
            projectFile = argv[++i];
        } else if (arg == "--save-project" && i + 1 < argc) {
            saveProjectPath = argv[++i];
        } else if (arg.endsWith(".xbsp") && projectFile.isEmpty()) {
            projectFile = arg;
        } else if (arg == "--test-project-io") {
            testProjectIo = true;
        } else if (arg == "--frames" && i + 1 < argc) {
            framesPath = argv[++i];
        } else if (arg == "--bg" && i + 1 < argc) {
            bgPath = argv[++i];
        } else if (arg == "--ox" && i + 1 < argc) {
            ox = TQString(argv[++i]).toInt();
        } else if (arg == "--oy" && i + 1 < argc) {
            oy = TQString(argv[++i]).toInt();
        } else if (arg == "--bg-ox" && i + 1 < argc) {
            bgOx = TQString(argv[++i]).toInt();
        } else if (arg == "--bg-oy" && i + 1 < argc) {
            bgOy = TQString(argv[++i]).toInt();
        } else if (arg == "--select" && i + 1 < argc) {
            TQString s = argv[++i];
            if (s == "bg" || s == "1") selectElem = 1;
            else selectElem = 0;
        } else if (arg == "--show-intruders") {
            showIntruders = true;
        } else if (arg == "--test-drm-dialog") {
            testDrmDialog = true;
        } else if (arg == "--test-silent-boot") {
            testSilentBootDialog = true;
        } else if (arg == "--test-about-dialog") {
            testAboutDialog = true;
        } else if (arg == "--test-user-guide") {
            testUserGuide = true;
        } else if (arg == "--test-plymouth-dialog") {
            testPlymouthDialog = true;
        } else if (arg == "--test-auth-dialog") {
            testAuthDialog = true;
        } else if (arg == "--test-auth-cache") {
            testAuthCache = true;
        } else if (arg == "--test-mode1-selection") {
            testMode1Selection = true;
        } else if (arg == "--test-log-banners") {
            testLogBanners = true;
        } else if (arg == "--test-vt-post-state") {
            testVtPostState = true;
        } else if (arg == "--test-menu-icons") {
            testMenuIcons = true;
        } else if (arg == "--test-build-engine") {
            testBuildEngine = true;
        } else if (arg == "--test-pipette") {
            testPipette = true;
        } else if (arg == "--test-cancel-pipette") {
            testCancelPipette = true;
        } else if (arg == "--test-pingpong") {
            testPingPong = true;
        } else if (arg == "--test-deb-export") {
            testDebExport = true;
        } else if (arg == "--expand-pos") {
            expandPos = true;
        } else if (arg == "--expand-color") {
            expandColor = true;
        } else if (arg == "--expand-time") {
            expandTime = true;
        } else if (arg == "--expand-build") {
            expandBuild = true;
        } else if (arg == "--pipette-x" && i + 1 < argc) {
            pipetteX = TQString(argv[++i]).toInt();
        } else if (arg == "--pipette-y" && i + 1 < argc) {
            pipetteY = TQString(argv[++i]).toInt();
        } else if (arg == "--pipette-click") {
            pipetteClick = true;
        } else if (arg == "--test-busy-status" && i + 1 < argc) {
            testBusyText = argv[++i];
        } else if (arg == "--test-inspector" && i + 1 < argc) {
            testInspectorPkg = argv[++i];
        } else if (arg == "--super-compress" && i + 1 < argc) {
            superCompressArg = TQString(argv[++i]).toInt();
        } else if (arg == "--screenshot" && i + 1 < argc) {
            screenshotPath = argv[++i];
        } else if (!arg.startsWith("-") && framesPath.isEmpty()) {
            framesPath = arg;
        }
    }

    if (!framesPath.isEmpty()) {
        TQFileInfo fi(framesPath);
        if (fi.isFile() && mode < 0) {
            mode = 3;
        }
        if (mode >= 0) {
            mw.configPanel()->setDisplayMode(mode);
        }
    } else if (mode >= 0) {
        mw.configPanel()->setDisplayMode(mode);
    }
    if (!bgPath.isEmpty()) {
        mw.configPanel()->setBgImagePath(bgPath);
    }
    if (!targetRes.isEmpty()) {
        mw.configPanel()->setTargetResolution(targetRes);
    }
    if (ox != -9999 || oy != -9999) {
        int curX = (ox != -9999) ? ox : mw.previewWidget()->offsetX();
        int curY = (oy != -9999) ? oy : mw.previewWidget()->offsetY();
        mw.previewWidget()->setOffsets(curX, curY);
        mw.configPanel()->onPreviewOffsetChanged(curX, curY);
    }
    if (bgOx != -9999 || bgOy != -9999) {
        int curX = (bgOx != -9999) ? bgOx : mw.previewWidget()->bgOffsetX();
        int curY = (bgOy != -9999) ? bgOy : mw.previewWidget()->bgOffsetY();
        mw.previewWidget()->setBgOffsets(curX, curY);
        mw.configPanel()->onPreviewBgOffsetChanged(curX, curY);
    }
    if (selectElem >= 0) {
        mw.previewWidget()->setSelectedElement((SplashPreviewWidget::SelectedElement)selectElem);
        mw.configPanel()->onPreviewSelectedElementChanged(selectElem);
    }

    if (testProjectIo || testVtPostState || testSilentBootDialog || testPlymouthDialog ||
        testAuthDialog || !testInspectorPkg.isEmpty() || testMode1Selection ||
        testPipette || testCancelPipette || testBuildEngine || testPingPong || !screenshotPath.isEmpty()) {
        mw.setSkipPlymouthDialog(true);
    }

    mw.show();
    for (int step = 0; step < 10; ++step) {
        app.processEvents();
    }

    if (!framesPath.isEmpty()) {
        mw.configPanel()->setFramesPath(framesPath, showIntruders);
    }

    if (selectElem >= 0) {
        mw.previewWidget()->setSelectedElement((SplashPreviewWidget::SelectedElement)selectElem);
        mw.configPanel()->onPreviewSelectedElementChanged(selectElem);
    }

    if (expandPos) {
        mw.configPanel()->expandPosSection();
    }

    if (expandColor) {
        mw.configPanel()->expandColorSection();
    }

    if (testDrmDialog) {
        mw.configPanel()->setUseDrm(true, true);
    }

    if (testSilentBootDialog) {
        SilentBootDialog dlg(&mw);
        dlg.show();
        for (int step = 0; step < 10; ++step) {
            app.processEvents();
        }
        if (!screenshotPath.isEmpty()) {
            TQPixmap pm = TQPixmap::grabWidget(&dlg);
            pm.save(screenshotPath, "PNG");
            return 0;
        }
        dlg.exec();
        return 0;
    }

    if (testAboutDialog) {
        mw.setSkipPlymouthDialog(true);
        AboutDialog dlg(&mw);
        dlg.show();
        app.processEvents();

        // Save frame 0 (about12, central small splash)
        TQPixmap pm0 = TQPixmap::grabWidget(&dlg);
        pm0.save("/home/cdef/.gemini/antigravity-ide/brain/71f461db-7163-4cc6-9812-5d10ba938dd3/about_dialog_frame0.png", "PNG");

        // Step to frame 6
        for (int s = 0; s < 6; ++s) {
            dlg.stepFrameForTest();
            app.processEvents();
        }
        TQPixmap pm6 = TQPixmap::grabWidget(&dlg);
        pm6.save("/home/cdef/.gemini/antigravity-ide/brain/71f461db-7163-4cc6-9812-5d10ba938dd3/about_dialog_frame6.png", "PNG");

        // Step to final frame (frame 11)
        for (int s = 6; s < 11; ++s) {
            dlg.stepFrameForTest();
            app.processEvents();
        }
        TQPixmap pm11 = TQPixmap::grabWidget(&dlg);
        pm11.save("/home/cdef/.gemini/antigravity-ide/brain/71f461db-7163-4cc6-9812-5d10ba938dd3/about_dialog_frame11.png", "PNG");

        printf("TEST_ABOUT_DIALOG: PASS\n");
        return 0;
    }

    if (testUserGuide) {
        mw.setSkipPlymouthDialog(true);
        UserGuideDialog dlg(&mw);
        dlg.show();
        for (int step = 0; step < 10; ++step) {
            app.processEvents();
        }

        // Cycle through all topics to verify no crashes or errors
        for (int t = 0; t < 7; ++t) {
            dlg.selectTopic(t);
            app.processEvents();
        }

        // Select Chapter 0 (Overview) and save screenshot
        dlg.selectTopic(0);
        app.processEvents();
        if (!screenshotPath.isEmpty()) {
            TQPixmap pm0 = TQPixmap::grabWidget(&dlg);
            pm0.save(screenshotPath, "PNG");
        }

        // Select Chapter 3 (Compression: UPKR vs ZX0) and save screenshot
        dlg.selectTopic(3);
        app.processEvents();
        if (!screenshotPath.isEmpty()) {
            TQString p3 = screenshotPath;
            int dot = p3.findRev('.');
            if (dot > 0) p3 = p3.left(dot) + "_ch3" + p3.mid(dot);
            TQPixmap pm3 = TQPixmap::grabWidget(&dlg);
            pm3.save(p3, "PNG");
        }

        // Select Chapter 1 (Tutorial) and save screenshot scrolled to Step 3
        dlg.selectTopic(1);
        dlg.scrollContents(0, 200);
        app.processEvents();
        if (!screenshotPath.isEmpty()) {
            TQString p1 = screenshotPath;
            int dot = p1.findRev('.');
            if (dot > 0) p1 = p1.left(dot) + "_ch1" + p1.mid(dot);
            TQPixmap pm1 = TQPixmap::grabWidget(&dlg);
            pm1.save(p1, "PNG");
        }

        printf("TEST_USER_GUIDE: PASS\n");
        return 0;
    }

    if (testPlymouthDialog) {
        mw.setSkipPlymouthDialog(true);
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

        TQMessageBox mb("Plymouth Conflict Warning", warnMsg, TQMessageBox::NoIcon,
                         1 | TQMessageBox::Default, 2 | TQMessageBox::Escape, 0, &mw, 0, true);
        mb.setButtonText(1, "Continue");
        mb.setButtonText(2, "Quit");
        mb.setIconPixmap(iconWarn());
        mb.show();
        for (int step = 0; step < 10; ++step) {
            app.processEvents();
        }
        if (!screenshotPath.isEmpty()) {
            TQPixmap pm = TQPixmap::grabWidget(&mb);
            pm.save(screenshotPath, "PNG");
            return 0;
        }
        mb.exec();
        return 0;
    }

    if (testAuthDialog) {
        SplashPasswordDialog dlg(&mw);
        dlg.show();
        for (int step = 0; step < 10; ++step) {
            app.processEvents();
        }
        if (!screenshotPath.isEmpty()) {
            TQPixmap pm = TQPixmap::grabWidget(&dlg);
            pm.save(screenshotPath, "PNG");
            return 0;
        }
    }

    if (!testInspectorPkg.isEmpty()) {
        PackageInspectorDialog dlg(testInspectorPkg, mw.packageManager(), mw.installerEngine(), mw.projectRoot(), &mw);
        dlg.show();
        for (int step = 0; step < 15; ++step) {
            app.processEvents();
            usleep(20000);
        }
        if (!screenshotPath.isEmpty()) {
            TQPixmap pm = TQPixmap::grabWidget(&dlg);
            pm.save(screenshotPath, "PNG");
            return 0;
        }
        return dlg.exec();
    }

    if (testCancelPipette) {
        mw.configPanel()->expandColorSection();
        mw.configPanel()->triggerPipette();
        app.processEvents();
        bool activeBefore = mw.previewWidget()->isColorPicking();
        mw.configPanel()->cancelPipetteIfActive();
        app.processEvents();
        bool activeAfter = mw.previewWidget()->isColorPicking();
        printf("TEST_CANCEL_PIPETTE: before=%d after=%d\n", activeBefore ? 1 : 0, activeAfter ? 1 : 0);
        return (activeBefore && !activeAfter) ? 0 : 1;
    }

    if (testPipette) {
        mw.configPanel()->expandColorSection();
        mw.configPanel()->triggerPipette();
        app.processEvents();
        if (pipetteX >= 0 && pipetteY >= 0) {
            TQPoint pt(pipetteX, pipetteY);
            TQMouseEvent moveEv(TQEvent::MouseMove, pt, TQt::NoButton, TQt::NoButton);
            TQApplication::sendEvent(mw.previewWidget(), &moveEv);
            app.processEvents();
            if (pipetteClick) {
                TQMouseEvent clickEv(TQEvent::MouseButtonPress, pt, TQt::LeftButton, TQt::LeftButton);
                TQApplication::sendEvent(mw.previewWidget(), &clickEv);
                app.processEvents();
            }
        }
    }

    if (!projectFile.isEmpty()) {
        mw.loadProject(projectFile);
    }
    if (expandColor) {
        mw.configPanel()->expandColorSection();
    }
    if (expandTime) {
        mw.configPanel()->expandTimeSection();
    }
    if (expandBuild) {
        mw.configPanel()->expandBuildSection();
    }
    if (superCompressArg >= 0) {
        mw.configPanel()->setSuperCompression(superCompressArg);
    }
    if (!saveProjectPath.isEmpty()) {
        mw.saveProject(saveProjectPath);
        if (quitImmediately) return 0;
    }
    if (quitImmediately) return 0;

    if (testLogBanners) {
        TQString bBuild = SplashLog::banner("BUILD");
        TQString bInstall = SplashLog::banner("INSTALL");
        TQString bExtract = SplashLog::banner("EXTRACT");
        TQString bDone = SplashLog::doneBanner();
        TQString bFailed = SplashLog::failedBanner();

        if (bBuild.length() != 70 || bInstall.length() != 70 || bExtract.length() != 70) {
            printf("TEST_LOG_BANNERS: FAIL (header width not 70: build=%d install=%d extract=%d)\n",
                   (int)bBuild.length(), (int)bInstall.length(), (int)bExtract.length());
            return 1;
        }
        if (bDone.length() != 70 || bFailed.length() != 70) {
            printf("TEST_LOG_BANNERS: FAIL (footer width not 70: done=%d failed=%d)\n",
                   (int)bDone.length(), (int)bFailed.length());
            return 1;
        }

        mw.onLogMessage(bBuild);
        mw.onLogMessage("Display mode : 0 (Animation on solid background)");
        mw.onLogMessage("Source       : /home/user/splash/frames");
        mw.onLogMessage("Target binary: xbs_amiga");
        mw.onLogMessage("Backend      : DRM/KMS");
        mw.onLogMessage("[1/3] Compiling splash generator (generate_splash)...");
        mw.onLogMessage("Generator compiled successfully.");
        mw.onLogMessage("[2/3] Generating splash data (frames_delta.h)...");
        mw.onLogMessage("Header frames_delta.h generated successfully.");
        mw.onLogMessage("[3/3] Compiling standalone binary xbs_amiga...");
        mw.onLogMessage("✔ Standalone binary compiled: /sbin/xbs_amiga");
        mw.onLogMessage("  Size: 1.12 MB (1177648 bytes)");
        mw.onLogMessage(bDone);

        mw.onLogMessage("");
        mw.onLogMessage(bExtract);
        mw.onLogMessage("Package     : matrix_drm.xbs");
        mw.onLogMessage("Destination : /home/user/packages/extracted_matrix");
        mw.onLogMessage("Extracted binary: xbs_matrix");
        mw.onLogMessage(bDone);

        mw.onLogMessage("");
        mw.onLogMessage(bInstall);
        mw.onLogMessage("Target binary : xbs_broken");
        mw.onLogMessage("[ERROR] Source binary does not exist: /sbin/xbs_broken");
        mw.onLogMessage(bFailed);

        app.processEvents();

        if (!screenshotPath.isEmpty()) {
            TQPixmap pm = TQPixmap::grabWidget(&mw);
            pm.save(screenshotPath, "PNG");
            printf("Screenshot saved to %s\n", screenshotPath.latin1());
        }

        printf("TEST_LOG_BANNERS: PASS\n");
        return 0;
    }

    if (testAuthCache) {
        SplashPasswordDialog::clearCachedPassword();
        if (SplashPasswordDialog::hasCachedPassword()) {
            printf("TEST_AUTH_CACHE: FAIL (hasCachedPassword should be false initially)\n");
            return 1;
        }
        SplashPasswordDialog::setCachedPassword("sessionSecret99");
        if (!SplashPasswordDialog::hasCachedPassword()) {
            printf("TEST_AUTH_CACHE: FAIL (hasCachedPassword should be true after setCachedPassword)\n");
            return 1;
        }
        TQString pwdOut;
        bool ok = SplashPasswordDialog::getPassword(NULL, pwdOut);
        if (!ok || pwdOut != "sessionSecret99") {
            printf("TEST_AUTH_CACHE: FAIL (getPassword didn't return cached password: ok=%d pwd='%s')\n",
                   ok ? 1 : 0, pwdOut.latin1());
            return 1;
        }
        SplashPasswordDialog::clearCachedPassword();
        if (SplashPasswordDialog::hasCachedPassword()) {
            printf("TEST_AUTH_CACHE: FAIL (hasCachedPassword should be false after clear)\n");
            return 1;
        }
        printf("TEST_AUTH_CACHE: PASS\n");
        return 0;
    }

    if (testMode1Selection) {
        mw.configPanel()->setDisplayMode(1);
        mw.configPanel()->setFramesPath(mw.projectRoot() + "/amiga", false);
        mw.configPanel()->setBgImagePath(mw.projectRoot() + "/amiga/frame_00_delay-0.16s.png");

        mw.previewWidget()->setOffsets(10, 20);
        mw.configPanel()->onPreviewOffsetChanged(10, 20);
        mw.previewWidget()->setBgOffsets(100, 200);
        mw.configPanel()->onPreviewBgOffsetChanged(100, 200);

        // 1. Select Animation
        mw.configPanel()->onSelectAnimClicked();
        if (mw.previewWidget()->selectedElement() != SplashPreviewWidget::SelectAnimation) {
            printf("TEST_MODE1_SELECTION: FAIL (SelectAnimation not set)\n");
            return 1;
        }

        // Change X slider to 30
        mw.configPanel()->onOffsetXSlider(30);
        if (mw.configPanel()->offsetX() != 30 || mw.previewWidget()->offsetX() != 30) {
            printf("TEST_MODE1_SELECTION: FAIL (anim offset not 30: panel=%d widget=%d)\n",
                   mw.configPanel()->offsetX(), mw.previewWidget()->offsetX());
            return 1;
        }
        if (mw.configPanel()->bgOffsetX() != 100 || mw.previewWidget()->bgOffsetX() != 100) {
            printf("TEST_MODE1_SELECTION: FAIL (bg offset was modified unexpectedly: %d)\n",
                   mw.configPanel()->bgOffsetX());
            return 1;
        }

        // 2. Select Static Image (Bg)
        mw.configPanel()->onSelectBgClicked();
        if (mw.previewWidget()->selectedElement() != SplashPreviewWidget::SelectBgImage) {
            printf("TEST_MODE1_SELECTION: FAIL (SelectBgImage not set)\n");
            return 1;
        }

        // Change X slider to 150
        mw.configPanel()->onOffsetXSlider(150);
        if (mw.configPanel()->bgOffsetX() != 150 || mw.previewWidget()->bgOffsetX() != 150) {
            printf("TEST_MODE1_SELECTION: FAIL (bg offset not 150: panel=%d widget=%d)\n",
                   mw.configPanel()->bgOffsetX(), mw.previewWidget()->bgOffsetX());
            return 1;
        }
        if (mw.configPanel()->offsetX() != 30 || mw.previewWidget()->offsetX() != 30) {
            printf("TEST_MODE1_SELECTION: FAIL (anim offset was modified unexpectedly: %d)\n",
                   mw.configPanel()->offsetX());
            return 1;
        }

        // 3. Reset while Static Image is selected
        mw.configPanel()->onResetOffsets();
        if (mw.configPanel()->bgOffsetX() != 0 || mw.previewWidget()->bgOffsetX() != 0) {
            printf("TEST_MODE1_SELECTION: FAIL (bg offset not reset to 0: %d)\n",
                   mw.configPanel()->bgOffsetX());
            return 1;
        }
        if (mw.configPanel()->offsetX() != 30 || mw.previewWidget()->offsetX() != 30) {
            printf("TEST_MODE1_SELECTION: FAIL (anim offset changed during bg reset: %d)\n",
                   mw.configPanel()->offsetX());
            return 1;
        }

        // 4. Reset while Animation is selected
        mw.configPanel()->onSelectAnimClicked();
        mw.configPanel()->onResetOffsets();
        if (mw.configPanel()->offsetX() != 0 || mw.previewWidget()->offsetX() != 0) {
            printf("TEST_MODE1_SELECTION: FAIL (anim offset not reset to 0: %d)\n",
                   mw.configPanel()->offsetX());
            return 1;
        }

        printf("TEST_MODE1_SELECTION: PASS\n");
        return 0;
    }

    if (testProjectIo) {
        TQString tmpPath = "/tmp/test_studio_roundtrip.xbsp";
        unlink(tmpPath.latin1());

        // 1. Configure non-default settings
        mw.configPanel()->setDisplayMode(1);
        mw.configPanel()->setTargetResolution("1920x1080");
        mw.configPanel()->setFramesPath(mw.projectRoot() + "/amiga", false);
        mw.configPanel()->setBgImagePath(mw.projectRoot() + "/amiga/frame_00_delay-0.16s.png");
        mw.previewWidget()->setOffsets(12, 34);
        mw.configPanel()->onPreviewOffsetChanged(12, 34);
        mw.previewWidget()->setBgOffsets(56, 78);
        mw.configPanel()->onPreviewBgOffsetChanged(56, 78);
        mw.configPanel()->setBinaryName("xbs_test_proj");
        mw.configPanel()->setAuthor("seb3773");
        mw.configPanel()->setNotes("Workbench 1.3 tribute");
        mw.configPanel()->setUseDrm(true, false);
        mw.configPanel()->setUseZx0(true);
        mw.configPanel()->setLoopMode(1); // Infinite loop
        mw.configPanel()->setFrameDelay(45);
        mw.configPanel()->setLoopStart(7);
        mw.configPanel()->setMinBootLoops(2);

        // 2. Save project
        bool saved = mw.saveProject(tmpPath);
        if (!saved) {
            printf("TEST_PROJECT_IO: FAILED (saveProject returned false)\n");
            return 1;
        }

        // Verify file exists on disk
        if (!TQFile::exists(tmpPath)) {
            printf("TEST_PROJECT_IO: FAILED (file does not exist on disk)\n");
            return 1;
        }

        // 3. Reset to defaults
        mw.configPanel()->resetToDefaults();
        if (mw.configPanel()->displayMode() != 0 || !mw.configPanel()->framesPath().isEmpty() || mw.configPanel()->useZx0() ||
            !mw.configPanel()->author().isEmpty() || !mw.configPanel()->notes().isEmpty()) {
            printf("TEST_PROJECT_IO: FAILED (resetToDefaults failed)\n");
            return 1;
        }

        // 4. Reload project
        mw.loadProject(tmpPath);

        // 5. Verify restored state
        bool ok = true;
        if (mw.configPanel()->displayMode() != 1) { printf("FAIL: mode != 1 (is %d)\n", mw.configPanel()->displayMode()); ok = false; }
        if (mw.configPanel()->offsetX() != 12 || mw.configPanel()->offsetY() != 34) {
            printf("FAIL: offsets (%d,%d) != (12,34)\n", mw.configPanel()->offsetX(), mw.configPanel()->offsetY());
            ok = false;
        }
        if (mw.configPanel()->bgOffsetX() != 56 || mw.configPanel()->bgOffsetY() != 78) {
            printf("FAIL: bg offsets (%d,%d) != (56,78)\n", mw.configPanel()->bgOffsetX(), mw.configPanel()->bgOffsetY());
            ok = false;
        }
        if (mw.configPanel()->binaryName() != "xbs_test_proj") {
            printf("FAIL: binaryName '%s' != 'xbs_test_proj'\n", mw.configPanel()->binaryName().latin1());
            ok = false;
        }
        if (mw.configPanel()->author() != "seb3773") {
            printf("FAIL: author '%s' != 'seb3773'\n", mw.configPanel()->author().latin1());
            ok = false;
        }
        if (mw.configPanel()->notes() != "Workbench 1.3 tribute") {
            printf("FAIL: notes '%s' != 'Workbench 1.3 tribute'\n", mw.configPanel()->notes().latin1());
            ok = false;
        }
        if (!mw.configPanel()->useDrm()) { printf("FAIL: useDrm is false\n"); ok = false; }
        if (!mw.configPanel()->useZx0()) { printf("FAIL: useZx0 is false\n"); ok = false; }
        if (mw.configPanel()->loopMode() != 1) {
            printf("FAIL: loopMode %d != 1 (Infinite loop not restored!)\n", mw.configPanel()->loopMode());
            ok = false;
        }
        if (mw.configPanel()->frameDelay() != 45) {
            printf("FAIL: frameDelay %d != 45\n", mw.configPanel()->frameDelay());
            ok = false;
        }
        if (mw.configPanel()->loopStart() != 7) {
            printf("FAIL: loopStart %d != 7\n", mw.configPanel()->loopStart());
            ok = false;
        }
        if (mw.configPanel()->minBootLoops() != 2) {
            printf("FAIL: minBootLoops %d != 2\n", mw.configPanel()->minBootLoops());
            ok = false;
        }
        if (mw.configPanel()->framesPath().isEmpty() || !mw.configPanel()->framesPath().endsWith("amiga")) {
            printf("FAIL: framesPath '%s'\n", mw.configPanel()->framesPath().latin1());
            ok = false;
        }

        // Test Package Metadata roundtrip (export & read)
        TQString dummyBin = "/tmp/dummy_test_bin";
        TQFile fBin(dummyBin);
        if (fBin.open(IO_WriteOnly)) {
            fBin.writeBlock("DUMMY_BINARY_DATA", 17);
            fBin.close();
        }
        SplashPackageManager pkgMgr;
        SplashPackageManager::PackageMetadata pMeta;
        pMeta.splashName = "amiga_tribute";
        pMeta.author = "seb3773";
        pMeta.notes = "Commodore Amiga boot theme";
        pMeta.resolution = "1920x1080";
        pMeta.displayMode = 1;
        pMeta.frameW = 320;
        pMeta.frameH = 240;
        pMeta.nframes = 12;
        pMeta.frameDelay = 33;
        TQString outPkg, pkgErr;
        bool expOk = pkgMgr.exportPackage(dummyBin, pMeta, "/tmp", outPkg, pkgErr);
        if (!expOk) {
            printf("FAIL: exportPackage failed: %s\n", pkgErr.latin1());
            ok = false;
        } else {
            SplashPackageManager::PackageMetadata readMeta;
            TQString prevPath, binName, readErr;
            bool readOk = pkgMgr.readPackageMetadata(outPkg, readMeta, prevPath, binName, readErr);
            if (!readOk) {
                printf("FAIL: readPackageMetadata failed: %s\n", readErr.latin1());
                ok = false;
            } else {
                if (readMeta.author != "seb3773") {
                    printf("FAIL: readMeta.author '%s' != 'seb3773'\n", readMeta.author.latin1());
                    ok = false;
                }
                if (readMeta.notes != "Commodore Amiga boot theme") {
                    printf("FAIL: readMeta.notes '%s' != 'Commodore Amiga boot theme'\n", readMeta.notes.latin1());
                    ok = false;
                }
            }
            unlink(outPkg.latin1());
        }
        unlink(dummyBin.latin1());

        printf("TEST_PROJECT_IO: %s\n", ok ? "PASS" : "FAIL");
        unlink(tmpPath.latin1());
        return ok ? 0 : 1;
    }

    if (testDebExport) {
        printf("Running Debian Package Export test...\n");
        TQString dummyBin = "/tmp/dummy_deb_bin";
        TQFile fBin(dummyBin);
        if (fBin.open(IO_WriteOnly)) {
            fBin.writeBlock("DUMMY_ELF_BOOTSPLASH_DATA", 26);
            fBin.close();
            chmod(dummyBin.latin1(), 0755);
        }

        SplashPackageManager pkgMgr;
        SplashPackageManager::PackageMetadata meta;
        meta.splashName = "amiga";
        meta.author = "seb3773";
        meta.notes = "Commodore Amiga Workbench 1.3 tribute boot animation";
        meta.backend = "fbdev";
        meta.resolution = "1920x1080";
        meta.displayMode = 1;
        meta.frameW = 320;
        meta.frameH = 240;
        meta.nframes = 32;
        meta.frameDelay = 33;

        TQString outDeb, err;
        TQString debTarget = "/tmp/test_export.deb";
        unlink(debTarget.latin1());

        bool ok = pkgMgr.exportDebianPackage(dummyBin, meta, debTarget, false, outDeb, err);
        if (!ok) {
            printf("FAIL: exportDebianPackage returned false: %s\n", err.latin1());
            unlink(dummyBin.latin1());
            return 1;
        }

        if (!TQFile::exists(outDeb)) {
            printf("FAIL: outDeb '%s' does not exist\n", outDeb.latin1());
            unlink(dummyBin.latin1());
            return 1;
        }

        // Verify with dpkg-deb -I
        TQString cmdInfo = TQString("dpkg-deb -I \"%1\" > /tmp/deb_info.txt 2>&1").arg(outDeb);
        if (system(cmdInfo.latin1()) != 0) {
            printf("FAIL: dpkg-deb -I failed on generated deb\n");
            unlink(dummyBin.latin1());
            unlink(outDeb.latin1());
            return 1;
        }

        TQFile fInfo("/tmp/deb_info.txt");
        if (fInfo.open(IO_ReadOnly)) {
            TQByteArray arr = fInfo.readAll();
            fInfo.close();
            TQString str(arr);
            if (!str.contains("Package: xbootsplash-theme-amiga")) {
                printf("FAIL: Package name missing or incorrect in control file\n");
                ok = false;
            }
            if (!str.contains("Maintainer: seb3773 <seb3773@localhost>")) {
                printf("FAIL: Maintainer missing or incorrect in control file\n");
                ok = false;
            }
            if (!str.contains("Depends: initramfs-tools")) {
                printf("FAIL: Depends missing in control file\n");
                ok = false;
            }
            if (!str.contains("Provides: xbootsplash-theme")) {
                printf("FAIL: Provides missing in control file\n");
                ok = false;
            }
        }
        unlink("/tmp/deb_info.txt");

        // Verify with dpkg-deb -c
        TQString cmdContents = TQString("dpkg-deb -c \"%1\" > /tmp/deb_contents.txt 2>&1").arg(outDeb);
        if (system(cmdContents.latin1()) != 0) {
            printf("FAIL: dpkg-deb -c failed on generated deb\n");
            unlink(dummyBin.latin1());
            unlink(outDeb.latin1());
            return 1;
        }

        TQFile fCont("/tmp/deb_contents.txt");
        if (fCont.open(IO_ReadOnly)) {
            TQByteArray arr = fCont.readAll();
            fCont.close();
            TQString str(arr);
            if (!str.contains("./sbin/dummy_deb_bin")) {
                printf("FAIL: ./sbin/dummy_deb_bin missing from package contents\n");
                ok = false;
            }
            if (!str.contains("./etc/initramfs-tools/hooks/dummy_deb_bin")) {
                printf("FAIL: initramfs hook missing from package contents\n");
                ok = false;
            }
            if (!str.contains("./etc/initramfs-tools/scripts/init-top/dummy_deb_bin")) {
                printf("FAIL: init-top script missing from package contents\n");
                ok = false;
            }
            if (!str.contains("./etc/initramfs-tools/scripts/init-bottom/dummy_deb_bin")) {
                printf("FAIL: init-bottom script missing from package contents\n");
                ok = false;
            }
            if (!str.contains("./etc/initramfs-tools/scripts/local-premount/00_dummy_deb_bin")) {
                printf("FAIL: local-premount script missing from package contents\n");
                ok = false;
            }
        }
        unlink("/tmp/deb_contents.txt");
        unlink(outDeb.latin1());

        // Test convertPackageToDeb from .xbs
        TQString outPkg, pkgErr;
        bool expPkgOk = pkgMgr.exportPackage(dummyBin, meta, "/tmp", outPkg, pkgErr);
        if (!expPkgOk) {
            printf("FAIL: exportPackage failed: %s\n", pkgErr.latin1());
            ok = false;
        } else {
            TQString convertedDeb = "/tmp/converted_from_xbs.deb";
            unlink(convertedDeb.latin1());
            TQString convOut, convErr;
            bool convOk = pkgMgr.convertPackageToDeb(outPkg, convertedDeb, false, convOut, convErr);
            if (!convOk || !TQFile::exists(convOut)) {
                printf("FAIL: convertPackageToDeb failed: %s\n", convErr.latin1());
                ok = false;
            } else {
                unlink(convOut.latin1());
            }
            unlink(outPkg.latin1());
        }

        unlink(dummyBin.latin1());
        printf("TEST_DEB_EXPORT: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 1;
    }

    if (testPingPong) {
        TQString tmpPath = "/tmp/test_pingpong_project.xbsp";
        unlink(tmpPath.latin1());

        // 1. Configure Ping-Pong mode (mode 3)
        mw.configPanel()->setLoopMode(3);
        if (mw.configPanel()->loopMode() != 3) {
            printf("TEST_PINGPONG: FAIL (loopMode() is %d, expected 3)\n", mw.configPanel()->loopMode());
            return 1;
        }

        // 2. Save project file
        if (!mw.saveProject(tmpPath)) {
            printf("TEST_PINGPONG: FAIL (saveProject returned false)\n");
            return 1;
        }

        // 3. Reset to defaults (which defaults to full loop = 1)
        mw.configPanel()->resetToDefaults();
        if (mw.configPanel()->loopMode() != 1) {
            printf("TEST_PINGPONG: FAIL (resetToDefaults loopMode is %d, expected 1)\n", mw.configPanel()->loopMode());
            return 1;
        }

        // 4. Reload project and verify loop mode 3 is preserved
        mw.loadProject(tmpPath);
        if (mw.configPanel()->loopMode() != 3) {
            printf("TEST_PINGPONG: FAIL (after reload, loopMode is %d, expected 3)\n", mw.configPanel()->loopMode());
            return 1;
        }

        // 5. Test preview widget bouncing
        SplashPreviewWidget *pw = mw.previewWidget();
        pw->loadFramesFromDir(mw.projectRoot() + "/datas/pacman", false);
        pw->setLoopMode(3, 0);
        pw->stop(); // resets to frame 0, direction 1

        if (pw->currentFrameIndex() != 0) {
            printf("TEST_PINGPONG: FAIL (initial frame is %d, expected 0)\n", pw->currentFrameIndex());
            return 1;
        }

        // Total frames in datas/pacman is 12 (indices 0..11)
        // Simulate forward playback: ticks 1 to 11 reach frame 11
        for (int step = 1; step <= 11; ++step) {
            pw->stepTimerForTest();
            if (pw->currentFrameIndex() != step) {
                printf("TEST_PINGPONG: FAIL (step %d: frame is %d, expected %d)\n", step, pw->currentFrameIndex(), step);
                return 1;
            }
        }

        // Next tick (tick 12) should bounce backward to frame 10 (not 11 or 0!)
        pw->stepTimerForTest();
        if (pw->currentFrameIndex() != 10) {
            printf("TEST_PINGPONG: FAIL (tick 12: frame is %d, expected 10)\n", pw->currentFrameIndex());
            return 1;
        }

        // Step backward: frames 9 down to 0
        for (int expected = 9; expected >= 0; --expected) {
            pw->stepTimerForTest();
            if (pw->currentFrameIndex() != expected) {
                printf("TEST_PINGPONG: FAIL (reverse: frame is %d, expected %d)\n", pw->currentFrameIndex(), expected);
                return 1;
            }
        }

        // Next tick after reaching 0 should bounce forward to frame 1!
        pw->stepTimerForTest();
        if (pw->currentFrameIndex() != 1) {
            printf("TEST_PINGPONG: FAIL (bounce at 0: frame is %d, expected 1)\n", pw->currentFrameIndex());
            return 1;
        }

        unlink(tmpPath.latin1());
        printf("TEST_PINGPONG: PASS\n");
        return 0;
    }

    if (testVtPostState) {
        // 1. Initial state with relative amiga path
        TQString amigaRel = TQFile::exists(mw.projectRoot() + "/datas/amiga") ? "datas/amiga" : "amiga";
        mw.configPanel()->setFramesPath(amigaRel, false);
        mw.configPanel()->setBinaryName("xbs_amiga");
        app.processEvents();

        // 2. Binary on disk exists
        TQString expectedBin = mw.projectRoot() + "/xbs_amiga";
        if (!TQFile::exists(expectedBin)) {
            printf("TEST_VT_POST_STATE: FAIL (expected binary %s does not exist)\n", expectedBin.latin1());
            return 1;
        }

        // 3. Before build simulation, candidate binary is discovered on disk
        TQString binPre = mw.currentBinaryPath();
        if (binPre != expectedBin) {
            printf("TEST_VT_POST_STATE: FAIL (pre-build lookup '%s' != '%s')\n",
                   binPre.latin1(), expectedBin.latin1());
            return 1;
        }
        if (!mw.isTestLiveEnabled()) {
            printf("TEST_VT_POST_STATE: FAIL (Test Live button should be enabled when candidate binary exists)\n");
            return 1;
        }

        // 4. Simulate build completed
        mw.simulateBuildFinished(true, expectedBin, 270624);
        app.processEvents();

        TQString binAfterBuild = mw.currentBinaryPath();
        if (binAfterBuild != expectedBin) {
            printf("TEST_VT_POST_STATE: FAIL (binAfterBuild '%s' != '%s')\n",
                   binAfterBuild.latin1(), expectedBin.latin1());
            return 1;
        }
        if (!mw.isTestLiveEnabled()) {
            printf("TEST_VT_POST_STATE: FAIL (Test Live button should be enabled after build)\n");
            return 1;
        }

        // 5. Simulate directory selection with trailing slash
        TQString testSlashDir = mw.projectRoot() + "/tdealien/";
        while (testSlashDir.endsWith("/") && testSlashDir.length() > 1) {
            testSlashDir.truncate(testSlashDir.length() - 1);
        }
        TQFileInfo fiDir(testSlashDir);
        TQString bName = fiDir.baseName(true);
        if (bName.isEmpty()) bName = fiDir.fileName();
        if (bName != "tdealien") {
            printf("TEST_VT_POST_STATE: FAIL (bName '%s' != 'tdealien')\n", bName.latin1());
            return 1;
        }

        printf("TEST_VT_POST_STATE: PASS\n");
        return 0;
    }

    if (testMenuIcons) {
        mw.setSkipPlymouthDialog(true);
        mw.show();
        for (int step = 0; step < 10; ++step) {
            app.processEvents();
            usleep(10000);
        }

        if (mw.fileMenu()) {
            mw.fileMenu()->popup(mw.mapToGlobal(TQPoint(10, 30)));
            for (int step = 0; step < 15; ++step) {
                app.processEvents();
                usleep(10000);
            }
            TQPixmap pmFile = TQPixmap::grabWidget(mw.fileMenu());
            pmFile.save("/home/cdef/.gemini/antigravity-ide/brain/71f461db-7163-4cc6-9812-5d10ba938dd3/menu_file.png", "PNG");
            mw.fileMenu()->hide();
        }

        if (mw.viewMenu()) {
            mw.viewMenu()->popup(mw.mapToGlobal(TQPoint(45, 30)));
            for (int step = 0; step < 15; ++step) {
                app.processEvents();
                usleep(10000);
            }
            TQPixmap pmView = TQPixmap::grabWidget(mw.viewMenu());
            pmView.save("/home/cdef/.gemini/antigravity-ide/brain/71f461db-7163-4cc6-9812-5d10ba938dd3/menu_view.png", "PNG");
            mw.viewMenu()->hide();
        }

        if (mw.toolsMenu()) {
            mw.toolsMenu()->popup(mw.mapToGlobal(TQPoint(70, 30)));
            for (int step = 0; step < 15; ++step) {
                app.processEvents();
                usleep(10000);
            }
            TQPixmap pmTools = TQPixmap::grabWidget(mw.toolsMenu());
            pmTools.save("/home/cdef/.gemini/antigravity-ide/brain/71f461db-7163-4cc6-9812-5d10ba938dd3/menu_tools.png", "PNG");
            mw.toolsMenu()->hide();
        }

        if (mw.helpMenu()) {
            mw.helpMenu()->popup(mw.mapToGlobal(TQPoint(110, 30)));
            for (int step = 0; step < 15; ++step) {
                app.processEvents();
                usleep(10000);
            }
            TQPixmap pmHelp = TQPixmap::grabWidget(mw.helpMenu());
            pmHelp.save("/home/cdef/.gemini/antigravity-ide/brain/71f461db-7163-4cc6-9812-5d10ba938dd3/menu_help.png", "PNG");
            mw.helpMenu()->hide();
        }

        // Test File Menu item enablement across display modes:
        // Mode 0: FramesDir enabled, SingleImage disabled
        mw.configPanel()->setDisplayMode(0);
        mw.updateFileMenuState();
        bool m0_frames = mw.fileMenu()->isItemEnabled(mw.actionOpenFramesDir());
        bool m0_image  = mw.fileMenu()->isItemEnabled(mw.actionOpenSingleImage());

        // Mode 1: FramesDir enabled, SingleImage enabled (bg image)
        mw.configPanel()->setDisplayMode(1);
        mw.updateFileMenuState();
        bool m1_frames = mw.fileMenu()->isItemEnabled(mw.actionOpenFramesDir());
        bool m1_image  = mw.fileMenu()->isItemEnabled(mw.actionOpenSingleImage());

        mw.fileMenu()->popup(mw.mapToGlobal(TQPoint(10, 30)));
        for (int step = 0; step < 10; ++step) { app.processEvents(); usleep(10000); }
        TQPixmap pmFileM1 = TQPixmap::grabWidget(mw.fileMenu());
        pmFileM1.save("/home/cdef/.gemini/antigravity-ide/brain/71f461db-7163-4cc6-9812-5d10ba938dd3/menu_file_mode1.png", "PNG");
        mw.fileMenu()->hide();

        // Mode 2: FramesDir enabled, SingleImage enabled (full screen bg image)
        mw.configPanel()->setDisplayMode(2);
        mw.updateFileMenuState();
        bool m2_frames = mw.fileMenu()->isItemEnabled(mw.actionOpenFramesDir());
        bool m2_image  = mw.fileMenu()->isItemEnabled(mw.actionOpenSingleImage());

        // Mode 3: FramesDir disabled, SingleImage enabled
        mw.configPanel()->setDisplayMode(3);
        mw.updateFileMenuState();
        bool m3_frames = mw.fileMenu()->isItemEnabled(mw.actionOpenFramesDir());
        bool m3_image  = mw.fileMenu()->isItemEnabled(mw.actionOpenSingleImage());

        mw.fileMenu()->popup(mw.mapToGlobal(TQPoint(10, 30)));
        for (int step = 0; step < 10; ++step) { app.processEvents(); usleep(10000); }
        TQPixmap pmFileM3 = TQPixmap::grabWidget(mw.fileMenu());
        pmFileM3.save("/home/cdef/.gemini/antigravity-ide/brain/71f461db-7163-4cc6-9812-5d10ba938dd3/menu_file_mode3.png", "PNG");
        mw.fileMenu()->hide();

        // Mode 4: FramesDir disabled, SingleImage enabled
        mw.configPanel()->setDisplayMode(4);
        mw.updateFileMenuState();
        bool m4_frames = mw.fileMenu()->isItemEnabled(mw.actionOpenFramesDir());
        bool m4_image  = mw.fileMenu()->isItemEnabled(mw.actionOpenSingleImage());

        if (!m0_frames || m0_image) {
            printf("TEST_MENU_ICONS: FAIL (Mode 0: frames=%d image=%d, expected frames=1 image=0)\n", m0_frames, m0_image);
            return 1;
        }
        if (!m1_frames || !m1_image) {
            printf("TEST_MENU_ICONS: FAIL (Mode 1: frames=%d image=%d, expected frames=1 image=1)\n", m1_frames, m1_image);
            return 1;
        }
        if (!m2_frames || !m2_image) {
            printf("TEST_MENU_ICONS: FAIL (Mode 2: frames=%d image=%d, expected frames=1 image=1)\n", m2_frames, m2_image);
            return 1;
        }
        if (m3_frames || !m3_image) {
            printf("TEST_MENU_ICONS: FAIL (Mode 3: frames=%d image=%d, expected frames=0 image=1)\n", m3_frames, m3_image);
            return 1;
        }
        if (m4_frames || !m4_image) {
            printf("TEST_MENU_ICONS: FAIL (Mode 4: frames=%d image=%d, expected frames=0 image=1)\n", m4_frames, m4_image);
            return 1;
        }

        printf("TEST_MENU_ICONS: PASS\n");
        return 0;
    }

    if (testBuildEngine) {
        printf("Running TEST_BUILD_ENGINE...\n");
        TQString projPath = mw.projectRoot() + "/projects/amiga.xbsp";
        if (!TQFile::exists(projPath)) {
            projPath = "/home/cdef/_PROJETS/bootsplash/projects/amiga.xbsp";
        }
        if (!TQFile::exists(projPath)) {
            printf("TEST_BUILD_ENGINE: FAIL (projects/amiga.xbsp not found)\n");
            return 1;
        }
        mw.loadProject(projPath);
        mw.configPanel()->setBinaryName("xbs_test_embedded");
        mw.configPanel()->setUseZx0(false);

        TQString dest = mw.projectRoot() + "/xbs_test_embedded";
        unlink(dest.latin1());

        mw.setSkipBuildDialog(true);
        mw.startBuild();

        time_t startTime = time(NULL);
        while (mw.buildEngine()->isBuilding() && (time(NULL) - startTime < 60)) {
            app.processEvents();
            usleep(10000);
        }

        if (mw.buildEngine()->isBuilding()) {
            printf("TEST_BUILD_ENGINE: FAIL (build timeout)\n");
            mw.buildEngine()->cancelBuild();
            return 1;
        }

        if (!TQFile::exists(dest) || TQFileInfo(dest).size() == 0) {
            printf("TEST_BUILD_ENGINE: FAIL (output binary %s not created)\n", dest.latin1());
            return 1;
        }

        printf("TEST_BUILD_ENGINE: PASS (binary created: %s, size: %lu bytes)\n",
               dest.latin1(), (unsigned long)TQFileInfo(dest).size());
        unlink(dest.latin1());
        return 0;
    }

    if (!testBusyText.isEmpty()) {
        mw.startBusyStatus(testBusyText);
        for (int step = 0; step < 26; ++step) {
            app.processEvents();
            usleep(80000);
        }
    }

    if (!screenshotPath.isEmpty()) {
        for (int step = 0; step < 25; ++step) {
            app.processEvents();
            usleep(15000);
        }
        TQPixmap pm = TQPixmap::grabWidget(&mw);
        pm.save(screenshotPath, "PNG");
        return 0;
    }

    return app.exec();
}
