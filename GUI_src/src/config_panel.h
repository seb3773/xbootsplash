#ifndef SPLASH_CONFIG_PANEL_H
#define SPLASH_CONFIG_PANEL_H

#include <ntqwidget.h>
#include <ntqlineedit.h>
#include <ntqpushbutton.h>
#include <ntqcombobox.h>
#include <ntqspinbox.h>
#include <ntqslider.h>
#include <ntqlabel.h>
#include <ntqcheckbox.h>
#include <ntqcolor.h>
#include <ntqscrollview.h>

class SplashPreviewWidget;
class TQtToggleSwitch;

class SplashConfigPanel : public TQScrollView {
    TQ_OBJECT

public:
    explicit SplashConfigPanel(SplashPreviewWidget *preview, TQWidget *parent = 0, const char *name = 0);
    virtual ~SplashConfigPanel();

    int displayMode() const;
    TQString framesPath() const;
    TQString bgImagePath() const;
    int offsetX() const;
    int offsetY() const;
    int bgOffsetX() const;
    int bgOffsetY() const;
    int frameDelay() const;
    int loopMode() const;
    int loopStart() const;
    int minBootLoops() const;
    bool invertFrames() const;
    TQColor bgColor() const;
    TQString targetResolution() const;
    TQString binaryName() const;
    TQString author() const;
    TQString notes() const;
    bool useDrm() const;
    TQString compressionMethod() const;
    bool useZx0() const;
    int superCompression() const; /* 0 = None, 1 = ZX0, 2 = UPKR */

    void setFramesPath(const TQString &path, bool reportIntruders = false);
    void setBgImagePath(const TQString &path);
    void setBinaryName(const TQString &name);
    void setAuthor(const TQString &author);
    void setNotes(const TQString &notes);
    void setDisplayMode(int mode);
    void setTargetResolution(const TQString &res);
    void setUseDrm(bool drm, bool triggerNotice = false);
    void setUseZx0(bool on);
    void setSuperCompression(int mode);
    void setLoopMode(int mode);
    void setMinBootLoops(int loops);
    void setFrameDelay(int ms);
    void setLoopStart(int startFrame);
    void expandPosSection();
    void expandColorSection();
    void expandBuildSection();
    void expandTimeSection();
    void triggerPipette();
    void cancelPipetteIfActive();

    bool saveToProjectFile(const TQString &filePath, const TQString &relativeBaseDir);
    bool loadFromProjectFile(const TQString &filePath, const TQString &relativeBaseDir);
    void resetToDefaults();

public slots:
    void onModeChanged(int index);
    void onBrowseFrames();
    void onBrowseBgImage();

    void onSelectAnimClicked();
    void onSelectBgClicked();

    void onOffsetXChanged(int val);
    void onOffsetYChanged(int val);
    void onOffsetXSlider(int val);
    void onOffsetYSlider(int val);
    void onResetOffsets();

    void onDelayChanged(int val);
    void onDelaySlider(int val);
    void onLoopModeChanged(int index);
    void onLoopStartChanged(int val);
    void onMinLoopsChanged(int val);
    void onInvertToggled(bool on);
    void onPickColor();
    void onHexColorEdited(const TQString &hex);
    void onPipetteToggled(bool on);
    void onColorPickedFromPreview(const TQColor &color);
    void onColorPickingCancelled();
    void onResolutionChanged(int index);

    void onPreviewOffsetChanged(int x, int y);
    void onPreviewBgOffsetChanged(int x, int y);
    void onPreviewSelectedElementChanged(int el);
    void onPreviewFramesLoaded(int count, int width, int height);
    void onIntruderFilesDetected(const TQStringList &files);
    void onBackendChanged(int index);
    void onSuperCompressToggled(bool on);
    void onSuperCompressAlgoChanged(int index);

signals:
    void configurationChanged();

private:
    void setupUI();
    void updateColorButton();
    void updateFpsLabel(int delayMs);
    void updateElementSelectorButtons(int element);

    SplashPreviewWidget *m_preview;

    // Controls
    TQComboBox *m_modeCombo;
    TQLineEdit *m_framesEdit;
    TQPushButton *m_browseFramesBtn;
    TQLabel *m_framesInfoLabel;

    TQLineEdit *m_bgImageEdit;
    TQPushButton *m_browseBgBtn;
    TQWidget *m_bgImageRow;

    // Element Selector (Mode 1)
    TQWidget *m_elementSelectorRow;
    TQPushButton *m_btnSelectAnim;
    TQPushButton *m_btnSelectBg;

    // Animation / Static image positioning
    TQLabel *m_animPosLabel;
    TQSpinBox *m_spinOffsetX;
    TQSlider *m_sliderOffsetX;
    TQSpinBox *m_spinOffsetY;
    TQSlider *m_sliderOffsetY;
    TQPushButton *m_resetOffsetBtn;

    int m_animOffsetX;
    int m_animOffsetY;
    int m_bgOffsetX;
    int m_bgOffsetY;

    TQComboBox *m_resCombo;

    TQSpinBox *m_spinDelay;
    TQSlider *m_sliderDelay;
    TQLabel *m_fpsLabel;

    TQComboBox *m_loopCombo;
    TQSpinBox *m_spinLoopStart;
    TQSpinBox *m_spinMinLoops;
    TQWidget *m_loopStartRow;
    TQtToggleSwitch *m_invertSwitch;

    TQPushButton *m_colorBtn;
    TQLineEdit *m_hexEdit;
    TQPushButton *m_pipetteBtn;

    TQLineEdit *m_binaryEdit;
    TQLineEdit *m_authorEdit;
    TQLineEdit *m_notesEdit;
    TQComboBox *m_backendCombo;
    TQComboBox *m_compressCombo;
    TQtToggleSwitch *m_superCompressSwitch;
    TQLabel *m_superCompressLabel;
    TQComboBox *m_superCompressCombo;
    TQLabel *m_superCompressHint;

    // Backward compatibility pointer
    TQtToggleSwitch *m_zx0Switch;

    TQLabel *m_framesLabel;
    class TQtCollapsibleGroup *m_grpPos;
    class TQtCollapsibleGroup *m_grpTime;
    class TQtCollapsibleGroup *m_grpColor;
    class TQtCollapsibleGroup *m_grpBuild;
    TQString m_savedAnimPath;
    TQString m_savedBgImagePath;
    TQString m_savedStaticImagePath;
    int m_previousMode;

protected:
    bool eventFilter(TQObject *watched, TQEvent *event);
};

#endif // SPLASH_CONFIG_PANEL_H
