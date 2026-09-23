#ifndef SPLASH_FULLSCREEN_PREVIEW_H
#define SPLASH_FULLSCREEN_PREVIEW_H

#include <ntqwidget.h>
#include <ntqpixmap.h>
#include <ntqcolor.h>
#include <ntqtimer.h>
#include <vector>

class SplashFullscreenPreview : public TQWidget {
    TQ_OBJECT

public:
    explicit SplashFullscreenPreview(TQWidget *parent = 0, const char *name = 0);
    virtual ~SplashFullscreenPreview();

    void setupAnimation(const std::vector<TQPixmap> &frames,
                        int frameDelay,
                        int offsetX, int offsetY,
                        int bgOffsetX, int bgOffsetY,
                        int displayMode,
                        const TQColor &bgColor,
                        const TQPixmap &bgPixmap,
                        int loopMode, int loopStart);

    void startPreview();

protected:
    virtual void paintEvent(TQPaintEvent *e);
    virtual void keyPressEvent(TQKeyEvent *e);
    virtual void mousePressEvent(TQMouseEvent *e);

private slots:
    void onTick();
    void onToastTimeout();

private:
    std::vector<TQPixmap> m_frames;
    int m_frameDelay;
    int m_offsetX;
    int m_offsetY;
    int m_bgOffsetX;
    int m_bgOffsetY;
    int m_displayMode;
    TQColor m_bgColor;
    TQPixmap m_bgPixmap;
    int m_loopMode;
    int m_loopStart;
    int m_playbackDirection;

    int m_currentFrame;
    TQTimer *m_timer;
    TQTimer *m_toastTimer;
    bool m_showToast;
};

#endif // SPLASH_FULLSCREEN_PREVIEW_H
