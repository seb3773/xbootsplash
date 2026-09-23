/*
 * package_inspector_dialog.cpp - Dedicated package inspector, animated preview & action dialog
 */

#include "package_inspector_dialog.h"
#include "app_icons.h"
#include "installer_engine.h"
#include "password_dialog.h"
#include "log_banner.h"

#include <ntqlayout.h>
#include <ntqlabel.h>
#include <ntqpushbutton.h>
#include <ntqframe.h>
#include <ntqfileinfo.h>
#include <ntqfiledialog.h>
#include <ntqmessagebox.h>
#include <ntqmovie.h>
#include <ntqpixmap.h>
#include <ntqimage.h>
#include <ntqcolor.h>
#include <ntqfont.h>
#include <ntqpainter.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "vt_runner_helper.h"

/* --- PackagePreviewWidget implementation --- */

PackagePreviewWidget::PackagePreviewWidget(TQWidget *parent)
    : TQWidget(parent), m_movie(0)
{
    setFixedSize(320, 180);
    setBackgroundMode(TQt::NoBackground);
}

PackagePreviewWidget::~PackagePreviewWidget() {
    clear();
}

void PackagePreviewWidget::clear() {
    if (m_movie) {
        m_movie->disconnectUpdate(this);
        m_movie->disconnectResize(this);
        delete m_movie;
        m_movie = 0;
    }
    m_staticPixmap = TQPixmap();
    update();
}

void PackagePreviewWidget::setMovie(const TQString &path) {
    clear();
    m_movie = new TQMovie(path);
    if (!m_movie->isNull()) {
        m_movie->connectUpdate(this, SLOT(onMovieUpdated(const TQRect&)));
        m_movie->connectResize(this, SLOT(onMovieResized(const TQSize&)));
    }
    update();
}

void PackagePreviewWidget::setPixmap(const TQPixmap &pm) {
    clear();
    m_staticPixmap = pm;
    update();
}

bool PackagePreviewWidget::hasContent() const {
    if (m_movie && !m_movie->isNull()) return true;
    if (!m_staticPixmap.isNull()) return true;
    return false;
}

bool PackagePreviewWidget::isMovie() const {
    return (m_movie && !m_movie->isNull());
}

void PackagePreviewWidget::onMovieUpdated(const TQRect &) {
    update();
}

void PackagePreviewWidget::onMovieResized(const TQSize &) {
    update();
}

void PackagePreviewWidget::paintEvent(TQPaintEvent *) {
    TQPixmap buffer(size());
    TQPainter p(&buffer);
    p.fillRect(rect(), TQColor("#060608"));

    TQPixmap currentFrame;
    if (m_movie && !m_movie->isNull()) {
        currentFrame = m_movie->framePixmap();
    } else if (!m_staticPixmap.isNull()) {
        currentFrame = m_staticPixmap;
    }

    if (!currentFrame.isNull() && currentFrame.width() > 0 && currentFrame.height() > 0) {
        double sx = (double)width() / (double)currentFrame.width();
        double sy = (double)height() / (double)currentFrame.height();
        double scale = (sx < sy) ? sx : sy;
        int drawW = (int)(currentFrame.width() * scale);
        int drawH = (int)(currentFrame.height() * scale);
        int drawX = (width() - drawW) / 2;
        int drawY = (height() - drawH) / 2;

        if (drawW == currentFrame.width() && drawH == currentFrame.height()) {
            p.drawPixmap(drawX, drawY, currentFrame);
        } else {
            TQImage scaled = currentFrame.convertToImage().smoothScale(drawW, drawH);
            TQPixmap pm;
            pm.convertFromImage(scaled);
            p.drawPixmap(drawX, drawY, pm);
        }
    } else {
        p.setPen(TQColor("#64748b"));
        p.drawText(rect(), TQt::AlignCenter, "No preview animation\navailable in package");
    }

    // Clean subtle outer border
    p.setPen(TQPen(TQColor(40, 42, 54), 1));
    p.drawRect(0, 0, width(), height());
    p.end();

    // Single atomic blit to screen to avoid all flicker
    TQPainter wp(this);
    wp.drawPixmap(0, 0, buffer);
}

/* --- PackageInspectorDialog implementation --- */

