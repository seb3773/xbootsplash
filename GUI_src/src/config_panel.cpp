#include "config_panel.h"
#include "preview_widget.h"
#include "app_icons.h"
#include "../libs/collapsible-group/tqtcollapsiblegroup.h"
#include "../libs/toggle-buttons/tqttoggleswitch.h"

#include <ntqlayout.h>
#include <ntqcolordialog.h>
#include <ntqfiledialog.h>
#include <ntqgroupbox.h>
#include <ntqapplication.h>
#include <ntqfileinfo.h>
#include <ntqdir.h>
#include <ntqtextstream.h>
#include <ntqmap.h>
#include <ntqtooltip.h>
#include <ntqmessagebox.h>
#include <stdio.h>

SplashConfigPanel::SplashConfigPanel(SplashPreviewWidget *preview, TQWidget *parent, const char *name)
    : TQScrollView(parent, name),
      m_preview(preview),
      m_framesLabel(0),
      m_grpPos(0),
      m_grpTime(0),
      m_savedAnimPath(""),
      m_savedBgImagePath(""),
      m_savedStaticImagePath(""),
      m_previousMode(0),
      m_animOffsetX(0),
      m_animOffsetY(80),
      m_bgOffsetX(0),
      m_bgOffsetY(0)
{
    setResizePolicy(TQScrollView::AutoOneFit);
    setupUI();

    // Connect preview signals to configuration panel
    connect(m_preview, SIGNAL(offsetChanged(int, int)), this, SLOT(onPreviewOffsetChanged(int, int)));
    connect(m_preview, SIGNAL(bgOffsetChanged(int, int)), this, SLOT(onPreviewBgOffsetChanged(int, int)));
    connect(m_preview, SIGNAL(selectedElementChanged(int)), this, SLOT(onPreviewSelectedElementChanged(int)));
    connect(m_preview, SIGNAL(framesLoaded(int, int, int)), this, SLOT(onPreviewFramesLoaded(int, int, int)));
    connect(m_preview, SIGNAL(intruderFilesDetected(const TQStringList&)), this, SLOT(onIntruderFilesDetected(const TQStringList&)));
}

SplashConfigPanel::~SplashConfigPanel() {
}

