#include "fullscreen_preview.h"

#include <ntqpainter.h>
#include <ntqevent.h>
#include <ntqcursor.h>
#include <ntqimage.h>

SplashFullscreenPreview::SplashFullscreenPreview(TQWidget *parent, const char *name)
    : TQWidget(parent, name, WType_TopLevel | WStyle_Customize | WStyle_NoBorder | WStyle_StaysOnTop),
      m_frameDelay(33),
      m_offsetX(0),
      m_offsetY(80),
      m_bgOffsetX(0),
      m_bgOffsetY(0),
      m_displayMode(0),
      m_bgColor(TQt::black),
      m_loopMode(1),
      m_loopStart(0),
      m_playbackDirection(1),
      m_currentFrame(0),
      m_showToast(true)
{
    setBackgroundMode(TQt::NoBackground);
    setFocusPolicy(TQWidget::StrongFocus);

    m_timer = new TQTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTick()));

    m_toastTimer = new TQTimer(this);
    connect(m_toastTimer, SIGNAL(timeout()), this, SLOT(onToastTimeout()));
}

SplashFullscreenPreview::~SplashFullscreenPreview() {
}

void SplashFullscreenPreview::setupAnimation(const std::vector<TQPixmap> &frames,
                                            int frameDelay,
                                            int offsetX, int offsetY,
                                            int bgOffsetX, int bgOffsetY,
                                            int displayMode,
                                            const TQColor &bgColor,
                                            const TQPixmap &bgPixmap,
                                            int loopMode, int loopStart)
{
    m_frames = frames;
    m_frameDelay = frameDelay > 0 ? frameDelay : 33;
    m_offsetX = offsetX;
    m_offsetY = offsetY;
    m_bgOffsetX = bgOffsetX;
    m_bgOffsetY = bgOffsetY;
    m_displayMode = displayMode;
    m_bgColor = bgColor;
    m_bgPixmap = bgPixmap;
    m_loopMode = loopMode;
    m_loopStart = loopStart;
    m_playbackDirection = 1;
    m_currentFrame = 0;
}

void SplashFullscreenPreview::startPreview() {
    m_currentFrame = 0;
    m_showToast = true;
    showFullScreen();
    raise();
    setFocus();
    m_timer->start(m_frameDelay);
    m_toastTimer->start(2500, true); // 2.5s single shot
    update();
}

void SplashFullscreenPreview::onTick() {
    if (m_frames.empty()) return;

    int total = (int)m_frames.size();

    if (m_loopMode == 3) {
        if (total <= 1) return;
        int next = m_currentFrame + m_playbackDirection;
        if (next >= total) {
            m_playbackDirection = -1;
            next = total - 2;
            if (next < 0) next = 0;
        } else if (next < 0) {
            m_playbackDirection = 1;
            next = (total > 1) ? 1 : 0;
        }
        m_currentFrame = next;
        update();
        return;
    }

    int next = m_currentFrame + 1;

    if (next >= total) {
        if (m_loopMode == 0) {
            m_timer->stop();
            return;
        } else if (m_loopMode == 2) {
            next = (m_loopStart >= 0 && m_loopStart < total) ? m_loopStart : 0;
        } else {
            next = 0;
        }
    }

    m_currentFrame = next;
    update();
}

void SplashFullscreenPreview::onToastTimeout() {
    m_showToast = false;
    update();
}

void SplashFullscreenPreview::paintEvent(TQPaintEvent * /* e */) {
    TQPixmap buffer(size());
    TQPainter p(&buffer);

    int sw = width();
    int sh = height();

    // 1. Solid background color
    buffer.fill(m_bgColor);

    // 2. Background image / Fullscreen static image
    if (m_displayMode == 4) {
        TQPixmap staticPm;
        if (!m_frames.empty()) staticPm = m_frames[0];
        else if (!m_bgPixmap.isNull()) staticPm = m_bgPixmap;

        if (!staticPm.isNull()) {
            TQImage scaledBg = staticPm.convertToImage().smoothScale(sw, sh);
            TQPixmap pm;
            pm.convertFromImage(scaledBg);
            p.drawPixmap(0, 0, pm);
        }
    } else {
        if (!m_bgPixmap.isNull()) {
            if (m_displayMode == 2) {
                // Fullscreen BG: scale to screen size
                TQImage scaledBg = m_bgPixmap.convertToImage().smoothScale(sw, sh);
                TQPixmap pm;
                pm.convertFromImage(scaledBg);
                p.drawPixmap(0, 0, pm);
            } else if (m_displayMode == 1) {
                // Centered BG + offset
                int bgX = (sw / 2) + m_bgOffsetX - (m_bgPixmap.width() / 2);
                int bgY = (sh / 2) + m_bgOffsetY - (m_bgPixmap.height() / 2);
                p.drawPixmap(bgX, bgY, m_bgPixmap);
            }
        }

        // 3. Animation frame (Mode 0, 1, 2) or Centered static image (Mode 3)
        if (!m_frames.empty() && m_currentFrame < (int)m_frames.size()) {
            const TQPixmap &frame = m_frames[m_currentFrame];
            int fx = (sw / 2) + m_offsetX - (frame.width() / 2);
            int fy = (sh / 2) + m_offsetY - (frame.height() / 2);
            p.drawPixmap(fx, fy, frame);
        }
    }

    // 4. Toast notification
    if (m_showToast) {
        TQString toastText = "Fullscreen Preview - Press Esc or click to exit";
        p.setFont(TQFont("sans", 10, TQFont::Bold));
        TQFontMetrics fm = p.fontMetrics();
        int tw = fm.width(toastText) + 24;
        int th = fm.height() + 14;
        int tx = (sw - tw) / 2;
        int ty = sh - th - 32;

        p.setPen(TQt::NoPen);
        p.setBrush(TQColor(20, 20, 20));
        p.drawRoundRect(tx, ty, tw, th, 15, 15);

        p.setPen(TQColor(240, 240, 240));
        p.drawText(tx + 12, ty + fm.ascent() + 7, toastText);
    }

    p.end();

    TQPainter screenP(this);
    screenP.drawPixmap(0, 0, buffer);
}

void SplashFullscreenPreview::keyPressEvent(TQKeyEvent *e) {
    if (e->key() == TQt::Key_Escape || e->key() == TQt::Key_F11 || e->key() == TQt::Key_Space) {
        m_timer->stop();
        m_toastTimer->stop();
        close();
        e->accept();
    } else {
        TQWidget::keyPressEvent(e);
    }
}

void SplashFullscreenPreview::mousePressEvent(TQMouseEvent *e) {
    m_timer->stop();
    m_toastTimer->stop();
    close();
    e->accept();
}

#include "fullscreen_preview.moc"