PackageInspectorDialog::PackageInspectorDialog(const TQString &packagePath,
                                               SplashPackageManager *pkgMgr,
                                               SplashInstallerEngine *installer,
                                               const TQString &projectRoot,
                                               TQWidget *parent,
                                               const char *name)
    : TQDialog(parent, name, true),
      m_packagePath(packagePath),
      m_pkgMgr(pkgMgr),
      m_installer(installer),
      m_projectRoot(projectRoot),
      m_tmpDir(""),
      m_isLoaded(false),
      m_previewPath(""),
      m_binaryName(""),
      m_previewWidget(0),
      m_btnTestLive(0),
      m_btnInstall(0),
      m_btnExtract(0),
      m_btnExportDeb(0),
      m_btnClose(0)
{
    setCaption("XBootsplash Package Inspector");
    setAppWindowIcon(this);
    setMinimumSize(660, 440);

    TQString err;
    if (!m_pkgMgr || !m_pkgMgr->readPackageMetadata(m_packagePath, m_meta, m_previewPath, m_binaryName, err, &m_tmpDir)) {
        TQMessageBox::critical(parent, "Import Error",
                              TQString("Failed to inspect package:\n%1\n\n%2")
                              .arg(packagePath).arg(err.isEmpty() ? "Unknown error" : err));
        m_isLoaded = false;
        return;
    }

    m_isLoaded = true;
    setupUI();
}

PackageInspectorDialog::~PackageInspectorDialog() {
    cleanupTempDir();
}

void PackageInspectorDialog::cleanupTempDir() {
    if (!m_tmpDir.isEmpty() && TQFile::exists(m_tmpDir)) {
        system(TQString("rm -rf \"%1\"").arg(m_tmpDir).latin1());
        m_tmpDir = "";
    }
}

static TQString formatBytesWithSpaces(unsigned long bytes) {
    TQString s = TQString::number(bytes);
    for (int i = (int)s.length() - 3; i > 0; i -= 3) {
        s.insert(i, ' ');
    }
    return s;
}