void SplashConfigPanel::setupUI() {
    TQWidget *container = new TQWidget(viewport());
    TQVBoxLayout *layout = new TQVBoxLayout(container, 8, 8);

    // ==========================================
    // 1. SECTION: SOURCE & DISPLAY MODE
    // ==========================================
    TQtCollapsibleGroup *grpSource = new TQtCollapsibleGroup("1. Source && Display Mode", container);
    TQVBoxLayout *sourceLayout = new TQVBoxLayout(grpSource->contentWidget(), 6, 6);

    // Mode
    sourceLayout->addWidget(new TQLabel("Display Mode:", grpSource->contentWidget()));
    m_modeCombo = new TQComboBox(false, grpSource->contentWidget());
    m_modeCombo->insertItem("0: Animation on solid background (default)");
    m_modeCombo->insertItem("1: Animation on background image on uniform color");
    m_modeCombo->insertItem("2: Animation on full screen image");
    m_modeCombo->insertItem("3: Single static image on uniform color");
    m_modeCombo->insertItem("4: Single static image full screen");
    connect(m_modeCombo, SIGNAL(activated(int)), this, SLOT(onModeChanged(int)));
    sourceLayout->addWidget(m_modeCombo);

    // Frames Directory / Image
    m_framesLabel = new TQLabel("Frames Directory:", grpSource->contentWidget());
    sourceLayout->addWidget(m_framesLabel);
    TQHBoxLayout *framesRow = new TQHBoxLayout(0, 0, 4);
    m_framesEdit = new TQLineEdit(grpSource->contentWidget());
    m_browseFramesBtn = new TQPushButton("Browse...", grpSource->contentWidget());
    framesRow->addWidget(m_framesEdit);
    framesRow->addWidget(m_browseFramesBtn);
    sourceLayout->addLayout(framesRow);
    connect(m_browseFramesBtn, SIGNAL(clicked()), this, SLOT(onBrowseFrames()));
    connect(m_framesEdit, SIGNAL(textChanged(const TQString &)), this, SIGNAL(configurationChanged()));

    m_framesInfoLabel = new TQLabel("No frames loaded", grpSource->contentWidget());
    m_framesInfoLabel->setPaletteForegroundColor(TQColor(128, 128, 128));
    sourceLayout->addWidget(m_framesInfoLabel);

    // Background Image (Modes 1 and 2)
    m_bgImageRow = new TQWidget(grpSource->contentWidget());
    TQVBoxLayout *bgImgLayout = new TQVBoxLayout(m_bgImageRow, 0, 4);
    bgImgLayout->addWidget(new TQLabel("Background Image:", m_bgImageRow));
    TQHBoxLayout *bgFileRow = new TQHBoxLayout(0, 0, 4);
    m_bgImageEdit = new TQLineEdit(m_bgImageRow);
    m_browseBgBtn = new TQPushButton("Browse...", m_bgImageRow);
    bgFileRow->addWidget(m_bgImageEdit);
    bgFileRow->addWidget(m_browseBgBtn);
    bgImgLayout->addLayout(bgFileRow);
    connect(m_browseBgBtn, SIGNAL(clicked()), this, SLOT(onBrowseBgImage()));
    connect(m_bgImageEdit, SIGNAL(textChanged(const TQString &)), this, SIGNAL(configurationChanged()));
    m_bgImageRow->hide();
    sourceLayout->addWidget(m_bgImageRow);

    layout->addWidget(grpSource);

    // ==========================================
    // 2. SECTION: POSITION & ALIGNMENT
    // ==========================================
    m_grpPos = new TQtCollapsibleGroup("2. Position && Alignment", container);
    TQVBoxLayout *posLayout = new TQVBoxLayout(m_grpPos->contentWidget(), 6, 6);

    // Active Element Selector for Mode 1
    m_elementSelectorRow = new TQWidget(m_grpPos->contentWidget());
    TQVBoxLayout *elemSelLayout = new TQVBoxLayout(m_elementSelectorRow, 0, 4);
    elemSelLayout->addWidget(new TQLabel("Active Element:", m_elementSelectorRow));
    TQHBoxLayout *elemBtnRow = new TQHBoxLayout(0, 0, 4);
    m_btnSelectAnim = new TQPushButton("Animation", m_elementSelectorRow);
    m_btnSelectBg = new TQPushButton("Static Image", m_elementSelectorRow);
    m_btnSelectAnim->setToggleButton(true);
    m_btnSelectBg->setToggleButton(true);
    m_btnSelectAnim->setOn(true);
    m_btnSelectBg->setOn(false);
    elemBtnRow->addWidget(m_btnSelectAnim);
    elemBtnRow->addWidget(m_btnSelectBg);
    elemSelLayout->addLayout(elemBtnRow);
    connect(m_btnSelectAnim, SIGNAL(clicked()), this, SLOT(onSelectAnimClicked()));
    connect(m_btnSelectBg, SIGNAL(clicked()), this, SLOT(onSelectBgClicked()));
    m_elementSelectorRow->hide();
    posLayout->addWidget(m_elementSelectorRow);

    // Animation / Static Image Position
    m_animPosLabel = new TQLabel("Animation Position (X / Y pixels):", m_grpPos->contentWidget());
    posLayout->addWidget(m_animPosLabel);

    // Offset X
    posLayout->addWidget(new TQLabel("Horizontal Offset X:", m_grpPos->contentWidget()));
    TQHBoxLayout *rowX = new TQHBoxLayout(0, 0, 6);
    m_spinOffsetX = new TQSpinBox(-1920, 1920, 1, m_grpPos->contentWidget());
    m_spinOffsetX->setValue(0);
    m_sliderOffsetX = new TQSlider(-1000, 1000, 1, 0, TQt::Horizontal, m_grpPos->contentWidget());
    rowX->addWidget(m_spinOffsetX);
    rowX->addWidget(m_sliderOffsetX);
    posLayout->addLayout(rowX);
    connect(m_spinOffsetX, SIGNAL(valueChanged(int)), this, SLOT(onOffsetXChanged(int)));
    connect(m_sliderOffsetX, SIGNAL(valueChanged(int)), this, SLOT(onOffsetXSlider(int)));

    // Offset Y
    posLayout->addWidget(new TQLabel("Vertical Offset Y:", m_grpPos->contentWidget()));
    TQHBoxLayout *rowY = new TQHBoxLayout(0, 0, 6);
    m_spinOffsetY = new TQSpinBox(-1080, 1080, 1, m_grpPos->contentWidget());
    m_spinOffsetY->setValue(80);
    m_sliderOffsetY = new TQSlider(-1000, 1000, 1, 80, TQt::Horizontal, m_grpPos->contentWidget());
    rowY->addWidget(m_spinOffsetY);
    rowY->addWidget(m_sliderOffsetY);
    posLayout->addLayout(rowY);
    connect(m_spinOffsetY, SIGNAL(valueChanged(int)), this, SLOT(onOffsetYChanged(int)));
    connect(m_sliderOffsetY, SIGNAL(valueChanged(int)), this, SLOT(onOffsetYSlider(int)));

    // Quick Center button for Animation / Static Image
    m_resetOffsetBtn = new TQPushButton("Center Animation (0, 0)", m_grpPos->contentWidget());
    connect(m_resetOffsetBtn, SIGNAL(clicked()), this, SLOT(onResetOffsets()));
    posLayout->addWidget(m_resetOffsetBtn);

    // Target Resolution
    posLayout->addWidget(new TQLabel("Target Resolution (preview / fullscreen):", m_grpPos->contentWidget()));
    m_resCombo = new TQComboBox(false, m_grpPos->contentWidget());
    m_resCombo->insertItem("1920x1080 (16:9 Full HD)");
    m_resCombo->insertItem("1920x1200 (16:10 WUXGA)");
    m_resCombo->insertItem("2560x1440 (16:9 2K QHD)");
    m_resCombo->insertItem("3840x2160 (16:9 4K UHD)");
    m_resCombo->insertItem("1366x768 (16:9 Laptop)");
    m_resCombo->insertItem("1280x720 (16:9 HD)");
    m_resCombo->insertItem("1024x768 (4:3 XGA)");
    connect(m_resCombo, SIGNAL(activated(int)), this, SLOT(onResolutionChanged(int)));
    posLayout->addWidget(m_resCombo);

    m_grpPos->setExpanded(false);
    layout->addWidget(m_grpPos);

    // ==========================================
    // 3. SECTION: ANIMATION & TIMING
    // ==========================================
    m_grpTime = new TQtCollapsibleGroup("3. Speed && Playback Loop", container);
    TQVBoxLayout *timeLayout = new TQVBoxLayout(m_grpTime->contentWidget(), 6, 6);

    // Delay
    timeLayout->addWidget(new TQLabel("Frame Delay (ms):", m_grpTime->contentWidget()));
    TQHBoxLayout *rowDelay = new TQHBoxLayout(0, 0, 6);
    m_spinDelay = new TQSpinBox(1, 1000, 1, m_grpTime->contentWidget());
    m_spinDelay->setValue(33);
    m_sliderDelay = new TQSlider(5, 200, 1, 33, TQt::Horizontal, m_grpTime->contentWidget());
    m_fpsLabel = new TQLabel("30.3 FPS", m_grpTime->contentWidget());
    m_fpsLabel->setFixedWidth(60);
    rowDelay->addWidget(m_spinDelay);
    rowDelay->addWidget(m_sliderDelay);
    rowDelay->addWidget(m_fpsLabel);
    timeLayout->addLayout(rowDelay);
    connect(m_spinDelay, SIGNAL(valueChanged(int)), this, SLOT(onDelayChanged(int)));
    connect(m_sliderDelay, SIGNAL(valueChanged(int)), this, SLOT(onDelaySlider(int)));

    // Loop mode
    timeLayout->addWidget(new TQLabel("Loop Behavior:", m_grpTime->contentWidget()));
    m_loopCombo = new TQComboBox(false, m_grpTime->contentWidget());
    m_loopCombo->insertItem("Infinite Loop (Full cycle)");
    m_loopCombo->insertItem("No Loop (Stop at final frame)");
    m_loopCombo->insertItem("Partial Loop (Loop from start frame)");
    m_loopCombo->insertItem("Ping-Pong (Forward ↔ Backward)");
    connect(m_loopCombo, SIGNAL(activated(int)), this, SLOT(onLoopModeChanged(int)));
    timeLayout->addWidget(m_loopCombo);

    // Partial loop start
    m_loopStartRow = new TQWidget(m_grpTime->contentWidget());
    TQHBoxLayout *loopStartLayout = new TQHBoxLayout(m_loopStartRow, 0, 4);
    loopStartLayout->addWidget(new TQLabel("Loop Restart Frame:", m_loopStartRow));
    m_spinLoopStart = new TQSpinBox(0, 9999, 1, m_loopStartRow);
    m_spinLoopStart->setValue(0);
    connect(m_spinLoopStart, SIGNAL(valueChanged(int)), this, SLOT(onLoopStartChanged(int)));
    loopStartLayout->addWidget(m_spinLoopStart);
    m_loopStartRow->hide();
    timeLayout->addWidget(m_loopStartRow);

    // Minimum complete loops at boot
    TQHBoxLayout *rowMinLoops = new TQHBoxLayout(0, 0, 6);
    rowMinLoops->addWidget(new TQLabel("Min Complete Loops at Boot:", m_grpTime->contentWidget()));
    m_spinMinLoops = new TQSpinBox(0, 10, 1, m_grpTime->contentWidget());
    m_spinMinLoops->setValue(0);
    m_spinMinLoops->setSpecialValueText("0 (Instant exit)");
    m_spinMinLoops->setSuffix(" cycle(s)");
    connect(m_spinMinLoops, SIGNAL(valueChanged(int)), this, SLOT(onMinLoopsChanged(int)));
    rowMinLoops->addWidget(m_spinMinLoops);
    rowMinLoops->addStretch();
    timeLayout->addLayout(rowMinLoops);

    // Invert frames order
    TQHBoxLayout *rowInvert = new TQHBoxLayout(0, 0, 6);
    rowInvert->addWidget(new TQLabel("Reverse Playback (N down to 0):", m_grpTime->contentWidget()));
    m_invertSwitch = new TQtToggleSwitch(TQtToggleSwitch::BorderSolidSoftRectNoText, m_grpTime->contentWidget());
    m_invertSwitch->setSuitableHeight(22);
    connect(m_invertSwitch, SIGNAL(stateChanged(bool)), this, SLOT(onInvertToggled(bool)));
    rowInvert->addWidget(m_invertSwitch);
    rowInvert->addStretch();
    timeLayout->addLayout(rowInvert);

    m_grpTime->setExpanded(false);
    layout->addWidget(m_grpTime);

    // ==========================================
    // 4. SECTION: COLOR & BACKGROUND
    // ==========================================
    TQtCollapsibleGroup *grpColor = new TQtCollapsibleGroup("4. Background Color", container);
    TQVBoxLayout *colorLayout = new TQVBoxLayout(grpColor->contentWidget(), 6, 6);

    colorLayout->addWidget(new TQLabel("Background Color:", grpColor->contentWidget()));
    TQHBoxLayout *colorRow = new TQHBoxLayout(0, 0, 6);
    m_colorBtn = new TQPushButton("Choose...", grpColor->contentWidget());
    m_colorBtn->setFixedWidth(75);
    m_hexEdit = new TQLineEdit("#000000", grpColor->contentWidget());
    m_hexEdit->setMaxLength(7);
    m_hexEdit->setFixedWidth(70);

    m_pipetteBtn = new TQPushButton(grpColor->contentWidget());
    m_pipetteBtn->setText(" Pick from Canvas");
    m_pipetteBtn->setIconSet(TQIconSet(iconPipette()));
    m_pipetteBtn->setToggleButton(true);
    TQToolTip::add(m_pipetteBtn, "Pick a color directly from the preview canvas (Eyedropper / Pipette)");

    colorRow->addWidget(m_colorBtn);
    colorRow->addWidget(m_hexEdit);
    colorRow->addWidget(m_pipetteBtn);
    colorLayout->addLayout(colorRow);

    connect(m_colorBtn, SIGNAL(clicked()), this, SLOT(onPickColor()));
    connect(m_hexEdit, SIGNAL(textChanged(const TQString&)), this, SLOT(onHexColorEdited(const TQString&)));
    connect(m_pipetteBtn, SIGNAL(toggled(bool)), this, SLOT(onPipetteToggled(bool)));
    connect(m_preview, SIGNAL(colorPicked(const TQColor&)), this, SLOT(onColorPickedFromPreview(const TQColor&)));
    connect(m_preview, SIGNAL(colorPickingCancelled()), this, SLOT(onColorPickingCancelled()));
    updateColorButton();

    m_grpColor = grpColor;
    grpColor->setExpanded(false);
    layout->addWidget(grpColor);

    // ==========================================
    // 5. SECTION: TARGET & BUILD
    // ==========================================
    m_grpBuild = new TQtCollapsibleGroup("5. Binary && Build Settings", container);
    TQVBoxLayout *buildLayout = new TQVBoxLayout(m_grpBuild->contentWidget(), 6, 6);

    buildLayout->addWidget(new TQLabel("Output Binary Name:", m_grpBuild->contentWidget()));
    m_binaryEdit = new TQLineEdit("xbootsplash", m_grpBuild->contentWidget());
    buildLayout->addWidget(m_binaryEdit);
    connect(m_binaryEdit, SIGNAL(textChanged(const TQString &)), this, SIGNAL(configurationChanged()));

    buildLayout->addWidget(new TQLabel("Graphics Backend:", m_grpBuild->contentWidget()));
    m_backendCombo = new TQComboBox(false, m_grpBuild->contentWidget());
    m_backendCombo->insertItem("Framebuffer (/dev/fb0) - Recommended (Boot + Shutdown, static)");
    m_backendCombo->insertItem("DRM/KMS - Specialized for Boot (tear-free VSync, dynamic)");
    buildLayout->addWidget(m_backendCombo);
    connect(m_backendCombo, SIGNAL(activated(int)), this, SLOT(onBackendChanged(int)));

    TQLabel *backendHint = new TQLabel(
        "<font color=\"#888888\" size=\"-1\"><i><b>Framebuffer:</b> Recommended universal mode (Boot & Shutdown, static, 0 deps).<br>"
        "<b>DRM/KMS:</b> Specialized for early boot (modern displays, tear-free VSync).</i></font>", m_grpBuild->contentWidget());
    backendHint->setTextFormat(TQt::RichText);
    buildLayout->addWidget(backendHint);

    buildLayout->addWidget(new TQLabel("Author (optional):", m_grpBuild->contentWidget()));
    m_authorEdit = new TQLineEdit(m_grpBuild->contentWidget());
    buildLayout->addWidget(m_authorEdit);
    connect(m_authorEdit, SIGNAL(textChanged(const TQString &)), this, SIGNAL(configurationChanged()));

    buildLayout->addWidget(new TQLabel("Description / Notes (optional):", m_grpBuild->contentWidget()));
    m_notesEdit = new TQLineEdit(m_grpBuild->contentWidget());
    buildLayout->addWidget(m_notesEdit);
    connect(m_notesEdit, SIGNAL(textChanged(const TQString &)), this, SIGNAL(configurationChanged()));

    buildLayout->addWidget(new TQLabel("Compression Algorithm:", m_grpBuild->contentWidget()));
    m_compressCombo = new TQComboBox(false, m_grpBuild->contentWidget());
    m_compressCombo->insertItem("Auto (Select best compression)");
    m_compressCombo->insertItem("RLE XOR");
    m_compressCombo->insertItem("RLE Direct");
    m_compressCombo->insertItem("Sparse XOR");
    m_compressCombo->insertItem("Raw XOR (No compression)");
    buildLayout->addWidget(m_compressCombo);

    // Super-compression toggle switch & algorithm selector
    TQHBoxLayout *rowSuper = new TQHBoxLayout(0, 0, 6);
    m_superCompressLabel = new TQLabel("Enable Super-Compression:", m_grpBuild->contentWidget());
    m_superCompressLabel->setCursor(TQt::pointingHandCursor);
    m_superCompressLabel->installEventFilter(this);
    m_superCompressSwitch = new TQtToggleSwitch(TQtToggleSwitch::BorderSolidSoftRectNoText, m_grpBuild->contentWidget());
    m_superCompressSwitch->setSuitableHeight(22);
    m_superCompressSwitch->setState(false);
    m_zx0Switch = m_superCompressSwitch; // backward compatibility pointer

    rowSuper->addWidget(m_superCompressLabel);
    rowSuper->addWidget(m_superCompressSwitch);
    rowSuper->addStretch();
    buildLayout->addLayout(rowSuper);

    // Algorithm selector row
    TQHBoxLayout *rowAlgo = new TQHBoxLayout(0, 0, 6);
    TQLabel *algoLabel = new TQLabel("Algorithm:", m_grpBuild->contentWidget());
    rowAlgo->addWidget(algoLabel);

    m_superCompressCombo = new TQComboBox(false, m_grpBuild->contentWidget());
    m_superCompressCombo->insertItem(TQString::fromUtf8("UPKR (Best ratio, fast build) — Recommended"));
    m_superCompressCombo->insertItem("ZX0 (Ultra-fast unpack, very slow build)");
    m_superCompressCombo->setCurrentItem(0); // UPKR default!
    m_superCompressCombo->setEnabled(false);
    rowAlgo->addWidget(m_superCompressCombo, 1);
    buildLayout->addLayout(rowAlgo);

    // Permanent note directly below selector
    m_superCompressHint = new TQLabel(
        "<font color=\"#888888\" size=\"-1\"><i>"
        "&bull; <b>UPKR</b> (Recommended): Absolute smallest binary (~15&ndash;25% smaller than ZX0), fast build.<br>"
        "&bull; <b>ZX0</b>: Battle-tested standard, ultra-fast microsecond unpack, but very slow build."
        "</i></font>", m_grpBuild->contentWidget());
    m_superCompressHint->setTextFormat(TQt::RichText);
    buildLayout->addWidget(m_superCompressHint);

    connect(m_superCompressSwitch, SIGNAL(stateChanged(bool)), this, SLOT(onSuperCompressToggled(bool)));
    connect(m_superCompressCombo, SIGNAL(activated(int)), this, SIGNAL(configurationChanged()));

    m_grpBuild->setExpanded(false);
    layout->addWidget(m_grpBuild);

    layout->addStretch();
    addChild(container);
}

