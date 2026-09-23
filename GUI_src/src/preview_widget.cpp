#include "preview_widget.h"

#include <ntqpainter.h>
#include <ntqevent.h>
#include <ntqdir.h>
#include <ntqfileinfo.h>
#include <ntqcursor.h>
#include <ntqapplication.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <algorithm>
#include <vector>
#include <set>
#include <utility>

namespace {
    // Natural compare helper for filenames
    bool naturalFilenameCompare(const TQString &s1, const TQString &s2) {
        const char *p1 = s1.latin1();
        const char *p2 = s2.latin1();

        while (*p1 && *p2) {
            if (isdigit((unsigned char)*p1) && isdigit((unsigned char)*p2)) {
                int z1 = 0, z2 = 0;
                while (*p1 == '0') { z1++; p1++; }
                while (*p2 == '0') { z2++; p2++; }

                const char *start1 = p1;
                const char *start2 = p2;
                while (isdigit((unsigned char)*p1)) p1++;
                while (isdigit((unsigned char)*p2)) p2++;

                int len1 = p1 - start1;
                int len2 = p2 - start2;

                if (len1 != len2) return len1 < len2;

                int num_cmp = strncmp(start1, start2, len1);
                if (num_cmp != 0) return num_cmp < 0;

                if (z1 != z2) return z2 < z1;
            } else {
                char c1 = tolower((unsigned char)*p1);
                char c2 = tolower((unsigned char)*p2);
                if (c1 != c2) return c1 < c2;
                p1++;
                p2++;
            }
        }
        return *p1 == '\0' && *p2 != '\0';
    }

    std::vector<long> extractNumbersFromTQString(const TQString &str) {
        std::vector<long> result;
        const char *p = str.latin1();
        while (*p) {
            if (isdigit((unsigned char)*p)) {
                char *end = 0;
                result.push_back(strtol(p, &end, 10));
                p = end;
            } else {
                p++;
            }
        }
        return result;
    }

    bool isGuiIntruderImage(const TQString &filename, bool hasDigitsMajority) {
        static const char *keywords[] = {
            "background", "bg_", "bg-", "static", "preview", "thumb",
            "thumbnail", "palette", "screenshot", "cover", "poster"
        };
        TQString lower = filename.lower();
        for (size_t k = 0; k < sizeof(keywords) / sizeof(keywords[0]); ++k) {
            if (lower.find(keywords[k]) != -1) return true;
        }
        if (hasDigitsMajority) {
            bool hasDigit = false;
            for (unsigned int i = 0; i < lower.length(); ++i) {
                if (lower[i].isDigit()) {
                    hasDigit = true;
                    break;
                }
            }
            if (!hasDigit) return true;
        }
        return false;
    }
}

SplashPreviewWidget::SplashPreviewWidget(TQWidget *parent, const char *name)
    : TQWidget(parent, name),
      m_screenWidth(1920),
      m_screenHeight(1080),
      m_offsetX(0),
      m_offsetY(80),
      m_bgOffsetX(0),
      m_bgOffsetY(0),
      m_displayMode(0),
      m_bgColor(TQt::black),
      m_frameDelay(33),
      m_loopMode(1),
      m_loopStart(0),
      m_playbackDirection(1),
      m_invertFrames(false),
      m_showCrosshair(true),
      m_isPlaying(false),
      m_selectedElement(SelectAnimation),
      m_dragTarget(SelectAnimation),
      m_currentFrame(0),
      m_isDragging(false),
      m_dragStartOffsetX(0),
      m_dragStartOffsetY(0),
      m_isColorPicking(false),
      m_wasPlayingBeforePicking(false),
      m_hoverPos(-1, -1),
      m_hoverColor(TQt::black)
{
    setFocusPolicy(TQWidget::StrongFocus);
    setBackgroundMode(TQt::NoBackground); // Manual double buffer
    setMouseTracking(true);

    m_timer = new TQTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimerTick()));
}

SplashPreviewWidget::~SplashPreviewWidget() {
}

TQSize SplashPreviewWidget::objectSize() const {
    if (!m_displayFrames.empty()) {
        return m_displayFrames[0].size();
    }
    return TQSize(0, 0);
}

void SplashPreviewWidget::startColorPicking() {
    m_isColorPicking = true;
    m_wasPlayingBeforePicking = m_isPlaying;
    if (m_isPlaying) {
        pause();
    }
    setCursor(TQCursor(TQt::CrossCursor));
    TQPoint globalMouse = TQCursor::pos();
    TQPoint localMouse = mapFromGlobal(globalMouse);
    if (rect().contains(localMouse)) {
        m_hoverPos = localMouse;
    } else {
        m_hoverPos = TQPoint(width() / 2, height() / 2);
    }
    update();
}

void SplashPreviewWidget::stopColorPicking() {
    if (!m_isColorPicking) return;
    m_isColorPicking = false;
    unsetCursor();
    m_hoverPos = TQPoint(-1, -1);
    if (m_wasPlayingBeforePicking) {
        play();
    }
    update();
}

void SplashPreviewWidget::play() {
    if (m_displayFrames.empty()) return;
    m_isPlaying = true;
    m_timer->start(m_frameDelay > 0 ? m_frameDelay : 33);
    emit playbackStateChanged(true);
}

void SplashPreviewWidget::pause() {
    m_isPlaying = false;
    m_timer->stop();
    emit playbackStateChanged(false);
}

void SplashPreviewWidget::togglePlay() {
    if (m_isPlaying) {
        pause();
    } else {
        play();
    }
}