void PackageInspectorDialog::setupUI() {
    setPaletteBackgroundColor(TQColor("#14141c"));
    resize(780, 720);
    setMinimumSize(750, 680);

    TQVBoxLayout *rootLay = new TQVBoxLayout(this, 0, 0);

    // 1. Header with package icon and title on a light background strip (230, 230, 230)
    TQFrame *headerBanner = new TQFrame(this);
    headerBanner->setFrameStyle(TQFrame::NoFrame);
    headerBanner->setPaletteBackgroundColor(TQColor(230, 230, 230));

    TQHBoxLayout *bannerLay = new TQHBoxLayout(headerBanner, 16, 12);
    bannerLay->setMargin(12);

    TQLabel *iconLbl = new TQLabel(headerBanner);
    iconLbl->setPixmap(iconPackage());
    iconLbl->setFixedSize(36, 36);
    iconLbl->setAlignment(TQt::AlignCenter);
    bannerLay->addWidget(iconLbl);

    TQString pkgFileName = TQFileInfo(m_packagePath).fileName();
    TQLabel *titleLbl = new TQLabel(
        TQString("<b><font size='+1' color='#111827'>Package Inspector</font></b><br>"
                 "<font color='#374151'>%1</font>").arg(pkgFileName),
        headerBanner);
    titleLbl->setAlignment(TQt::AlignVCenter | TQt::AlignLeft);
    bannerLay->addWidget(titleLbl, 1);

    rootLay->addWidget(headerBanner);

    // Subtle divider line below the light banner
    TQFrame *bannerSep = new TQFrame(this);
    bannerSep->setFrameStyle(TQFrame::HLine | TQFrame::Plain);
    bannerSep->setLineWidth(1);
    bannerSep->setPaletteForegroundColor(TQColor(205, 205, 205));
    rootLay->addWidget(bannerSep);

    // 2. Main content area on dark background
    TQVBoxLayout *mainLay = new TQVBoxLayout(rootLay, 14);
    mainLay->setMargin(16);

    // Center Content: Left = Preview card, Right = Specs card
    TQHBoxLayout *contentLay = new TQHBoxLayout(mainLay, 14);

    // Left Preview Card
    TQFrame *prevCard = new TQFrame(this);
    prevCard->setFrameStyle(TQFrame::StyledPanel | TQFrame::Plain);
    prevCard->setLineWidth(1);
    prevCard->setPaletteBackgroundColor(TQColor("#0f0f14"));
    prevCard->setFixedWidth(340);

    TQVBoxLayout *prevLay = new TQVBoxLayout(prevCard, 10, 8);
    prevLay->setAlignment(TQt::AlignCenter);

    m_previewWidget = new PackagePreviewWidget(prevCard);
    m_previewWidget->setFixedSize(320, 180);

    if (!m_previewPath.isEmpty() && TQFile::exists(m_previewPath)) {
        if (m_previewPath.endsWith(".gif")) {
            m_previewWidget->setMovie(m_previewPath);
        }
        if (!m_previewWidget->hasContent()) {
            TQPixmap px(m_previewPath);
            if (!px.isNull()) {
                m_previewWidget->setPixmap(px);
            }
        }
    }

    prevLay->addWidget(m_previewWidget);

    TQLabel *prevCaption = new TQLabel(
        m_previewWidget->isMovie() ? "<font color='#94a3b8'>Animated Preview (GIF)</font>" : "<font color='#94a3b8'>Static Preview (PNG)</font>",
        prevCard);
    prevCaption->setAlignment(TQt::AlignCenter);
    prevLay->addWidget(prevCaption);

    contentLay->addWidget(prevCard);

    // Right Specs Card
    TQFrame *specsCard = new TQFrame(this);
    specsCard->setFrameStyle(TQFrame::StyledPanel | TQFrame::Plain);
    specsCard->setLineWidth(1);
    specsCard->setPaletteBackgroundColor(TQColor("#181824"));

    TQVBoxLayout *specsLay = new TQVBoxLayout(specsCard, 14, 8);

    TQString titleHtml = TQString("<b><font size='+2' color='#38bdf8'>%1</font></b>").arg(m_meta.splashName);
    if (!m_meta.author.isEmpty()) {
        titleHtml += TQString("<br><font size='+0' color='#fbbf24'>by <b>%1</b></font>").arg(m_meta.author);
    }
    TQLabel *splashTitle = new TQLabel(titleHtml, specsCard);
    specsLay->addWidget(splashTitle);

    if (!m_meta.notes.isEmpty()) {
        TQLabel *notesLbl = new TQLabel(TQString("<font color='#cbd5e1'><i>&ldquo;%1&rdquo;</i></font>").arg(m_meta.notes), specsCard);
        notesLbl->setTextFormat(TQt::RichText);
        specsLay->addWidget(notesLbl);
    }

    // Backend text
    TQString backendDesc = "Framebuffer fbdev";
    if (m_meta.backend.lower() == "drm" || m_meta.backend.lower() == "drm/kms") {
        backendDesc = "<font color='#10b981'><b>DRM / KMS</b></font> <font color='#94a3b8'>(VSync, direct)</font>";
    } else {
        backendDesc = "<font color='#60a5fa'><b>Framebuffer (fbdev)</b></font> <font color='#94a3b8'>(Universal)</font>";
    }

    // Display mode text
    TQString modeDesc = "Animation on solid background";
    if (m_meta.displayMode == 1) modeDesc = "Animation on background image";
    else if (m_meta.displayMode == 2) modeDesc = "Static image on solid background";
    else if (m_meta.displayMode == 3) modeDesc = "Static image fullscreen";

    // Format animation summary
    TQString animSummary = "";
    if (m_meta.displayMode < 2) {
        animSummary = TQString("<b>%1</b> frames @ <b>%2</b> FPS (%3 ms/frame)")
                      .arg(m_meta.nframes).arg(m_meta.fps).arg(m_meta.frameDelay);
    } else {
        animSummary = "Single static frame";
    }

    TQFileInfo pkgFi(m_packagePath);
    unsigned long pkgSizeBytes = (unsigned long)pkgFi.size();
    unsigned long pkgSizeKb = (pkgSizeBytes + 1023) / 1024;

    unsigned long binSizeBytes = m_meta.binarySize;
    if (binSizeBytes == 0 && !m_binaryName.isEmpty()) {
        TQString binPath = m_tmpDir + "/" + m_binaryName;
        if (TQFile::exists(binPath)) {
            binSizeBytes = (unsigned long)TQFileInfo(binPath).size();
        }
    }
    unsigned long binSizeKb = (binSizeBytes > 0) ? ((binSizeBytes + 1023) / 1024) : 0;

    TQString binSizeStr;
    if (binSizeBytes > 0) {
        binSizeStr = TQString("<font color='#38bdf8'><b>%1 KB</b></font> <font color='#94a3b8'>(%2 bytes)</font>")
                     .arg(binSizeKb)
                     .arg(formatBytesWithSpaces(binSizeBytes));
    } else {
        binSizeStr = "<font color='#94a3b8'>Unknown</font>";
    }

    TQString pkgSizeStr = TQString("<font color='#cbd5e1'><b>%1 KB</b></font> <font color='#94a3b8'>(%2 bytes)</font>")
                          .arg(pkgSizeKb)
                          .arg(formatBytesWithSpaces(pkgSizeBytes));

    TQString compStr = m_meta.compression.isEmpty() ? "Unknown" : m_meta.compression;
    TQString compHtml;
    if (compStr.contains("UPKR")) {
        compHtml = TQString("<font color='#f43f5e'><b>%1</b></font>").arg(compStr);
    } else if (compStr.contains("ZX0")) {
        compHtml = TQString("<font color='#c084fc'><b>%1</b></font>").arg(compStr);
    } else if (compStr.contains("LZSS")) {
        compHtml = TQString("<font color='#38bdf8'><b>%1</b></font>").arg(compStr);
    } else {
        compHtml = TQString("<font color='#34d399'><b>%1</b></font>").arg(compStr);
    }

    TQString detailsHtml = "<table cellpadding='2' cellspacing='2'>" +
        TQString(
        "<tr><td align='right'><font color='#94a3b8'><b>Backend :</b></font></td><td><font color='#f1f5f9'>%1</font></td></tr>"
        "<tr><td align='right'><font color='#94a3b8'><b>Resolution :</b></font></td><td><font color='#f1f5f9'><b>%2</b></font></td></tr>"
        "<tr><td align='right'><font color='#94a3b8'><b>Mode :</b></font></td><td><font color='#f1f5f9'>%3</font></td></tr>"
        "<tr><td align='right'><font color='#94a3b8'><b>Compression :</b></font></td><td>%4</td></tr>"
        "<tr><td align='right'><font color='#94a3b8'><b>Animation :</b></font></td><td><font color='#f1f5f9'>%5</font></td></tr>"
        "<tr><td align='right'><font color='#94a3b8'><b>Bg Color :</b></font></td><td><font color='#f1f5f9'>#%6</font></td></tr>"
        "<tr><td align='right'><font color='#94a3b8'><b>Binary :</b></font></td><td><font color='#38bdf8'><b>%7</b></font></td></tr>"
        "<tr><td align='right'><font color='#94a3b8'><b>Binary size :</b></font></td><td>%8</td></tr>"
        "<tr><td align='right'><font color='#94a3b8'><b>Package size :</b></font></td><td>%9</td></tr>"
        "</table>")
        .arg(backendDesc)
        .arg(m_meta.resolution)
        .arg(modeDesc)
        .arg(compHtml)
        .arg(animSummary)
        .arg(m_meta.bgColor.isEmpty() ? "000000" : m_meta.bgColor)
        .arg(m_binaryName)
        .arg(binSizeStr)
        .arg(pkgSizeStr);

    TQLabel *detailsLbl = new TQLabel(detailsHtml, specsCard);
    specsLay->addWidget(detailsLbl);
    specsLay->addStretch(1);

    contentLay->addWidget(specsCard, 1);

    // 3. Question & Actions Bar
    mainLay->addSpacing(6);
    TQLabel *actionPrompt = new TQLabel("<font color='#f8fafc'><b>What would you like to do with this package?</b></font>", this);
    mainLay->addWidget(actionPrompt);

    TQHBoxLayout *btnLay = new TQHBoxLayout(mainLay, 8);

    m_btnTestLive = new TQPushButton(this);
    m_btnTestLive->setText("Test Live (VT)");
    m_btnTestLive->setIconSet(TQIconSet(iconLivePlay()));
    m_btnTestLive->setMinimumHeight(32);
    connect(m_btnTestLive, SIGNAL(clicked()), this, SLOT(onTestLive()));
    btnLay->addWidget(m_btnTestLive);

    m_btnInstall = new TQPushButton(this);
    m_btnInstall->setText("Install Bootsplash");
    m_btnInstall->setIconSet(TQIconSet(iconInstall()));
    m_btnInstall->setPaletteBackgroundColor(TQColor("#0f766e")); // Teal/Green accent
    m_btnInstall->setPaletteForegroundColor(TQColor("#ffffff"));
    m_btnInstall->setMinimumHeight(32);
    connect(m_btnInstall, SIGNAL(clicked()), this, SLOT(onInstallPackage()));
    btnLay->addWidget(m_btnInstall);

    m_btnExtract = new TQPushButton(this);
    m_btnExtract->setText("Extract Files...");
    m_btnExtract->setIconSet(TQIconSet(iconPackage()));
    m_btnExtract->setMinimumHeight(32);
    connect(m_btnExtract, SIGNAL(clicked()), this, SLOT(onExtractPackage()));
    btnLay->addWidget(m_btnExtract);

    m_btnExportDeb = new TQPushButton(this);
    m_btnExportDeb->setText("Export as .deb...");
    m_btnExportDeb->setIconSet(TQIconSet(iconDeb()));
    m_btnExportDeb->setMinimumHeight(32);
    connect(m_btnExportDeb, SIGNAL(clicked()), this, SLOT(onExportDeb()));
    btnLay->addWidget(m_btnExportDeb);

    btnLay->addStretch(1);

    m_btnClose = new TQPushButton("Close", this);
    m_btnClose->setMinimumHeight(32);
    connect(m_btnClose, SIGNAL(clicked()), this, SLOT(reject()));
    btnLay->addWidget(m_btnClose);
}