int SplashConfigPanel::displayMode() const {
    return m_modeCombo->currentItem();
}

TQString SplashConfigPanel::framesPath() const {
    return m_framesEdit->text().stripWhiteSpace();
}

TQString SplashConfigPanel::bgImagePath() const {
    return m_bgImageEdit->text().stripWhiteSpace();
}

int SplashConfigPanel::offsetX() const {
    return m_animOffsetX;
}

int SplashConfigPanel::offsetY() const {
    return m_animOffsetY;
}

int SplashConfigPanel::bgOffsetX() const {
    return m_bgOffsetX;
}

int SplashConfigPanel::bgOffsetY() const {
    return m_bgOffsetY;
}

int SplashConfigPanel::frameDelay() const {
    return m_spinDelay->value();
}

int SplashConfigPanel::loopMode() const {
    int idx = m_loopCombo->currentItem();
    if (idx == 0) return 1; // Full loop
    if (idx == 1) return 0; // No loop
    if (idx == 2) return 2; // Partial loop
    if (idx == 3) return 3; // Ping-Pong
    return 1;
}

int SplashConfigPanel::loopStart() const {
    return m_spinLoopStart->value();
}

int SplashConfigPanel::minBootLoops() const {
    return m_spinMinLoops ? m_spinMinLoops->value() : 0;
}