void SplashPreviewWidget::stop() {
    pause();
    m_playbackDirection = 1;
    seekFrame(0);
}

void SplashPreviewWidget::nextFrame() {
    if (m_displayFrames.empty()) return;
    int next = m_currentFrame + 1;
    if (next >= (int)m_displayFrames.size()) {
        next = (m_loopMode == 2 && m_loopStart < (int)m_displayFrames.size()) ? m_loopStart : 0;
    }
    seekFrame(next);
}

void SplashPreviewWidget::prevFrame() {
    if (m_displayFrames.empty()) return;
    int prev = m_currentFrame - 1;
    if (prev < 0) {
        prev = (int)m_displayFrames.size() - 1;
    }
    seekFrame(prev);
}

void SplashPreviewWidget::firstFrame() {
    seekFrame(0);
}

void SplashPreviewWidget::lastFrame() {
    if (!m_displayFrames.empty()) {
        seekFrame((int)m_displayFrames.size() - 1);
    }
}

void SplashPreviewWidget::seekFrame(int index) {
    if (m_displayFrames.empty()) {
        m_currentFrame = 0;
        emit frameChanged(0, 0);
        update();
        return;
    }
    if (index < 0) index = 0;
    if (index >= (int)m_displayFrames.size()) index = (int)m_displayFrames.size() - 1;

    m_currentFrame = index;
    emit frameChanged(m_currentFrame + 1, (int)m_displayFrames.size());
    update();
}

void SplashPreviewWidget::onTimerTick() {
    if (m_displayFrames.empty()) return;

    int total = (int)m_displayFrames.size();

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
        emit frameChanged(m_currentFrame + 1, total);
        update();
        return;
    }

    int next = m_currentFrame + 1;

    if (next >= total) {
        if (m_loopMode == 0) {
            // No loop - stay on last frame and pause
            pause();
            return;
        } else if (m_loopMode == 2) {
            // Partial loop
            next = (m_loopStart >= 0 && m_loopStart < total) ? m_loopStart : 0;
        } else {
            // Full loop
            next = 0;
        }
    }

    m_currentFrame = next;
    emit frameChanged(m_currentFrame + 1, total);
    update();
}

void SplashPreviewWidget::stepTimerForTest() {
    onTimerTick();
}

void SplashPreviewWidget::setOffsetX(int x) {
    if (m_offsetX != x) {
        m_offsetX = x;
        emit offsetChanged(m_offsetX, m_offsetY);
        update();
    }
}

void SplashPreviewWidget::setOffsetY(int y) {
    if (m_offsetY != y) {
        m_offsetY = y;
        emit offsetChanged(m_offsetX, m_offsetY);
        update();
    }
}

void SplashPreviewWidget::setOffsets(int x, int y) {
    if (m_offsetX != x || m_offsetY != y) {
        m_offsetX = x;
        m_offsetY = y;
        emit offsetChanged(m_offsetX, m_offsetY);
        update();
    }
}

void SplashPreviewWidget::setSelectedElement(SelectedElement el) {
    if (m_selectedElement != el) {
        m_selectedElement = el;
        emit selectedElementChanged((int)m_selectedElement);
        update();
    }
}

void SplashPreviewWidget::setBgOffsetX(int x) {
    if (m_bgOffsetX != x) {
        m_bgOffsetX = x;
        emit bgOffsetChanged(m_bgOffsetX, m_bgOffsetY);
        update();
    }
}

void SplashPreviewWidget::setBgOffsetY(int y) {
    if (m_bgOffsetY != y) {
        m_bgOffsetY = y;
        emit bgOffsetChanged(m_bgOffsetX, m_bgOffsetY);
        update();
    }
}

void SplashPreviewWidget::setBgOffsets(int x, int y) {
    if (m_bgOffsetX != x || m_bgOffsetY != y) {
        m_bgOffsetX = x;
        m_bgOffsetY = y;
        emit bgOffsetChanged(m_bgOffsetX, m_bgOffsetY);
        update();
    }
}

void SplashPreviewWidget::resetAnimOffsets() {
    setOffsets(0, 0);
}

void SplashPreviewWidget::resetBgOffsets() {
    setBgOffsets(0, 0);
}

void SplashPreviewWidget::setBgColor(const TQColor &color) {
    m_bgColor = color;
    update();
}

void SplashPreviewWidget::setBgImage(const TQString &imagePath) {
    m_bgImagePath = imagePath;
    if (!imagePath.isEmpty() && TQFile::exists(imagePath)) {
        m_bgPixmap.load(imagePath);
    } else {
        m_bgPixmap = TQPixmap();
    }
    update();
}

void SplashPreviewWidget::clearBgImage() {
    m_bgImagePath = TQString();
    m_bgPixmap = TQPixmap();
    update();
}

void SplashPreviewWidget::clearFrames() {
    pause();
    m_framesDir = TQString();
    m_frames.clear();
    m_displayFrames.clear();
    m_currentFrame = 0;
    emit frameChanged(0, 0);
    emit framesLoaded(0, 0, 0);
    update();
}

void SplashPreviewWidget::setFrameDelay(int ms) {
    if (ms < 1) ms = 1;
    if (ms > 1000) ms = 1000;
    m_frameDelay = ms;
    if (m_isPlaying) {
        m_timer->changeInterval(m_frameDelay);
    }
}

void SplashPreviewWidget::setLoopMode(int mode, int startFrame) {
    m_loopMode = mode;
    m_loopStart = startFrame;
    m_playbackDirection = 1;
}