void PackageInspectorDialog::onTestLive() {
    if (m_tmpDir.isEmpty() || !TQFile::exists(m_tmpDir)) {
        TQMessageBox::critical(this, "Error", "Package temporary inspection directory is not available.");
        return;
    }

    TQString binPath = m_tmpDir + "/" + m_binaryName;
    if (!TQFile::exists(binPath)) {
        TQDir d(m_tmpDir);
        TQStringList subdirs = d.entryList(TQDir::Dirs);
        for (TQStringList::Iterator sit = subdirs.begin(); sit != subdirs.end(); ++sit) {
            if (*sit == "." || *sit == "..") continue;
            TQString candidate = m_tmpDir + "/" + (*sit) + "/" + m_binaryName;
            if (TQFile::exists(candidate)) {
                binPath = candidate;
                break;
            }
        }
    }

    if (!TQFile::exists(binPath)) {
        TQMessageBox::critical(this, "Binary Not Found",
                               TQString("Could not locate bootsplash binary '%1' in the package.")
                               .arg(m_binaryName));
        return;
    }

    // Ensure target binary is executable
    chmod(binPath.latin1(), 0755);

    TQString logOutput;
    TQString dispName = m_meta.splashName + " (" + m_meta.backend + ")";
    executeVtLiveTest(this, m_projectRoot, binPath, dispName, logOutput);
}