bool SplashConfigPanel::invertFrames() const {
    return m_invertSwitch->isChecked();
}

TQColor SplashConfigPanel::bgColor() const {
    return m_preview->bgColor();
}

TQString SplashConfigPanel::targetResolution() const {
    int idx = m_resCombo->currentItem();
    switch (idx) {
        case 0: return "1920x1080";
        case 1: return "1920x1200";
        case 2: return "2560x1440";
        case 3: return "3840x2160";
        case 4: return "1366x768";
        case 5: return "1280x720";
        case 6: return "1024x768";
        default: return "1920x1080";
    }
}

TQString SplashConfigPanel::binaryName() const {
    TQString name = m_binaryEdit->text().stripWhiteSpace();
    return name.isEmpty() ? "xbootsplash" : name;
}

TQString SplashConfigPanel::author() const {
    return m_authorEdit ? m_authorEdit->text().stripWhiteSpace() : TQString("");
}

TQString SplashConfigPanel::notes() const {
    return m_notesEdit ? m_notesEdit->text().stripWhiteSpace() : TQString("");
}

bool SplashConfigPanel::useDrm() const {
    return m_backendCombo->currentItem() == 1;
}

void SplashConfigPanel::setUseDrm(bool drm, bool triggerNotice) {
    int idx = drm ? 1 : 0;
    if (m_backendCombo->currentItem() != idx) {
        m_backendCombo->setCurrentItem(idx);
        if (triggerNotice) {
            onBackendChanged(idx);
        } else {
            emit configurationChanged();
        }
    }
}

void SplashConfigPanel::onBackendChanged(int index) {
    if (index == 1) {
        showWarning(
            this,
            "DRM/KMS Backend Notice",
            "Dynamic DRM mode is not guaranteed during system shutdown,\n"
            "as systemd unmounts shared libraries (/usr) during late teardown.\n"
            "For shutdown splashes or universal compatibility (Boot & Shutdown),\n"
            "the standalone Framebuffer (fbdev) backend is strongly recommended.");
    }
    emit configurationChanged();
}

TQString SplashConfigPanel::compressionMethod() const {
    int idx = m_compressCombo->currentItem();
    switch (idx) {
        case 0: return "auto";
        case 1: return "rle_xor";
        case 2: return "rle_direct";
        case 3: return "sparse";
        case 4: return "raw";
        default: return "auto";
    }
}

void SplashConfigPanel::onSuperCompressToggled(bool on) {
    if (m_superCompressCombo) {
        m_superCompressCombo->setEnabled(on);
    }
    emit configurationChanged();
}

void SplashConfigPanel::onSuperCompressAlgoChanged(int index) {
    (void)index;
    emit configurationChanged();
}

bool SplashConfigPanel::useZx0() const {
    return m_superCompressSwitch && m_superCompressSwitch->isChecked() &&
           m_superCompressCombo && m_superCompressCombo->currentItem() == 1;
}

int SplashConfigPanel::superCompression() const {
    if (!m_superCompressSwitch || !m_superCompressSwitch->isChecked()) return 0;
    if (m_superCompressCombo && m_superCompressCombo->currentItem() == 1) return 1; // ZX0
    return 2; // UPKR
}

void SplashConfigPanel::setSuperCompression(int mode) {
    if (mode <= 0) {
        if (m_superCompressSwitch) m_superCompressSwitch->setState(false);
        if (m_superCompressCombo) m_superCompressCombo->setEnabled(false);
    } else if (mode == 1) { // ZX0
        if (m_superCompressCombo) {
            m_superCompressCombo->setCurrentItem(1);
            m_superCompressCombo->setEnabled(true);
        }
        if (m_superCompressSwitch) m_superCompressSwitch->setState(true);
    } else if (mode == 2) { // UPKR
        if (m_superCompressCombo) {
            m_superCompressCombo->setCurrentItem(0);
            m_superCompressCombo->setEnabled(true);
        }
        if (m_superCompressSwitch) m_superCompressSwitch->setState(true);
    }
}

void SplashConfigPanel::setUseZx0(bool on) {
    if (on) {
        setSuperCompression(1); // ZX0
    } else {
        if (m_superCompressSwitch && m_superCompressCombo && m_superCompressCombo->currentItem() == 1) {
            m_superCompressSwitch->setState(false);
        }
    }
}

void SplashConfigPanel::setLoopMode(int mode) {
    int comboIndex = 0;
    if (mode == 1) comboIndex = 0;      // Infinite Loop (Full cycle) -> Index 0
    else if (mode == 0) comboIndex = 1; // No Loop (Stop at final frame) -> Index 1
    else if (mode == 2) comboIndex = 2; // Partial Loop (Loop from start frame) -> Index 2
    else if (mode == 3) comboIndex = 3; // Ping-Pong (Forward ↔ Backward) -> Index 3
    
    m_loopCombo->setCurrentItem(comboIndex);
    onLoopModeChanged(comboIndex);
}

void SplashConfigPanel::setFrameDelay(int ms) {
    if (ms < 1) ms = 33;
    m_spinDelay->setValue(ms);
    m_sliderDelay->setValue(ms);
    updateFpsLabel(ms);
    m_preview->setFrameDelay(ms);
}

void SplashConfigPanel::setLoopStart(int startFrame) {
    if (startFrame < 0) startFrame = 0;
    m_spinLoopStart->setValue(startFrame);
    m_preview->setLoopMode(loopMode(), startFrame);
}

void SplashConfigPanel::setMinBootLoops(int loops) {
    if (loops < 0) loops = 0;
    if (loops > 10) loops = 10;
    if (m_spinMinLoops) {
        m_spinMinLoops->setValue(loops);
    }
}

void SplashConfigPanel::setFramesPath(const TQString &path, bool reportIntruders) {
    m_framesEdit->setText(path);
    TQFileInfo fi(path);
    if (fi.isFile()) {
        m_savedStaticImagePath = path;
        m_preview->loadSingleImage(path);
    } else {
        m_savedAnimPath = path;
        m_preview->loadFramesFromDir(path, reportIntruders);
    }
}

void SplashConfigPanel::setBinaryName(const TQString &name) {
    m_binaryEdit->setText(name);
    emit configurationChanged();
}

void SplashConfigPanel::setAuthor(const TQString &author) {
    if (m_authorEdit) {
        m_authorEdit->setText(author);
    }
}

void SplashConfigPanel::setNotes(const TQString &notes) {
    if (m_notesEdit) {
        m_notesEdit->setText(notes);
    }
}

void SplashConfigPanel::setBgImagePath(const TQString &path) {
    m_bgImageEdit->setText(path);
    m_savedBgImagePath = path;
    m_preview->setBgImage(path);
}

void SplashConfigPanel::setDisplayMode(int mode) {
    if (mode >= 0 && mode < m_modeCombo->count()) {
        m_modeCombo->setCurrentItem(mode);
        onModeChanged(mode);
    }
}

void SplashConfigPanel::expandPosSection() {
    if (m_grpPos) {
        m_grpPos->setExpanded(true);
    }
}

void SplashConfigPanel::expandColorSection() {
    if (m_grpColor) {
        m_grpColor->setExpanded(true);
    }
}

void SplashConfigPanel::expandBuildSection() {
    if (m_grpBuild) {
        m_grpBuild->setExpanded(true);
    }
}

void SplashConfigPanel::expandTimeSection() {
    if (m_grpTime) {
        m_grpTime->setExpanded(true);
    }
}

void SplashConfigPanel::triggerPipette() {
    if (m_pipetteBtn) {
        m_pipetteBtn->setOn(true);
    }
}

void SplashConfigPanel::cancelPipetteIfActive() {
    if (m_pipetteBtn && m_pipetteBtn->isOn()) {
        m_pipetteBtn->setOn(false);
    }
}