void SplashPreviewWidget::setInvertFrames(bool invert) {
    if (m_invertFrames == invert) return;
    m_invertFrames = invert;

    // Rebuild display frames list
    m_displayFrames.clear();
    if (m_invertFrames) {
        for (int i = (int)m_frames.size() - 1; i >= 0; --i) {
            m_displayFrames.push_back(m_frames[i]);
        }
    } else {
        m_displayFrames = m_frames;
    }

    seekFrame(0);
}

void SplashPreviewWidget::setDisplayMode(int mode) {
    m_displayMode = mode;
    update();
}

void SplashPreviewWidget::setTargetResolution(int w, int h) {
    if (w > 0 && h > 0) {
        m_screenWidth = w;
        m_screenHeight = h;
        update();
    }
}

void SplashPreviewWidget::setShowCrosshair(bool show) {
    m_showCrosshair = show;
    update();
}

bool SplashPreviewWidget::loadFramesFromDir(const TQString &dirPath, bool reportIntruders) {
    pause();
    m_framesDir = dirPath;
    m_frames.clear();
    m_displayFrames.clear();
    m_currentFrame = 0;

    TQDir dir(dirPath);
    if (!dir.exists()) {
        emit framesLoaded(0, 0, 0);
        update();
        return false;
    }

    TQStringList filters;
    filters << "*.png" << "*.PNG" << "*.jpg" << "*.JPG" << "*.jpeg" << "*.JPEG";
    dir.setNameFilter(filters.join(" "));
    dir.setFilter(TQDir::Files | TQDir::Readable);

    TQStringList fileList = dir.entryList();
    if (fileList.isEmpty()) {
        emit framesLoaded(0, 0, 0);
        update();
        return false;
    }

    // Check if files have digits
    int filesWithDigits = 0;
    for (TQStringList::ConstIterator it = fileList.begin(); it != fileList.end(); ++it) {
        TQString name = *it;
        for (unsigned int i = 0; i < name.length(); ++i) {
            if (name[i].isDigit()) {
                filesWithDigits++;
                break;
            }
        }
    }

    std::vector<TQString> candidateNames;
    TQStringList intruderList;

    for (TQStringList::ConstIterator it = fileList.begin(); it != fileList.end(); ++it) {
        if (isGuiIntruderImage(*it, filesWithDigits > 0)) {
            intruderList.append(*it);
        } else {
            candidateNames.push_back(*it);
        }
    }

    // Fallback if all images were excluded
    if (candidateNames.empty() && !intruderList.isEmpty()) {
        for (TQStringList::ConstIterator it = intruderList.begin(); it != intruderList.end(); ++it) {
            candidateNames.push_back(*it);
        }
        intruderList.clear();
    }

    // 1. Natural sort
    std::sort(candidateNames.begin(), candidateNames.end(), naturalFilenameCompare);

    // 2. Sequence column detection
    if (candidateNames.size() > 1) {
        std::vector<std::vector<long> > allNums;
        size_t minNums = 999;
        for (size_t i = 0; i < candidateNames.size(); ++i) {
            std::vector<long> nums = extractNumbersFromTQString(candidateNames[i]);
            if (nums.size() < minNums) minNums = nums.size();
            allNums.push_back(nums);
        }

        int bestCol = -1;
        int bestScore = -1;
        if (minNums > 0) {
            for (size_t col = 0; col < minNums; ++col) {
                std::set<long> uniqueVals;
                long minV = allNums[0][col];
                long maxV = allNums[0][col];
                for (size_t i = 0; i < candidateNames.size(); ++i) {
                    long v = allNums[i][col];
                    if (v < minV) minV = v;
                    if (v > maxV) maxV = v;
                    uniqueVals.insert(v);
                }
                if (uniqueVals.size() == candidateNames.size()) {
                    int score = 1000;
                    if (maxV - minV + 1 == (long)candidateNames.size()) score += 500;
                    score += (int)col;
                    if (score > bestScore) {
                        bestScore = score;
                        bestCol = (int)col;
                    }
                }
            }
        }

        if (bestCol >= 0) {
            std::vector<std::pair<long, TQString> > indexed;
            for (size_t i = 0; i < candidateNames.size(); ++i) {
                indexed.push_back(std::make_pair(allNums[i][bestCol], candidateNames[i]));
            }
            std::sort(indexed.begin(), indexed.end());
            for (size_t i = 0; i < indexed.size(); ++i) {
                candidateNames[i] = indexed[i].second;
            }
        }
    }

    for (size_t i = 0; i < candidateNames.size(); ++i) {
        TQString fullPath = dir.filePath(candidateNames[i]);
        TQPixmap pm;
        if (pm.load(fullPath)) {
            m_frames.push_back(pm);
        }
    }

    if (m_invertFrames) {
        for (int i = (int)m_frames.size() - 1; i >= 0; --i) {
            m_displayFrames.push_back(m_frames[i]);
        }
    } else {
        m_displayFrames = m_frames;
    }

    int count = (int)m_displayFrames.size();
    int w = count > 0 ? m_displayFrames[0].width() : 0;
    int h = count > 0 ? m_displayFrames[0].height() : 0;

    seekFrame(0);
    emit framesLoaded(count, w, h);

    if (reportIntruders && !intruderList.isEmpty()) {
        emit intruderFilesDetected(intruderList);
    }

    return count > 0;
}