void PackageInspectorDialog::onInstallPackage() {
    if (!m_installer) return;

    if (m_installer->isRunning()) {
        TQMessageBox::information(this, "Operation in Progress",
                                 "An initramfs operation is already in progress.");
        return;
    }

    // Extract cleanly to packages/extracted_<splashName>
    TQString destDir = m_projectRoot + "/packages/extracted_" + m_meta.splashName;
    TQString outBin, err;
    if (!m_pkgMgr->extractPackage(m_packagePath, destDir, outBin, err)) {
        emit statusMessage(SplashLog::banner("EXTRACT"));
        emit statusMessage(TQString("[ERROR] Failed to extract package before installation: %1").arg(err));
        emit statusMessage(SplashLog::failedBanner());
        TQMessageBox::critical(this, "Extraction Error",
                              TQString("Failed to extract package before installation:\n%1").arg(err));
        return;
    }

    // Ask install target
    int choice = TQMessageBox::information(this, "Install Bootsplash Package",
        TQString("Package  : %1\nBinary   : %2\nBackend  : %3\n\nChoose install target:")
        .arg(TQFileInfo(m_packagePath).fileName())
        .arg(outBin)
        .arg(m_meta.backend),
        "Boot & Shutdown (Recommended)",
        "Boot Only",
        "Shutdown Only", 0, 3);

    if (choice == 3) return; // Cancelled

    SplashInstallerEngine::InstallTarget target = SplashInstallerEngine::TargetBoth;
    if (choice == 1) target = SplashInstallerEngine::TargetBoot;
    else if (choice == 2) target = SplashInstallerEngine::TargetShutdown;

    TQString password = "";
    if (getuid() != 0) {
        if (!SplashPasswordDialog::getPassword(this, password)) {
            emit statusMessage(SplashLog::banner("INSTALL"));
            emit statusMessage("Installation cancelled: Administrator password not provided.");
            emit statusMessage(SplashLog::failedBanner());
            return;
        }
    }

    bool isDrm = (m_meta.backend.lower() == "drm" || m_meta.backend.lower() == "drm/kms");
    m_installer->startInstall(outBin, target, isDrm, password);
    accept();
}