void SplashConfigPanel::setTargetResolution(const TQString &res) {
    for (int i = 0; i < m_resCombo->count(); ++i) {
        if (m_resCombo->text(i).startsWith(res)) {
            m_resCombo->setCurrentItem(i);
            onResolutionChanged(i);
            break;
        }
    }
}

void SplashConfigPanel::onModeChanged(int index) {
    cancelPipetteIfActive();
    int oldMode = m_previousMode;
    m_previousMode = index;
    m_preview->setDisplayMode(index);

    // If switching from an animation mode (0, 1, 2) to a static image mode (3, 4)
    if (index >= 3 && oldMode < 3) {
        // Save current animation directory
        TQString curFrames = m_framesEdit->text().stripWhiteSpace();
        if (TQDir(curFrames).exists()) {
            m_savedAnimPath = curFrames;
        }
        TQString curBg = m_bgImageEdit->text().stripWhiteSpace();
        if (TQFile::exists(curBg)) {
            m_savedBgImagePath = curBg;
        }

        // Select the image: prioritize the background image from mode 1/2
        TQString imageToSelect = "";
        if (!m_savedBgImagePath.isEmpty() && TQFile::exists(m_savedBgImagePath)) {
            imageToSelect = m_savedBgImagePath;
        } else if (!m_savedStaticImagePath.isEmpty() && TQFile::exists(m_savedStaticImagePath)) {
            imageToSelect = m_savedStaticImagePath;
        } else if (TQFile::exists(curFrames)) {
            imageToSelect = curFrames;
        }

        if (!imageToSelect.isEmpty()) {
            m_savedStaticImagePath = imageToSelect;
            m_framesEdit->setText(imageToSelect);
            m_preview->loadSingleImage(imageToSelect);
            TQFileInfo fi(imageToSelect);
            m_binaryEdit->setText("xbs_" + fi.baseName(true));
        } else {
            m_framesEdit->setText("");
            m_framesInfoLabel->setText("Please select a static image file");
        }
    }
    // If switching from a static image mode (3, 4) back to an animation mode (0, 1, 2)
    else if (index < 3 && oldMode >= 3) {
        // Save current static image path
        TQString curStatic = m_framesEdit->text().stripWhiteSpace();
        if (TQFile::exists(curStatic)) {
            m_savedStaticImagePath = curStatic;
        }

        // Restore animation directory
        if (!m_savedAnimPath.isEmpty() && TQDir(m_savedAnimPath).exists()) {
            m_framesEdit->setText(m_savedAnimPath);
            m_preview->loadFramesFromDir(m_savedAnimPath, false);
            TQFileInfo fi(m_savedAnimPath);
            m_binaryEdit->setText("xbs_" + fi.baseName(true));
        }

        // If switching to Mode 1 or 2, restore background image
        if (index == 1 || index == 2) {
            TQString bgToRestore = "";
            if (!m_savedBgImagePath.isEmpty() && TQFile::exists(m_savedBgImagePath)) {
                bgToRestore = m_savedBgImagePath;
            } else if (!m_savedStaticImagePath.isEmpty() && TQFile::exists(m_savedStaticImagePath)) {
                bgToRestore = m_savedStaticImagePath;
            }
            if (!bgToRestore.isEmpty()) {
                m_bgImageEdit->setText(bgToRestore);
                m_preview->setBgImage(bgToRestore);
            }
        }
    }

    if (m_framesLabel) {
        m_framesLabel->setText(index >= 3 ? "Static Image File:" : "Frames Directory:");
    }
    if (m_grpTime) {
        if (index >= 3) m_grpTime->hide();
        else m_grpTime->show();
    }

    if (index == 1) {
        m_bgImageRow->show();
        m_elementSelectorRow->show();
        m_animPosLabel->setText("Animation Position (X / Y pixels):");
        m_resetOffsetBtn->setText("Center Animation (0, 0)");
        m_resetOffsetBtn->show();
        m_spinOffsetX->setEnabled(true);
        m_sliderOffsetX->setEnabled(true);
        m_spinOffsetY->setEnabled(true);
        m_sliderOffsetY->setEnabled(true);
        m_preview->setSelectedElement(SplashPreviewWidget::SelectAnimation);
        onPreviewSelectedElementChanged(SplashPreviewWidget::SelectAnimation);
    } else if (index == 2) {
        m_bgImageRow->show();
        m_elementSelectorRow->hide();
        m_animPosLabel->setText("Animation Position (X / Y pixels):");
        m_resetOffsetBtn->setText("Center Animation (0, 0)");
        m_resetOffsetBtn->show();
        m_spinOffsetX->setEnabled(true);
        m_sliderOffsetX->setEnabled(true);
        m_spinOffsetY->setEnabled(true);
        m_sliderOffsetY->setEnabled(true);
        m_preview->setSelectedElement(SplashPreviewWidget::SelectAnimation);
        onPreviewSelectedElementChanged(SplashPreviewWidget::SelectAnimation);
    } else if (index == 3) {
        m_bgImageRow->hide();
        m_elementSelectorRow->hide();
        m_animPosLabel->setText("Static Image Position (X / Y pixels):");
        m_resetOffsetBtn->setText("Center Image (0, 0)");
        m_resetOffsetBtn->show();
        m_spinOffsetX->setEnabled(true);
        m_sliderOffsetX->setEnabled(true);
        m_spinOffsetY->setEnabled(true);
        m_sliderOffsetY->setEnabled(true);
        m_preview->setSelectedElement(SplashPreviewWidget::SelectAnimation);
        onPreviewSelectedElementChanged(SplashPreviewWidget::SelectAnimation);
    } else if (index == 4) {
        m_bgImageRow->hide();
        m_elementSelectorRow->hide();
        m_animPosLabel->setText("Position: Full Screen (Scales to Target Resolution)");
        m_resetOffsetBtn->hide();
        m_spinOffsetX->setEnabled(false);
        m_sliderOffsetX->setEnabled(false);
        m_spinOffsetY->setEnabled(false);
        m_sliderOffsetY->setEnabled(false);
    } else {
        m_bgImageRow->hide();
        m_elementSelectorRow->hide();
        m_animPosLabel->setText("Animation Position (X / Y pixels):");
        m_resetOffsetBtn->setText("Center Animation (0, 0)");
        m_resetOffsetBtn->show();
        m_spinOffsetX->setEnabled(true);
        m_sliderOffsetX->setEnabled(true);
        m_spinOffsetY->setEnabled(true);
        m_sliderOffsetY->setEnabled(true);
        m_preview->setSelectedElement(SplashPreviewWidget::SelectAnimation);
        onPreviewSelectedElementChanged(SplashPreviewWidget::SelectAnimation);
    }

    emit configurationChanged();
}

void SplashConfigPanel::onSelectAnimClicked() {
    m_preview->setSelectedElement(SplashPreviewWidget::SelectAnimation);
    updateElementSelectorButtons(SplashPreviewWidget::SelectAnimation);
    onPreviewSelectedElementChanged(SplashPreviewWidget::SelectAnimation);
}

void SplashConfigPanel::onSelectBgClicked() {
    m_preview->setSelectedElement(SplashPreviewWidget::SelectBgImage);
    updateElementSelectorButtons(SplashPreviewWidget::SelectBgImage);
    onPreviewSelectedElementChanged(SplashPreviewWidget::SelectBgImage);
}

void SplashConfigPanel::updateElementSelectorButtons(int element) {
    m_btnSelectAnim->blockSignals(true);
    m_btnSelectBg->blockSignals(true);
    m_btnSelectAnim->setOn(element == SplashPreviewWidget::SelectAnimation);
    m_btnSelectBg->setOn(element == SplashPreviewWidget::SelectBgImage);
    m_btnSelectAnim->blockSignals(false);
    m_btnSelectBg->blockSignals(false);
}