bool SplashPreviewWidget::loadSingleImage(const TQString &filePath) {
    pause();
    m_frames.clear();
    m_displayFrames.clear();
    m_currentFrame = 0;

    TQPixmap pm;
    if (pm.load(filePath)) {
        m_frames.push_back(pm);
        m_displayFrames.push_back(pm);
        seekFrame(0);
        emit framesLoaded(1, pm.width(), pm.height());
        return true;
    }

    emit framesLoaded(0, 0, 0);
    update();
    return false;
}

void SplashPreviewWidget::computeViewport(TQRect &viewportRect, double &scale) const {
    int w = width();
    int h = height();
    if (w <= 20 || h <= 20) {
        viewportRect = TQRect(0, 0, w, h);
        scale = 1.0;
        return;
    }

    // Margin around preview
    const int margin = 16;
    int availW = w - margin * 2;
    int availH = h - margin * 2;
    if (availW < 10) availW = 10;
    if (availH < 10) availH = 10;

    double scaleX = (double)availW / (double)m_screenWidth;
    double scaleY = (double)availH / (double)m_screenHeight;
    scale = (scaleX < scaleY) ? scaleX : scaleY;

    int vpW = (int)(m_screenWidth * scale);
    int vpH = (int)(m_screenHeight * scale);
    int vpX = (w - vpW) / 2;
    int vpY = (h - vpH) / 2;

    viewportRect = TQRect(vpX, vpY, vpW, vpH);
}

TQPoint SplashPreviewWidget::screenToVirtual(const TQPoint &pos) const {
    TQRect vp;
    double scale = 1.0;
    computeViewport(vp, scale);
    if (scale <= 0.0) return TQPoint(0, 0);

    int vx = (int)((pos.x() - vp.left()) / scale);
    int vy = (int)((pos.y() - vp.top()) / scale);
    return TQPoint(vx, vy);
}

TQPoint SplashPreviewWidget::virtualToScreen(const TQPoint &pos) const {
    TQRect vp;
    double scale = 1.0;
    computeViewport(vp, scale);
    int sx = vp.left() + (int)(pos.x() * scale);
    int sy = vp.top() + (int)(pos.y() * scale);
    return TQPoint(sx, sy);
}

TQRect SplashPreviewWidget::virtualFrameRect() const {
    if (m_displayMode == 4) {
        return TQRect(0, 0, m_screenWidth, m_screenHeight);
    }
    if (m_displayFrames.empty() || m_currentFrame >= (int)m_displayFrames.size()) {
        return TQRect(0, 0, 0, 0);
    }
    const TQPixmap &frame = m_displayFrames[m_currentFrame];
    int fw = frame.width();
    int fh = frame.height();
    int fx = (m_screenWidth / 2) + m_offsetX - (fw / 2);
    int fy = (m_screenHeight / 2) + m_offsetY - (fh / 2);
    return TQRect(fx, fy, fw, fh);
}

TQRect SplashPreviewWidget::virtualBgRect() const {
    if (m_bgPixmap.isNull()) {
        return TQRect(0, 0, 0, 0);
    }
    int bw = m_bgPixmap.width();
    int bh = m_bgPixmap.height();
    int bx = (m_screenWidth / 2) + m_bgOffsetX - (bw / 2);
    int by = (m_screenHeight / 2) + m_bgOffsetY - (bh / 2);
    return TQRect(bx, by, bw, bh);
}