void PackageInspectorDialog::onExtractPackage() {
    TQString defaultDest = m_projectRoot + "/packages/extracted_" + m_meta.splashName;
    TQString dest = TQFileDialog::getExistingDirectory(defaultDest, this, "select_dest", "Select Destination Directory");
    if (dest.isEmpty()) return;

    emit statusMessage(SplashLog::banner("EXTRACT"));
    emit statusMessage(TQString("Package     : %1").arg(TQFileInfo(m_packagePath).fileName()));
    emit statusMessage(TQString("Destination : %1").arg(dest));

    TQString outBin, err;
    if (m_pkgMgr->extractPackage(m_packagePath, dest, outBin, err)) {
        TQMessageBox::information(this, "Package Extracted",
            TQString("Package was successfully extracted to:\n%1\n\nExtracted binary: %2")
            .arg(dest).arg(outBin));
        emit statusMessage(TQString("Extracted binary : %1").arg(outBin));
        emit statusMessage(SplashLog::doneBanner());
    } else {
        TQMessageBox::critical(this, "Extraction Error", err);
        emit statusMessage(TQString("[ERROR] Extraction failed: %1").arg(err));
        emit statusMessage(SplashLog::failedBanner());
    }
}

void PackageInspectorDialog::onExportDeb() {
    if (!m_pkgMgr || !TQFile::exists(m_packagePath)) {
        TQMessageBox::critical(this, "Error", "Package file is not available.");
        return;
    }

    TQString baseName = m_meta.splashName.isEmpty() ? "splash" : m_meta.splashName;
    if (baseName.startsWith("xbs_")) baseName = baseName.mid(4);

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

    TQString suggestedFile = m_projectRoot + "/packages/xbootsplash-theme-" + baseName.lower() + "_1.0_" + arch + ".deb";

    TQString savePath = TQFileDialog::getSaveFileName(suggestedFile,
                                                     "Debian Package (*.deb)",
                                                     this, "save_deb_from_pkg",
                                                     "Export as Debian Package");
    if (savePath.isEmpty()) return;

    TQString outDeb, err;
    bool ok = m_pkgMgr->convertPackageToDeb(m_packagePath, savePath, (m_meta.displayMode != 1), outDeb, err);

    if (ok) {
        TQFileInfo fiDeb(outDeb);
        unsigned long szKb = (fiDeb.size() + 1023) / 1024;
        emit statusMessage(TQString("✔ Debian package exported: %1 (%2 KB)").arg(outDeb).arg(szKb));

        TQMessageBox::information(
            this, "Debian Package Created",
            TQString("<p><b>Debian Package successfully exported!</b></p>"
                    "<p><b>File:</b> %1<br><b>Size:</b> %2 KB (%3 bytes)</p>"
                    "<p>To install this package on any Debian / Q4OS / Ubuntu system, run:<br>"
                    "<tt><b>sudo apt install \"%4\"</b></tt></p>"
                    "<p>To remove it later:<br>"
                    "<tt><b>sudo apt remove xbootsplash-theme-%5</b></tt></p>")
            .arg(fiDeb.fileName())
            .arg(szKb)
            .arg(fiDeb.size())
            .arg(outDeb)
            .arg(baseName.lower()));
    } else {
        emit statusMessage(TQString("[ERROR] Debian export failed: %1").arg(err));
        TQMessageBox::critical(this, "Debian Export Error",
                              TQString("Failed to create Debian package:\n\n%1").arg(err));
    }
}

#include "package_inspector_dialog.moc"