void SplashConfigPanel::onBrowseFrames() {
    cancelPipetteIfActive();
    TQString current = m_framesEdit->text();
    if (current.isEmpty() || !TQFile::exists(current)) {
        current = TQDir::currentDirPath();
    }

    if (displayMode() >= 3) {
        // Single static image mode
        TQString file = TQFileDialog::getOpenFileName(current, "PNG/JPEG Images (*.png *.PNG *.jpg *.jpeg)", this);
        if (!file.isEmpty()) {
            m_framesEdit->setText(file);
            m_preview->loadSingleImage(file);
            TQFileInfo fi(file);
            m_binaryEdit->setText("xbs_" + fi.baseName(true));
        }
    } else {
        // Animation directory mode
        TQString dir = TQFileDialog::getExistingDirectory(current, this, "select_dir", "Select PNG Frames Directory", true);
        if (!dir.isEmpty()) {
            while (dir.endsWith("/") && dir.length() > 1) {
                dir.truncate(dir.length() - 1);
            }
            m_framesEdit->setText(dir);
            m_preview->loadFramesFromDir(dir, true);
            TQFileInfo fi(dir);
            TQString bName = fi.baseName(true);
            if (bName.isEmpty()) bName = fi.fileName();
            m_binaryEdit->setText("xbs_" + bName);
        }
    }
}

void SplashConfigPanel::onBrowseBgImage() {
    cancelPipetteIfActive();
    TQString current = m_bgImageEdit->text();
    if (current.isEmpty()) current = TQDir::currentDirPath();

    TQString file = TQFileDialog::getOpenFileName(current, "PNG/JPEG Images (*.png *.PNG *.jpg *.jpeg)", this);
    if (!file.isEmpty()) {
        m_bgImageEdit->setText(file);
        m_preview->setBgImage(file);
    }
}

void SplashConfigPanel::onOffsetXChanged(int val) {
    m_sliderOffsetX->blockSignals(true);
    m_sliderOffsetX->setValue(val);
    m_sliderOffsetX->blockSignals(false);

    int el = m_preview ? (int)m_preview->selectedElement() : (int)SplashPreviewWidget::SelectAnimation;
    if (el == SplashPreviewWidget::SelectBgImage) {
        m_bgOffsetX = val;
        m_preview->setBgOffsetX(val);
    } else {
        m_animOffsetX = val;
        m_preview->setOffsetX(val);
    }
    emit configurationChanged();
}

void SplashConfigPanel::onOffsetYChanged(int val) {
    m_sliderOffsetY->blockSignals(true);
    m_sliderOffsetY->setValue(val);
    m_sliderOffsetY->blockSignals(false);

    int el = m_preview ? (int)m_preview->selectedElement() : (int)SplashPreviewWidget::SelectAnimation;
    if (el == SplashPreviewWidget::SelectBgImage) {
        m_bgOffsetY = val;
        m_preview->setBgOffsetY(val);
    } else {
        m_animOffsetY = val;
        m_preview->setOffsetY(val);
    }
    emit configurationChanged();
}

void SplashConfigPanel::onOffsetXSlider(int val) {
    m_spinOffsetX->blockSignals(true);
    m_spinOffsetX->setValue(val);
    m_spinOffsetX->blockSignals(false);

    int el = m_preview ? (int)m_preview->selectedElement() : (int)SplashPreviewWidget::SelectAnimation;
    if (el == SplashPreviewWidget::SelectBgImage) {
        m_bgOffsetX = val;
        m_preview->setBgOffsetX(val);
    } else {
        m_animOffsetX = val;
        m_preview->setOffsetX(val);
    }
    emit configurationChanged();
}

void SplashConfigPanel::onOffsetYSlider(int val) {
    m_spinOffsetY->blockSignals(true);
    m_spinOffsetY->setValue(val);
    m_spinOffsetY->blockSignals(false);

    int el = m_preview ? (int)m_preview->selectedElement() : (int)SplashPreviewWidget::SelectAnimation;
    if (el == SplashPreviewWidget::SelectBgImage) {
        m_bgOffsetY = val;
        m_preview->setBgOffsetY(val);
    } else {
        m_animOffsetY = val;
        m_preview->setOffsetY(val);
    }
    emit configurationChanged();
}

void SplashConfigPanel::onResetOffsets() {
    int el = m_preview ? (int)m_preview->selectedElement() : (int)SplashPreviewWidget::SelectAnimation;
    if (el == SplashPreviewWidget::SelectBgImage) {
        m_bgOffsetX = 0;
        m_bgOffsetY = 0;
        m_preview->setBgOffsets(0, 0);
    } else {
        m_animOffsetX = 0;
        m_animOffsetY = 0;
        m_preview->setOffsets(0, 0);
    }

    m_spinOffsetX->blockSignals(true);
    m_sliderOffsetX->blockSignals(true);
    m_spinOffsetY->blockSignals(true);
    m_sliderOffsetY->blockSignals(true);

    m_spinOffsetX->setValue(0);
    m_sliderOffsetX->setValue(0);
    m_spinOffsetY->setValue(0);
    m_sliderOffsetY->setValue(0);

    m_spinOffsetX->blockSignals(false);
    m_sliderOffsetX->blockSignals(false);
    m_spinOffsetY->blockSignals(false);
    m_sliderOffsetY->blockSignals(false);

    emit configurationChanged();
}

void SplashConfigPanel::onDelayChanged(int val) {
    m_sliderDelay->blockSignals(true);
    m_sliderDelay->setValue(val);
    m_sliderDelay->blockSignals(false);
    updateFpsLabel(val);
    m_preview->setFrameDelay(val);
    emit configurationChanged();
}

void SplashConfigPanel::onDelaySlider(int val) {
    m_spinDelay->blockSignals(true);
    m_spinDelay->setValue(val);
    m_spinDelay->blockSignals(false);
    updateFpsLabel(val);
    m_preview->setFrameDelay(val);
    emit configurationChanged();
}

void SplashConfigPanel::updateFpsLabel(int delayMs) {
    if (delayMs > 0) {
        double fps = 1000.0 / (double)delayMs;
        m_fpsLabel->setText(TQString("%1 FPS").arg(fps, 0, 'f', 1));
    }
}

void SplashConfigPanel::onLoopModeChanged(int index) {
    if (index == 2) {
        m_loopStartRow->show();
    } else {
        m_loopStartRow->hide();
    }
    m_preview->setLoopMode(loopMode(), loopStart());
    emit configurationChanged();
}

void SplashConfigPanel::onLoopStartChanged(int val) {
    m_preview->setLoopMode(loopMode(), val);
    emit configurationChanged();
}

void SplashConfigPanel::onMinLoopsChanged(int val) {
    (void)val;
    emit configurationChanged();
}

void SplashConfigPanel::onInvertToggled(bool on) {
    m_preview->setInvertFrames(on);
    emit configurationChanged();
}

void SplashConfigPanel::onPickColor() {
    cancelPipetteIfActive();
    TQColor chosen = TQColorDialog::getColor(m_preview->bgColor(), this);
    if (chosen.isValid()) {
        m_preview->setBgColor(chosen);
        char hexBuf[16];
        snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", chosen.red(), chosen.green(), chosen.blue());
        m_hexEdit->blockSignals(true);
        m_hexEdit->setText(hexBuf);
        m_hexEdit->blockSignals(false);
        updateColorButton();
        emit configurationChanged();
    }
}

