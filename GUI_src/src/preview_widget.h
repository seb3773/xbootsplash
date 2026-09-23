#ifndef SPLASH_PREVIEW_WIDGET_H
#define SPLASH_PREVIEW_WIDGET_H

#include <ntqwidget.h>
#include <ntqpixmap.h>
#include <ntqimage.h>
#include <ntqcolor.h>
#include <ntqtimer.h>
#include <ntqvaluelist.h>
#include <ntqstringlist.h>
#include <vector>

class SplashPreviewWidget : public TQWidget {
    TQ_OBJECT

public:
    explicit SplashPreviewWidget(TQWidget *parent = 0, const char *name = 0);
    virtual ~SplashPreviewWidget();

    int screenWidth() const { return m_screenWidth; }
    int screenHeight() const { return m_screenHeight; }
    int offsetX() const { return m_offsetX; }
    int offsetY() const { return m_offsetY; }
    int bgOffsetX() const { return m_bgOffsetX; }
    int bgOffsetY() const { return m_bgOffsetY; }
    int displayMode() const { return m_displayMode; }
    TQColor bgColor() const { return m_bgColor; }
    int frameDelay() const { return m_frameDelay; }
    int loopMode() const { return m_loopMode; }
    int loopStart() const { return m_loopStart; }
    bool invertFrames() const { return m_invertFrames; }
    bool showCrosshair() const { return m_showCrosshair; }
    bool isPlaying() const { return m_isPlaying; }
    int frameCount() const { return (int)m_frames.size(); }
    int currentFrameIndex() const { return m_currentFrame; }
    TQSize objectSize() const;
    const std::vector<TQPixmap>& displayFrames() const { return m_displayFrames; }
    const TQPixmap& bgPixmap() const { return m_bgPixmap; }
    const TQString& currentFramesDir() const { return m_framesDir; }
    const TQString& currentBgImagePath() const { return m_bgImagePath; }

    TQPixmap renderVirtualComposite(int frameIndex, int targetW, int targetH) const;
    bool exportPreviewVisual(const TQString &outputPath, int targetW = 480, int targetH = 270) const;

    enum SelectedElement {
        SelectAnimation = 0,
        SelectBgImage = 1
    };

    SelectedElement selectedElement() const { return m_selectedElement; }
    void setSelectedElement(SelectedElement el);

    bool isColorPicking() const { return m_isColorPicking; }

public slots:
    void startColorPicking();
    void stopColorPicking();
    void play();
    void pause();
    void togglePlay();
    void stop();
    void nextFrame();
    void prevFrame();
    void firstFrame();
    void lastFrame();
    void seekFrame(int index);

    void setOffsetX(int x);
    void setOffsetY(int y);
    void setOffsets(int x, int y);
    void setBgOffsetX(int x);
    void setBgOffsetY(int y);
    void setBgOffsets(int x, int y);
    void resetAnimOffsets();
    void resetBgOffsets();
    void setBgColor(const TQColor &color);
    void setBgImage(const TQString &imagePath);
    void clearBgImage();
    void clearFrames();
    void setFrameDelay(int ms);
    void setLoopMode(int mode, int startFrame = 0);
    void setInvertFrames(bool invert);
    void setDisplayMode(int mode);
    void setTargetResolution(int w, int h);
    void setShowCrosshair(bool show);
    void stepTimerForTest();

    bool loadFramesFromDir(const TQString &dirPath, bool reportIntruders = true);
    bool loadSingleImage(const TQString &filePath);

signals:
    void frameChanged(int current, int total);
    void playbackStateChanged(bool isPlaying);
    void offsetChanged(int x, int y);
    void bgOffsetChanged(int x, int y);
    void selectedElementChanged(int element);
    void framesLoaded(int count, int width, int height);
    void intruderFilesDetected(const TQStringList &files);
    void colorPicked(const TQColor &color);
    void colorPickingCancelled();

protected:
    virtual void paintEvent(TQPaintEvent *e);
    virtual void mousePressEvent(TQMouseEvent *e);
    virtual void mouseMoveEvent(TQMouseEvent *e);
    virtual void mouseReleaseEvent(TQMouseEvent *e);
    virtual void keyPressEvent(TQKeyEvent *e);
    virtual void resizeEvent(TQResizeEvent *e);

private slots:
    void onTimerTick();

private:
    void computeViewport(TQRect &viewportRect, double &scale) const;
    TQPoint screenToVirtual(const TQPoint &pos) const;
    TQPoint virtualToScreen(const TQPoint &pos) const;
    TQRect virtualFrameRect() const;
    TQRect virtualBgRect() const;

    int m_screenWidth;
    int m_screenHeight;
    int m_offsetX;
    int m_offsetY;
    int m_bgOffsetX;
    int m_bgOffsetY;
    int m_displayMode;
    TQColor m_bgColor;
    int m_frameDelay;
    int m_loopMode;
    int m_loopStart;
    int m_playbackDirection;
    bool m_invertFrames;
    bool m_showCrosshair;
    bool m_isPlaying;

    SelectedElement m_selectedElement;
    SelectedElement m_dragTarget;

    TQString m_framesDir;
    TQString m_bgImagePath;
    TQPixmap m_bgPixmap;
    std::vector<TQPixmap> m_frames;
    std::vector<TQPixmap> m_displayFrames;
    int m_currentFrame;
    TQTimer *m_timer;

    bool m_isDragging;
    TQPoint m_dragStartPos;
    int m_dragStartOffsetX;
    int m_dragStartOffsetY;

    bool m_isColorPicking;
    bool m_wasPlayingBeforePicking;
    TQPoint m_hoverPos;
    TQColor m_hoverColor;
    TQImage m_pickingImage;
};

#endif // SPLASH_PREVIEW_WIDGET_H