void SplashPreviewWidget::paintEvent(TQPaintEvent * /* e */) {
    TQPixmap buffer(size());
    TQPainter p(&buffer);

    // 1. Draw workspace background (dark canvas)
    buffer.fill(TQColor(30, 32, 36));

    TQRect vp;
    double scale = 1.0;
    computeViewport(vp, scale);

    bool drawGuides = m_showCrosshair && !m_isColorPicking;

    // Subtle drop shadow behind virtual screen
    p.setPen(TQt::NoPen);
    p.setBrush(TQColor(15, 15, 18));
    p.drawRect(vp.x() + 4, vp.y() + 4, vp.width(), vp.height());

    // 2. Draw virtual screen content
    // Set clipping to viewport
    p.setClipRect(vp);

    // Background color of virtual screen
    p.fillRect(vp, m_bgColor);

    // If Mode 4 (Single static image full screen)
    if (m_displayMode == 4) {
        TQPixmap staticPm;
        if (!m_displayFrames.empty()) staticPm = m_displayFrames[0];
        else if (!m_bgPixmap.isNull()) staticPm = m_bgPixmap;

        if (!staticPm.isNull()) {
            TQImage scaledBg = staticPm.convertToImage().smoothScale(vp.width(), vp.height());
            TQPixmap pm;
            pm.convertFromImage(scaledBg);
            p.drawPixmap(vp.left(), vp.top(), pm);

            if (drawGuides) {
                p.setPen(TQPen(TQColor(0, 200, 255), 2, TQt::DashLine));
                p.setBrush(TQt::NoBrush);
                p.drawRect(vp.left(), vp.top(), vp.width(), vp.height());

                TQString tag = "Static Image [Full Screen]";
                p.setFont(TQFont("sans", 7, TQFont::Bold));
                TQFontMetrics bfm = p.fontMetrics();
                int tw = bfm.width(tag);
                p.setBrush(TQColor(20, 20, 20));
                p.drawRoundRect(vp.left() + 4, vp.top() + 4, tw + 8, bfm.height() + 4, 30, 30);
                p.setPen(TQColor(0, 200, 255));
                p.drawText(vp.left() + 8, vp.top() + 4 + bfm.ascent() + 2, tag);
            }
        }
    } else if (m_displayMode == 1 || m_displayMode == 2) {
        // Mode 1 (centered BG image) or Mode 2 (fullscreen BG image)
        if (!m_bgPixmap.isNull()) {
            if (m_displayMode == 2) {
                // Fullscreen BG: scale to virtual screen
                TQImage scaledBg = m_bgPixmap.convertToImage().smoothScale(vp.width(), vp.height());
                TQPixmap pm;
                pm.convertFromImage(scaledBg);
                p.drawPixmap(vp.left(), vp.top(), pm);
            } else if (m_displayMode == 1) {
                // Static background image + BG offset
                int bgW = (int)(m_bgPixmap.width() * scale);
                int bgH = (int)(m_bgPixmap.height() * scale);
                int bgX = vp.left() + (vp.width() / 2) + (int)(m_bgOffsetX * scale) - (bgW / 2);
                int bgY = vp.top() + (vp.height() / 2) + (int)(m_bgOffsetY * scale) - (bgH / 2);
                TQImage scaledBg = m_bgPixmap.convertToImage().smoothScale(bgW, bgH);
                TQPixmap pm;
                pm.convertFromImage(scaledBg);
                p.drawPixmap(bgX, bgY, pm);

                // Background bounding box & handles when guides enabled
                if (drawGuides) {
                    bool isSel = (m_selectedElement == SelectBgImage);
                    TQColor boxColor = isSel ? TQColor(245, 158, 11) : TQColor(180, 140, 30);
                    p.setPen(TQPen(boxColor, isSel ? 2 : 1, TQt::DashLine));
                    p.setBrush(TQt::NoBrush);
                    p.drawRect(bgX, bgY, bgW, bgH);

                    if (isSel) {
                        // Corner handles
                        p.setPen(TQt::NoPen);
                        p.setBrush(boxColor);
                        const int hs = 6;
                        p.drawRect(bgX - hs/2, bgY - hs/2, hs, hs);
                        p.drawRect(bgX + bgW - hs/2, bgY - hs/2, hs, hs);
                        p.drawRect(bgX - hs/2, bgY + bgH - hs/2, hs, hs);
                        p.drawRect(bgX + bgW - hs/2, bgY + bgH - hs/2, hs, hs);

                        // Badge
                        TQString tag = TQString("Background [%1, %2]").arg(m_bgOffsetX).arg(m_bgOffsetY);
                        p.setFont(TQFont("sans", 7, TQFont::Bold));
                        TQFontMetrics bfm = p.fontMetrics();
                        int tw = bfm.width(tag);
                        p.setBrush(TQColor(20, 20, 20));
                        p.drawRoundRect(bgX + 2, bgY + 2, tw + 8, bfm.height() + 4, 30, 30);
                        p.setPen(boxColor);
                        p.drawText(bgX + 6, bgY + 2 + bfm.ascent() + 2, tag);
                    }
                }
            }
        }
    }

    // 3. Draw active animation frame (Modes 0, 1, 2) or Centered static image (Mode 3)
    if (m_displayMode != 4 && !m_displayFrames.empty() && m_currentFrame < (int)m_displayFrames.size()) {
        const TQPixmap &srcFrame = m_displayFrames[m_currentFrame];
        int fw = (int)(srcFrame.width() * scale);
        int fh = (int)(srcFrame.height() * scale);
        if (fw > 0 && fh > 0) {
            int fx = vp.left() + (vp.width() / 2) + (int)(m_offsetX * scale) - (fw / 2);
            int fy = vp.top() + (vp.height() / 2) + (int)(m_offsetY * scale) - (fh / 2);

            TQImage scaledFrame = srcFrame.convertToImage().smoothScale(fw, fh);
            TQPixmap pm;
            pm.convertFromImage(scaledFrame);
            p.drawPixmap(fx, fy, pm);

            // Bounding box & handles when guides enabled
            if (drawGuides) {
                bool isSel = (m_displayMode != 1 || m_selectedElement == SelectAnimation);
                TQColor boxColor = isSel ? TQColor(0, 200, 255) : TQColor(0, 160, 210);
                p.setPen(TQPen(boxColor, isSel ? 2 : 1, TQt::DashLine));
                p.setBrush(TQt::NoBrush);
                p.drawRect(fx, fy, fw, fh);

                if (isSel) {
                    // Corner handles
                    p.setPen(TQt::NoPen);
                    p.setBrush(boxColor);
                    const int hs = 6;
                    p.drawRect(fx - hs/2, fy - hs/2, hs, hs);
                    p.drawRect(fx + fw - hs/2, fy - hs/2, hs, hs);
                    p.drawRect(fx - hs/2, fy + fh - hs/2, hs, hs);
                    p.drawRect(fx + fw - hs/2, fy + fh - hs/2, hs, hs);

                    // Badge
                    TQString tag = (m_displayMode == 3 ? TQString("Static Image [%1, %2]") : TQString("Animation [%1, %2]")).arg(m_offsetX).arg(m_offsetY);
                    p.setFont(TQFont("sans", 7, TQFont::Bold));
                    TQFontMetrics bfm = p.fontMetrics();
                    int tw = bfm.width(tag);
                    p.setBrush(TQColor(20, 20, 20));
                    p.drawRoundRect(fx + 2, fy + 2, tw + 8, bfm.height() + 4, 30, 30);
                    p.setPen(boxColor);
                    p.drawText(fx + 6, fy + 2 + bfm.ascent() + 2, tag);
                }
            }
        }
    }

    // 4. Crosshairs / Alignment Guides
    if (drawGuides) {
        int cx = vp.left() + vp.width() / 2;
        int cy = vp.top() + vp.height() / 2;

        p.setPen(TQPen(TQColor(160, 160, 160), 1, TQt::DotLine));
        p.drawLine(cx, vp.top(), cx, vp.bottom());
        p.drawLine(vp.left(), cy, vp.right(), cy);

        // Center point dot
        p.setPen(TQPen(TQColor(255, 200, 0), 2));
        p.drawPoint(cx, cy);
    }

    // Reset clipping
    p.setClipping(false);

    // 5. Screen border frame
    p.setPen(TQPen(TQColor(80, 85, 95), 1));
    p.setBrush(TQt::NoBrush);
    p.drawRect(vp);

    // 6. Resolution label badge in top-left corner of preview widget
    TQString resStr = TQString("%1 x %2").arg(m_screenWidth).arg(m_screenHeight);
    p.setFont(TQFont("sans", 8, TQFont::Bold));
    TQFontMetrics fm = p.fontMetrics();
    int textW = fm.width(resStr);
    int badgeW = textW + 12;
    int badgeH = fm.height() + 6;
    p.setPen(TQt::NoPen);
    p.setBrush(TQColor(20, 20, 20));
    p.drawRoundRect(8, 8, badgeW, badgeH, 20, 20);
    p.setPen(TQColor(200, 200, 200));
    p.drawText(14, 8 + fm.ascent() + 3, resStr);

    // 7. Eyedropper / Color Picker Overlay
    if (m_isColorPicking) {
        // Save pristine frame for accurate color sampling
        m_pickingImage = buffer.convertToImage();

        if (m_hoverPos.x() >= 0 && m_hoverPos.y() >= 0) {
            int hx = m_hoverPos.x();
            int hy = m_hoverPos.y();

            // Precision crosshair lines with center gap
            p.setPen(TQPen(TQColor(0, 0, 0), 2));
            p.drawLine(hx - 14, hy, hx - 4, hy);
            p.drawLine(hx + 4, hy, hx + 14, hy);
            p.drawLine(hx, hy - 14, hx, hy - 4);
            p.drawLine(hx, hy + 4, hx, hy + 14);

            p.setPen(TQPen(TQColor(255, 255, 255), 1));
            p.drawLine(hx - 14, hy, hx - 4, hy);
            p.drawLine(hx + 4, hy, hx + 14, hy);
            p.drawLine(hx, hy - 14, hx, hy - 4);
            p.drawLine(hx, hy + 4, hx, hy + 14);

            // Center targeting circle
            p.setBrush(TQt::NoBrush);
            p.setPen(TQPen(TQColor(255, 255, 255), 1));
            p.drawEllipse(hx - 4, hy - 4, 9, 9);

            // Floating Inspector card / Magnifier HUD
            int cardW = 172;
            int cardH = 54;
            int cardX = hx + 18;
            int cardY = hy + 18;

            if (cardX + cardW > width() - 8) {
                cardX = hx - cardW - 16;
            }
            if (cardY + cardH > height() - 8) {
                cardY = hy - cardH - 16;
            }
            // Hard bounds clamp to guarantee 100% visibility
            if (cardX + cardW > width() - 8) cardX = width() - cardW - 8;
            if (cardX < 8) cardX = 8;
            if (cardY + cardH > height() - 8) cardY = height() - cardH - 8;
            if (cardY < 8) cardY = 8;

            // Card shadow & container
            p.setPen(TQt::NoPen);
            p.setBrush(TQColor(10, 12, 16));
            p.drawRoundRect(cardX + 2, cardY + 2, cardW, cardH, 16, 16);

            p.setBrush(TQColor(24, 26, 32));
            p.setPen(TQPen(TQColor(75, 80, 92), 1));
            p.drawRoundRect(cardX, cardY, cardW, cardH, 16, 16);

            // Swatch rect (34x34)
            int swSize = 34;
            int swX = cardX + 10;
            int swY = cardY + 10;
            p.setPen(TQPen(TQColor(240, 240, 240), 1));
            p.setBrush(m_hoverColor);
            p.drawRect(swX, swY, swSize, swSize);

            // Swatch inner border for contrast on light colors
            p.setPen(TQPen(TQColor(40, 40, 45), 1));
            p.setBrush(TQt::NoBrush);
            p.drawRect(swX + 1, swY + 1, swSize - 2, swSize - 2);

            // Text
            char hexBuf[16];
            snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", m_hoverColor.red(), m_hoverColor.green(), m_hoverColor.blue());
            char rgbBuf[32];
            snprintf(rgbBuf, sizeof(rgbBuf), "RGB: %d, %d, %d", m_hoverColor.red(), m_hoverColor.green(), m_hoverColor.blue());

            int textX = swX + swSize + 10;
            p.setPen(TQColor(255, 255, 255));
            p.setFont(TQFont("sans", 9, TQFont::Bold));
            p.drawText(textX, cardY + 21, hexBuf);

            p.setPen(TQColor(185, 190, 200));
            p.setFont(TQFont("sans", 7, TQFont::Normal));
            p.drawText(textX, cardY + 34, rgbBuf);

            p.setPen(TQColor(135, 140, 150));
            p.setFont(TQFont("sans", 6, TQFont::Normal));
            p.drawText(textX, cardY + 46, TQString::fromUtf8("Click to pick • Esc to cancel"));
        }
    }

    p.end();

    // Blit to screen widget
    TQPainter widgetPainter(this);
    widgetPainter.drawPixmap(0, 0, buffer);
}