void SplashConfigPanel::onHexColorEdited(const TQString &hex) {
    cancelPipetteIfActive();
    TQString h = hex.stripWhiteSpace();
    if (h.startsWith("#")) h = h.mid(1);
    if (h.length() == 6) {
        bool ok = false;
        unsigned int val = h.toUInt(&ok, 16);
        if (ok) {
            TQColor c((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF);
            m_preview->setBgColor(c);
            updateColorButton();
            emit configurationChanged();
        }
    }
}

void SplashConfigPanel::updateColorButton() {
    TQColor c = m_preview->bgColor();
    m_colorBtn->setPaletteBackgroundColor(c);
    int lum = (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000;
    m_colorBtn->setPaletteForegroundColor(lum > 128 ? TQt::black : TQt::white);
}

void SplashConfigPanel::onPipetteToggled(bool on) {
    if (on) {
        m_pipetteBtn->setText(" Cancel (Esc)");
        m_preview->startColorPicking();
    } else {
        m_pipetteBtn->setText(" Pick from Canvas");
        m_preview->stopColorPicking();
    }
}

void SplashConfigPanel::onColorPickedFromPreview(const TQColor &color) {
    m_preview->setBgColor(color);
    char hexBuf[16];
    snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", color.red(), color.green(), color.blue());
    m_hexEdit->blockSignals(true);
    m_hexEdit->setText(hexBuf);
    m_hexEdit->blockSignals(false);
    updateColorButton();

    m_pipetteBtn->blockSignals(true);
    m_pipetteBtn->setOn(false);
    m_pipetteBtn->setText(" Pick from Canvas");
    m_pipetteBtn->blockSignals(false);

    emit configurationChanged();
}

void SplashConfigPanel::onColorPickingCancelled() {
    m_pipetteBtn->blockSignals(true);
    m_pipetteBtn->setOn(false);
    m_pipetteBtn->setText(" Pick from Canvas");
    m_pipetteBtn->blockSignals(false);
}

void SplashConfigPanel::onResolutionChanged(int /* index */) {
    TQString res = targetResolution();
    int w = 1920, h = 1080;
    if (sscanf(res.latin1(), "%dx%d", &w, &h) == 2) {
        m_preview->setTargetResolution(w, h);
    }
    emit configurationChanged();
}

void SplashConfigPanel::onPreviewOffsetChanged(int x, int y) {
    m_animOffsetX = x;
    m_animOffsetY = y;
    int el = m_preview ? (int)m_preview->selectedElement() : (int)SplashPreviewWidget::SelectAnimation;
    if (el == SplashPreviewWidget::SelectAnimation) {
        m_spinOffsetX->blockSignals(true);
        m_sliderOffsetX->blockSignals(true);
        m_spinOffsetY->blockSignals(true);
        m_sliderOffsetY->blockSignals(true);

        m_spinOffsetX->setValue(x);
        m_sliderOffsetX->setValue(x);
        m_spinOffsetY->setValue(y);
        m_sliderOffsetY->setValue(y);

        m_spinOffsetX->blockSignals(false);
        m_sliderOffsetX->blockSignals(false);
        m_spinOffsetY->blockSignals(false);
        m_sliderOffsetY->blockSignals(false);
    }
}

void SplashConfigPanel::onPreviewBgOffsetChanged(int x, int y) {
    m_bgOffsetX = x;
    m_bgOffsetY = y;
    int el = m_preview ? (int)m_preview->selectedElement() : (int)SplashPreviewWidget::SelectAnimation;
    if (el == SplashPreviewWidget::SelectBgImage) {
        m_spinOffsetX->blockSignals(true);
        m_sliderOffsetX->blockSignals(true);
        m_spinOffsetY->blockSignals(true);
        m_sliderOffsetY->blockSignals(true);

        m_spinOffsetX->setValue(x);
        m_sliderOffsetX->setValue(x);
        m_spinOffsetY->setValue(y);
        m_sliderOffsetY->setValue(y);

        m_spinOffsetX->blockSignals(false);
        m_sliderOffsetX->blockSignals(false);
        m_spinOffsetY->blockSignals(false);
        m_sliderOffsetY->blockSignals(false);
    }
}

void SplashConfigPanel::onPreviewSelectedElementChanged(int el) {
    updateElementSelectorButtons(el);

    int curX = (el == SplashPreviewWidget::SelectBgImage) ? m_bgOffsetX : m_animOffsetX;
    int curY = (el == SplashPreviewWidget::SelectBgImage) ? m_bgOffsetY : m_animOffsetY;

    m_spinOffsetX->blockSignals(true);
    m_sliderOffsetX->blockSignals(true);
    m_spinOffsetY->blockSignals(true);
    m_sliderOffsetY->blockSignals(true);

    m_spinOffsetX->setValue(curX);
    m_sliderOffsetX->setValue(curX);
    m_spinOffsetY->setValue(curY);
    m_sliderOffsetY->setValue(curY);

    m_spinOffsetX->blockSignals(false);
    m_sliderOffsetX->blockSignals(false);
    m_spinOffsetY->blockSignals(false);
    m_sliderOffsetY->blockSignals(false);

    if (displayMode() == 1) {
        if (el == SplashPreviewWidget::SelectBgImage) {
            m_animPosLabel->setText("Static Image Position (X / Y pixels):");
            m_resetOffsetBtn->setText("Center Static Image (0, 0)");
        } else {
            m_animPosLabel->setText("Animation Position (X / Y pixels):");
            m_resetOffsetBtn->setText("Center Animation (0, 0)");
        }
    }
}

void SplashConfigPanel::onPreviewFramesLoaded(int count, int width, int height) {
    if (count > 0) {
        m_framesInfoLabel->setText(TQString("%1 frames (%2x%3 px)").arg(count).arg(width).arg(height));
        m_framesInfoLabel->setPaletteForegroundColor(TQColor(50, 160, 50));
        m_spinLoopStart->setMaxValue(count > 1 ? count - 1 : 0);
    } else {
        m_framesInfoLabel->setText("No valid images found");
        m_framesInfoLabel->setPaletteForegroundColor(TQColor(200, 60, 60));
    }
}

void SplashConfigPanel::onIntruderFilesDetected(const TQStringList &files) {
    if (files.isEmpty()) return;

    TQString msg = TQString::fromUtf8("The following non-animation asset(s) were detected\n"
                                      "and excluded from the animation sequence:\n\n");
    for (TQStringList::ConstIterator it = files.begin(); it != files.end(); ++it) {
        msg += TQString::fromUtf8("  • ") + *it + "\n";
    }
    msg += TQString::fromUtf8("\nThey will not interfere with the bootsplash animation.");

    TQMessageBox::information(this, "Non-Animation Images Excluded", msg);
}

static TQString makeRelativePath(const TQString &fullPath, const TQString &baseDir) {
    if (fullPath.isEmpty()) return "";
    TQFileInfo fi(fullPath);
    TQString absTarget = fi.absFilePath();
    if (baseDir.isEmpty()) return absTarget;

    TQFileInfo baseFi(baseDir);
    TQString absBase = baseFi.absFilePath();
    if (absBase.endsWith("/")) absBase.truncate(absBase.length() - 1);

    if (absTarget.startsWith(absBase + "/")) {
        return absTarget.mid(absBase.length() + 1);
    }
    return absTarget;
}

static TQString resolveRelativePath(const TQString &path, const TQString &baseDir, const TQString &fileDir) {
    if (path.isEmpty()) return "";
    TQFileInfo fi(path);
    if (fi.isRelative()) {
        if (!baseDir.isEmpty()) {
            TQFileInfo c1(baseDir + "/" + path);
            if (c1.exists()) return c1.absFilePath();
        }
        if (!fileDir.isEmpty()) {
            TQFileInfo c2(fileDir + "/" + path);
            if (c2.exists()) return c2.absFilePath();
        }
    }
    return fi.absFilePath();
}

bool SplashConfigPanel::saveToProjectFile(const TQString &filePath, const TQString &relativeBaseDir) {
    TQFile file(filePath);
    if (!file.open(IO_WriteOnly | IO_Truncate)) {
        return false;
    }

    TQTextStream ts(&file);
    ts.setEncoding(TQTextStream::UnicodeUTF8);

    ts << "[XBootsplashProject]\n";
    ts << "version=1\n";
    ts << "display_mode=" << displayMode() << "\n";
    ts << "frames_path=" << makeRelativePath(framesPath(), relativeBaseDir) << "\n";
    ts << "bg_image_path=" << makeRelativePath(bgImagePath(), relativeBaseDir) << "\n";
    ts << "offset_x=" << offsetX() << "\n";
    ts << "offset_y=" << offsetY() << "\n";
    ts << "bg_offset_x=" << bgOffsetX() << "\n";
    ts << "bg_offset_y=" << bgOffsetY() << "\n";
    ts << "frame_delay=" << frameDelay() << "\n";
    ts << "loop_mode=" << loopMode() << "\n";
    ts << "loop_start=" << loopStart() << "\n";
    ts << "min_boot_loops=" << minBootLoops() << "\n";
    ts << "invert_frames=" << (invertFrames() ? 1 : 0) << "\n";
    ts << "bg_color=" << bgColor().name() << "\n";
    ts << "target_resolution=" << targetResolution() << "\n";
    ts << "binary_name=" << binaryName() << "\n";
    ts << "author=" << author() << "\n";
    ts << "notes=" << notes() << "\n";
    ts << "use_drm=" << (useDrm() ? 1 : 0) << "\n";
    ts << "compression_method=" << compressionMethod() << "\n";
    ts << "use_zx0=" << (useZx0() ? 1 : 0) << "\n";
    ts << "super_compression=" << superCompression() << "\n";

    file.close();
    return true;
}

bool SplashConfigPanel::loadFromProjectFile(const TQString &filePath, const TQString &relativeBaseDir) {
    TQFile file(filePath);
    if (!file.open(IO_ReadOnly)) {
        return false;
    }

    TQTextStream ts(&file);
    ts.setEncoding(TQTextStream::UnicodeUTF8);

    TQMap<TQString, TQString> map;
    while (!ts.atEnd()) {
        TQString line = ts.readLine().stripWhiteSpace();
        if (line.isEmpty() || line.startsWith("#") || line.startsWith(";")) continue;
        if (line.startsWith("[") && line.endsWith("]")) continue;
        int eq = line.find('=');
        if (eq > 0) {
            TQString key = line.left(eq).stripWhiteSpace();
            TQString val = line.mid(eq + 1).stripWhiteSpace();
            map[key] = val;
        }
    }
    file.close();

    // 1. Reset state
    resetToDefaults();

    // 2. Display mode
    int mode = map.contains("display_mode") ? map["display_mode"].toInt() : 0;
    setDisplayMode(mode);

    // 3. Target resolution
    if (map.contains("target_resolution")) {
        setTargetResolution(map["target_resolution"]);
    }

    // 4. Frames path & background image
    TQString fileDir = TQFileInfo(filePath).dirPath(true);
    if (map.contains("frames_path")) {
        TQString rawFrames = map["frames_path"];
        if (!rawFrames.isEmpty()) {
            TQString fullFrames = resolveRelativePath(rawFrames, relativeBaseDir, fileDir);
            setFramesPath(fullFrames, false);
        }
    }
    if (map.contains("bg_image_path")) {
        TQString rawBg = map["bg_image_path"];
        if (!rawBg.isEmpty()) {
            TQString fullBg = resolveRelativePath(rawBg, relativeBaseDir, fileDir);
            setBgImagePath(fullBg);
        }
    }

    // 5. Positions & Offsets
    int ox = map.contains("offset_x") ? map["offset_x"].toInt() : 0;
    int oy = map.contains("offset_y") ? map["offset_y"].toInt() : 0;
    m_preview->setOffsets(ox, oy);
    onPreviewOffsetChanged(ox, oy);

    int bgOx = map.contains("bg_offset_x") ? map["bg_offset_x"].toInt() : 0;
    int bgOy = map.contains("bg_offset_y") ? map["bg_offset_y"].toInt() : 0;
    m_preview->setBgOffsets(bgOx, bgOy);
    onPreviewBgOffsetChanged(bgOx, bgOy);

    // 6. Timing and playback
    if (map.contains("frame_delay")) {
        setFrameDelay(map["frame_delay"].toInt());
    }
    if (map.contains("loop_mode")) {
        setLoopMode(map["loop_mode"].toInt());
    }
    if (map.contains("loop_start")) {
        setLoopStart(map["loop_start"].toInt());
    }
    if (map.contains("min_boot_loops")) {
        setMinBootLoops(map["min_boot_loops"].toInt());
    }
    if (map.contains("invert_frames")) {
        bool inv = (map["invert_frames"].toInt() == 1);
        m_invertSwitch->setState(inv);
        m_preview->setInvertFrames(inv);
    }

    // 7. Background Color
    if (map.contains("bg_color")) {
        TQColor col(map["bg_color"]);
        if (col.isValid()) {
            m_hexEdit->setText(col.name().upper());
            m_preview->setBgColor(col);
            updateColorButton();
        }
    }

    // 8. Build settings
    if (map.contains("binary_name") && !map["binary_name"].isEmpty()) {
        setBinaryName(map["binary_name"]);
    }
    if (map.contains("author")) {
        setAuthor(map["author"]);
    }
    if (map.contains("notes")) {
        setNotes(map["notes"]);
    }
    if (map.contains("use_drm")) {
        bool drm = (map["use_drm"].toInt() == 1);
        setUseDrm(drm, false);
    }
    if (map.contains("compression_method")) {
        TQString comp = map["compression_method"];
        if (comp == "rle_xor") m_compressCombo->setCurrentItem(1);
        else if (comp == "rle_direct") m_compressCombo->setCurrentItem(2);
        else if (comp == "sparse") m_compressCombo->setCurrentItem(3);
        else if (comp == "raw") m_compressCombo->setCurrentItem(4);
        else m_compressCombo->setCurrentItem(0);
    }
    if (map.contains("super_compression")) {
        setSuperCompression(map["super_compression"].toInt());
    } else if (map.contains("use_zx0")) {
        bool zx0 = (map["use_zx0"].toInt() == 1);
        setSuperCompression(zx0 ? 1 : 0);
    }

    emit configurationChanged();
    return true;
}

void SplashConfigPanel::resetToDefaults() {
    m_savedAnimPath = "";
    m_savedBgImagePath = "";
    m_savedStaticImagePath = "";

    // 1. Reset mode to 0
    setDisplayMode(0);

    // 2. Clear paths
    m_framesEdit->setText("");
    m_framesInfoLabel->setText("No frames loaded");
    m_framesInfoLabel->setPaletteForegroundColor(TQColor(128, 128, 128));
    m_bgImageEdit->setText("");
    m_preview->clearFrames();
    m_preview->clearBgImage();

    // 3. Reset offsets
    m_preview->resetAnimOffsets();
    m_preview->resetBgOffsets();
    onPreviewOffsetChanged(0, 0);
    onPreviewBgOffsetChanged(0, 0);

    // 4. Reset timings
    setFrameDelay(33);
    setLoopMode(1); // Default is Infinite Loop (Full cycle)
    setLoopStart(0);
    setMinBootLoops(0);
    m_invertSwitch->setState(false);
    m_preview->setInvertFrames(false);

    // 5. Reset color to black
    m_hexEdit->setText("#000000");
    m_preview->setBgColor(TQColor(0, 0, 0));
    updateColorButton();

    // 6. Reset target resolution to 1920x1080 (item 0)
    m_resCombo->setCurrentItem(0);
    onResolutionChanged(0);

    // 7. Reset binary name, metadata, and backend
    m_binaryEdit->setText("xbootsplash");
    if (m_authorEdit) m_authorEdit->setText("");
    if (m_notesEdit) m_notesEdit->setText("");
    setUseDrm(false, false);
    if (m_compressCombo->count() > 0) m_compressCombo->setCurrentItem(0);
    setSuperCompression(0);

    emit configurationChanged();
}

bool SplashConfigPanel::eventFilter(TQObject *watched, TQEvent *event) {
    if (watched == m_superCompressLabel && event->type() == TQEvent::MouseButtonRelease) {
        TQMouseEvent *me = static_cast<TQMouseEvent*>(event);
        if (me->button() == TQt::LeftButton && m_superCompressSwitch) {
            m_superCompressSwitch->toggleState();
            return true;
        }
    }
    return TQScrollView::eventFilter(watched, event);
}

#include "config_panel.moc"