void SplashPreviewWidget::mousePressEvent(TQMouseEvent *e) {
    if (m_isColorPicking) {
        if (e->button() == TQt::LeftButton) {
            if (m_pickingImage.isNull()) {
                repaint();
            }
            TQColor picked = m_hoverColor;
            if (m_pickingImage.valid(e->x(), e->y())) {
                TQRgb rgb = m_pickingImage.pixel(e->x(), e->y());
                picked = TQColor(tqRed(rgb), tqGreen(rgb), tqBlue(rgb));
            }
            stopColorPicking();
            emit colorPicked(picked);
        } else if (e->button() == TQt::RightButton) {
            stopColorPicking();
            emit colorPickingCancelled();
        }
        return;
    }
    if (m_displayMode == 4) {
        return; // Fullscreen mode is fixed to screen
    }
    if (e->button() == TQt::LeftButton) {
        TQRect vp;
        double scale = 1.0;
        computeViewport(vp, scale);

        if (vp.contains(e->pos())) {
            TQPoint vpos = screenToVirtual(e->pos());
            TQRect frameRect = virtualFrameRect();
            TQRect bgRect = virtualBgRect();

            if (m_displayMode == 1) {
                // In Mode 1, test animation first (as it's overlaid on top)
                if (frameRect.contains(vpos)) {
                    setSelectedElement(SelectAnimation);
                    m_dragTarget = SelectAnimation;
                    m_dragStartOffsetX = m_offsetX;
                    m_dragStartOffsetY = m_offsetY;
                } else if (!m_bgPixmap.isNull() && bgRect.contains(vpos)) {
                    setSelectedElement(SelectBgImage);
                    m_dragTarget = SelectBgImage;
                    m_dragStartOffsetX = m_bgOffsetX;
                    m_dragStartOffsetY = m_bgOffsetY;
                } else {
                    // Clicked canvas: drag whichever element is currently selected
                    m_dragTarget = m_selectedElement;
                    m_dragStartOffsetX = (m_dragTarget == SelectAnimation) ? m_offsetX : m_bgOffsetX;
                    m_dragStartOffsetY = (m_dragTarget == SelectAnimation) ? m_offsetY : m_bgOffsetY;
                }
            } else {
                setSelectedElement(SelectAnimation);
                m_dragTarget = SelectAnimation;
                m_dragStartOffsetX = m_offsetX;
                m_dragStartOffsetY = m_offsetY;
            }

            m_isDragging = true;
            m_dragStartPos = e->pos();
            setCursor(TQCursor(TQt::SizeAllCursor));
            update();
        }
    }
}

void SplashPreviewWidget::mouseMoveEvent(TQMouseEvent *e) {
    if (m_isColorPicking) {
        m_hoverPos = e->pos();
        if (m_pickingImage.isNull()) {
            repaint();
        }
        if (m_pickingImage.valid(e->x(), e->y())) {
            TQRgb rgb = m_pickingImage.pixel(e->x(), e->y());
            m_hoverColor = TQColor(tqRed(rgb), tqGreen(rgb), tqBlue(rgb));
        }
        setCursor(TQCursor(TQt::CrossCursor));
        update();
        return;
    }
    if (m_isDragging) {
        TQRect vp;
        double scale = 1.0;
        computeViewport(vp, scale);
        if (scale > 0.0) {
            int deltaX = (int)((e->pos().x() - m_dragStartPos.x()) / scale);
            int deltaY = (int)((e->pos().y() - m_dragStartPos.y()) / scale);
            if (m_dragTarget == SelectAnimation) {
                setOffsets(m_dragStartOffsetX + deltaX, m_dragStartOffsetY + deltaY);
            } else {
                setBgOffsets(m_dragStartOffsetX + deltaX, m_dragStartOffsetY + deltaY);
            }
        }
    } else {
        TQRect vp;
        double scale = 1.0;
        computeViewport(vp, scale);
        if (vp.contains(e->pos())) {
            TQPoint vpos = screenToVirtual(e->pos());
            if (m_displayMode == 1) {
                if (virtualFrameRect().contains(vpos) || (!m_bgPixmap.isNull() && virtualBgRect().contains(vpos))) {
                    setCursor(TQCursor(TQt::PointingHandCursor));
                } else {
                    setCursor(TQCursor(TQt::CrossCursor));
                }
            } else if (virtualFrameRect().contains(vpos)) {
                setCursor(TQCursor(TQt::PointingHandCursor));
            } else {
                setCursor(TQCursor(TQt::CrossCursor));
            }
        } else {
            setCursor(TQCursor(TQt::ArrowCursor));
        }
    }
}

void SplashPreviewWidget::mouseReleaseEvent(TQMouseEvent *e) {
    if (m_isColorPicking) return;
    if (e->button() == TQt::LeftButton && m_isDragging) {
        m_isDragging = false;
        setCursor(TQCursor(TQt::ArrowCursor));
    }
}

void SplashPreviewWidget::keyPressEvent(TQKeyEvent *e) {
    if (m_isColorPicking && e->key() == TQt::Key_Escape) {
        stopColorPicking();
        emit colorPickingCancelled();
        return;
    }
    TQWidget::keyPressEvent(e);
}

void SplashPreviewWidget::resizeEvent(TQResizeEvent * /* e */) {
    update();
}

TQPixmap SplashPreviewWidget::renderVirtualComposite(int frameIndex, int targetW, int targetH) const {
    if (targetW <= 0 || targetH <= 0) {
        targetW = 480;
        targetH = 270;
    }

    TQPixmap canvas(targetW, targetH);
    canvas.fill(m_bgColor);

    TQPainter p(&canvas);
    double scale = (double)targetW / (double)(m_screenWidth > 0 ? m_screenWidth : 1920);

    // 1. Mode 4 (full screen static image)
    if (m_displayMode == 4) {
        TQPixmap staticPm;
        if (!m_displayFrames.empty()) staticPm = m_displayFrames[0];
        else if (!m_bgPixmap.isNull()) staticPm = m_bgPixmap;

        if (!staticPm.isNull()) {
            TQImage scaled = staticPm.convertToImage().smoothScale(targetW, targetH);
            TQPixmap pm;
            pm.convertFromImage(scaled);
            p.drawPixmap(0, 0, pm);
        }
        return canvas;
    }

    // 2. Background image for modes 1 and 2
    if ((m_displayMode == 1 || m_displayMode == 2) && !m_bgPixmap.isNull()) {
        if (m_displayMode == 2) {
            TQImage scaled = m_bgPixmap.convertToImage().smoothScale(targetW, targetH);
            TQPixmap pm;
            pm.convertFromImage(scaled);
            p.drawPixmap(0, 0, pm);
        } else if (m_displayMode == 1) {
            int bgW = (int)(m_bgPixmap.width() * scale);
            int bgH = (int)(m_bgPixmap.height() * scale);
            int bgX = (targetW / 2) + (int)(m_bgOffsetX * scale) - (bgW / 2);
            int bgY = (targetH / 2) + (int)(m_bgOffsetY * scale) - (bgH / 2);
            if (bgW > 0 && bgH > 0) {
                TQImage scaled = m_bgPixmap.convertToImage().smoothScale(bgW, bgH);
                TQPixmap pm;
                pm.convertFromImage(scaled);
                p.drawPixmap(bgX, bgY, pm);
            }
        }
    }

    // 3. Animation frame or centered static image (Modes 0, 1, 2, 3)
    if (frameIndex >= 0 && frameIndex < (int)m_displayFrames.size()) {
        const TQPixmap &srcFrame = m_displayFrames[frameIndex];
        int fw = (int)(srcFrame.width() * scale);
        int fh = (int)(srcFrame.height() * scale);
        if (fw > 0 && fh > 0) {
            int fx = (targetW / 2) + (int)(m_offsetX * scale) - (fw / 2);
            int fy = (targetH / 2) + (int)(m_offsetY * scale) - (fh / 2);
            TQImage scaled = srcFrame.convertToImage().smoothScale(fw, fh);
            TQPixmap pm;
            pm.convertFromImage(scaled);
            p.drawPixmap(fx, fy, pm);
        }
    }

    return canvas;
}

bool SplashPreviewWidget::exportPreviewVisual(const TQString &outputPath, int targetW, int targetH) const {
    if (m_displayFrames.empty() && m_bgPixmap.isNull()) return false;

    if (targetW <= 0 || targetH <= 0) {
        targetW = 480;
        targetH = 270;
    }

    if (m_displayMode >= 2) {
        // Static image modes (2, 3, 4): export lossless PNG directly
        TQPixmap pm = renderVirtualComposite(0, targetW, targetH);
        return pm.save(outputPath, "PNG");
    } else {
        // Animation modes (0, 1): export animated GIF looping over all frames
        char tmpTemplate[] = "/tmp/xbs_gifgen_XXXXXX";
        char *tmpDir = mkdtemp(tmpTemplate);
        if (!tmpDir) return false;
        TQString tmpDirPath(tmpDir);

        int n = (int)m_displayFrames.size();
        int frameIdx = 0;
        for (int i = 0; i < n; ++i) {
            TQPixmap pm = renderVirtualComposite(i, targetW, targetH);
            char fName[64];
            snprintf(fName, sizeof(fName), "/frame_%04d.png", frameIdx++);
            pm.save(tmpDirPath + fName, "PNG");
            tqApp->processEvents();
        }
        if (m_loopMode == 3 && n > 2) {
            for (int i = n - 2; i >= 1; --i) {
                TQPixmap pm = renderVirtualComposite(i, targetW, targetH);
                char fName[64];
                snprintf(fName, sizeof(fName), "/frame_%04d.png", frameIdx++);
                pm.save(tmpDirPath + fName, "PNG");
                tqApp->processEvents();
            }
        }

        int fps = (m_frameDelay > 0) ? (1000 / m_frameDelay) : 15;
        if (fps < 1) fps = 1;

        // Try ffmpeg first (fast, high quality palettegen)
        TQString cmd = TQString("ffmpeg -y -framerate %1 -i \"%2/frame_%04d.png\" "
                               "-vf \"split[s0][s1];[s0]palettegen=max_colors=128[p];[s1][p]paletteuse\" "
                               "\"%3\" >/dev/null 2>&1")
                       .arg(fps).arg(tmpDirPath).arg(outputPath);
        int ret = system(cmd.latin1());
        if (ret != 0 || !TQFile::exists(outputPath) || TQFileInfo(outputPath).size() == 0) {
            // Fallback: convert
            int cs = (m_frameDelay + 5) / 10;
            if (cs < 1) cs = 1;
            cmd = TQString("convert -delay %1 -loop 0 \"%2/frame_*.png\" \"%3\" >/dev/null 2>&1")
                  .arg(cs).arg(tmpDirPath).arg(outputPath);
            ret = system(cmd.latin1());
        }

        system(TQString("rm -rf \"%1\"").arg(tmpDirPath).latin1());
        return (ret == 0 && TQFile::exists(outputPath) && TQFileInfo(outputPath).size() > 0);
    }
}

#include "preview_widget.moc"
