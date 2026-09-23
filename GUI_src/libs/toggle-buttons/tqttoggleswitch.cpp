#include "tqttoggleswitch.h"

#include <ntqapplication.h>
#include <ntqpainter.h>
#include <ntqpen.h>
#include <ntqfont.h>
#include <ntqfontmetrics.h>
#include <ntqevent.h>

#include <math.h>

static inline double tqt_absd(double v) { return v < 0.0 ? -v : v; }

static inline int tqt_iround(double v) {
    if (v >= 0.0) return (int)(v + 0.5);
    return (int)(v - 0.5);
}

static inline double tqt_clamp(double x, double a, double b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

static inline TQColor tqt_toggle_solid_fg_color_(const TQColor& bg)
{
    const int y = (bg.red() * 30 + bg.green() * 59 + bg.blue() * 11) / 100;
    if (y > 200) return TQColor(0, 0, 0);
    return TQColor(255, 255, 255);
}

TQtToggleSwitch::TQtToggleSwitch(TQWidget* parent, const char* name)
    : TQWidget(parent, name),
      m_mode((int)BorderTraceRounded),
      m_checked(0),
      m_fg(TQColor(255, 255, 255)),
      m_on(TQColor(30, 144, 255)),
      m_off(TQColor(192, 192, 192)),
      m_bd(TQColor(30, 144, 255)),
      m_bdSize(2),
      m_animDuration(180),
      m_easing((int)OutCirc),
      m_prog(0.0),
      m_progStart(0.0),
      m_progTarget(0.0),
      m_animStart(),
      m_animating(0),
      m_pressed(0),
      m_moved(0),
      m_dragging(0),
      m_moveTargetState(0),
      m_prevX(0),
      m_pressPos(),
      m_slideLeft(0.0),
      m_slideRight(1.0),
      m_timer(this) {
    setBackgroundMode(PaletteBackground);
    setMinimumSize(96, 24);
    setSizePolicy(TQSizePolicy::Fixed, TQSizePolicy::Fixed);

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onAnimTick()));
    m_timer.start(15);

    calculateGeometry();
}

TQtToggleSwitch::TQtToggleSwitch(int mode, TQWidget* parent, const char* name)
    : TQWidget(parent, name),
      m_mode((int)BorderTraceRounded),
      m_checked(0),
      m_fg(TQColor(255, 255, 255)),
      m_on(TQColor(30, 144, 255)),
      m_off(TQColor(192, 192, 192)),
      m_bd(TQColor(30, 144, 255)),
      m_bdSize(2),
      m_animDuration(180),
      m_easing((int)OutCirc),
      m_prog(0.0),
      m_progStart(0.0),
      m_progTarget(0.0),
      m_animStart(),
      m_animating(0),
      m_pressed(0),
      m_moved(0),
      m_dragging(0),
      m_moveTargetState(0),
      m_prevX(0),
      m_pressPos(),
      m_slideLeft(0.0),
      m_slideRight(1.0),
      m_timer(this) {
    setBackgroundMode(PaletteBackground);
    setMinimumSize(96, 24);
    setSizePolicy(TQSizePolicy::Fixed, TQSizePolicy::Fixed);

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onAnimTick()));
    m_timer.start(15);

    setMode(mode);
    calculateGeometry();
}

TQtToggleSwitch::TQtToggleSwitch(int mode, bool checked, TQWidget* parent, const char* name)
    : TQWidget(parent, name),
      m_mode((int)BorderTraceRounded),
      m_checked(0),
      m_fg(TQColor(255, 255, 255)),
      m_on(TQColor(30, 144, 255)),
      m_off(TQColor(192, 192, 192)),
      m_bd(TQColor(30, 144, 255)),
      m_bdSize(2),
      m_animDuration(180),
      m_easing((int)OutCirc),
      m_prog(0.0),
      m_progStart(0.0),
      m_progTarget(0.0),
      m_animStart(),
      m_animating(0),
      m_pressed(0),
      m_moved(0),
      m_dragging(0),
      m_moveTargetState(0),
      m_prevX(0),
      m_pressPos(),
      m_slideLeft(0.0),
      m_slideRight(1.0),
      m_timer(this) {
    setBackgroundMode(PaletteBackground);
    setMinimumSize(96, 24);
    setSizePolicy(TQSizePolicy::Fixed, TQSizePolicy::Fixed);

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onAnimTick()));
    m_timer.start(15);

    setMode(mode);
    setStateWithoutSignal(checked);
    calculateGeometry();
}

bool TQtToggleSwitch::isChecked() const { return m_checked != 0; }

bool TQtToggleSwitch::getState() const { return isChecked(); }

void TQtToggleSwitch::setMode(int mode) {
    if (mode < 0) mode = 0;
    if (mode > 6) mode = 6;
    m_mode = mode;
    update();
}

int TQtToggleSwitch::mode() const { return m_mode; }

void TQtToggleSwitch::setSuitableHeight(int h) {
    if (h < 8) h = 8;
    setFixedSize(h * 3, h);
    calculateGeometry();
}

void TQtToggleSwitch::setForeground(const TQColor& c) { m_fg = c; update(); }

void TQtToggleSwitch::setBackground(const TQColor& on, const TQColor& off) {
    m_on = on;
    m_off = off;
    update();
}

void TQtToggleSwitch::setBorder(const TQColor& c, int size) {
    m_bd = c;
    if (size < 0) size = 0;
    m_bdSize = size;
    calculateGeometry();
    update();
}

void TQtToggleSwitch::setAnimationDuration(int ms) {
    if (ms < 0) ms = 0;
    m_animDuration = ms;
}

void TQtToggleSwitch::setAnimationEasing(int easing) {
    if (easing < 0) easing = 0;
    if (easing > 2) easing = 2;
    m_easing = easing;
}

const TQColor& TQtToggleSwitch::foregroundColor() const { return m_fg; }
const TQColor& TQtToggleSwitch::onColor() const { return m_on; }
const TQColor& TQtToggleSwitch::offColor() const { return m_off; }
const TQColor& TQtToggleSwitch::borderColor() const { return m_bd; }
int TQtToggleSwitch::borderSize() const { return m_bdSize; }

TQSize TQtToggleSwitch::minimumSizeHint() const { return TQSize(96, 24); }

TQSize TQtToggleSwitch::sizeHint() const {
    const int h = height();
    if (h > 0) return TQSize(h * 3, h);
    return minimumSizeHint();
}

void TQtToggleSwitch::setState(bool state) {
    const int ns = state ? 1 : 0;
    const int changed = (ns != m_checked);
    setStateWithoutSignal(state);
    if (changed) emit stateChanged(isChecked());
}

void TQtToggleSwitch::setStateWithoutSignal(bool state) {
    m_checked = state ? 1 : 0;
    startSwitchAnimation();
}

void TQtToggleSwitch::toggleState() {
    toggleStateWithoutSignal();
    emit stateChanged(isChecked());
}

void TQtToggleSwitch::toggleStateWithoutSignal() {
    setStateWithoutSignal(!isChecked());
}

void TQtToggleSwitch::resizeEvent(TQResizeEvent* e) {
    TQWidget::resizeEvent(e);
    calculateGeometry();
}

void TQtToggleSwitch::mousePressEvent(TQMouseEvent* e) {
    if (e->button() == TQt::LeftButton) {
        m_pressPos = e->pos();
        m_moved = 0;
        m_dragging = 0;
        m_prevX = m_pressPos.x();
        m_pressed = 1;
        e->accept();
        return;
    }
    TQWidget::mousePressEvent(e);
}

void TQtToggleSwitch::mouseMoveEvent(TQMouseEvent* e) {
    if (e->state() & TQt::LeftButton) {
        if (!m_moved) {
            if ((e->pos() - m_pressPos).manhattanLength() > TQApplication::startDragDistance()) m_moved = 1;
        }

        if (m_moved) {
            const int x = e->pos().x();
            if (x <= (int)m_slideLeft) setProgressManual(0.0);
            else if (x >= (int)m_slideRight) setProgressManual(1.0);
            else setProgressManual(((double)x - m_slideLeft) / (m_slideRight - m_slideLeft));

            m_moveTargetState = (x > m_prevX) ? 1 : 0;
            m_prevX = x;
            m_dragging = 1;
        }

        e->accept();
        return;
    }

    TQWidget::mouseMoveEvent(e);
}

void TQtToggleSwitch::mouseReleaseEvent(TQMouseEvent* e) {
    if (e->button() == TQt::LeftButton) {
        const int old = m_checked;

        if (!m_moved || (e->pos() - m_pressPos).manhattanLength() < 12) {
            toggleState();
        } else {
            const double total = (m_slideRight - m_slideLeft);
            const double stick = total * 0.15;
            const int x = e->pos().x();

            if ((double)x <= m_slideLeft + stick) m_checked = 0;
            else if ((double)x >= m_slideRight - stick) m_checked = 1;
            else m_checked = m_moveTargetState;

            if (m_checked != old) emit stateChanged(isChecked());

            startSwitchAnimation();
        }

        m_pressed = 0;

        e->accept();
        return;
    }

    TQWidget::mouseReleaseEvent(e);
}

void TQtToggleSwitch::paintEvent(TQPaintEvent*) {
    TQPainter p(this);

    p.fillRect(rect(), colorGroup().brush(TQColorGroup::Background));

    const int w = width();
    const int h = height();
    if (w <= 2 || h <= 2) return;

    const double prog = clamp01(m_prog);

    const int bd = (m_bdSize < 0) ? 0 : m_bdSize;
    const int bx = (bd / 2) + 1;
    const int by = (bd / 2) + 1;
    const int bw = w - (bd + 2);
    const int bh = h - (bd + 2);
    if (bw <= 2 || bh <= 2) return;

    const int rr = (bh > 1) ? ((bh - 1) / 2) : 0;

    const TQRect body(bx, by, bw, bh);

    TQRect bodyR = body;
    int rrR = rr;
    if (m_mode == (int)BorderTraceRounded) {
        if (bodyR.bottom() < (h - 1)) bodyR.setBottom(bodyR.bottom() + 1);
        rrR = (bodyR.height() > 1) ? ((bodyR.height() - 1) / 2) : 0;
    }

    const TQColor bg = blendedBgColor(prog);
    const TQColor textDim(160, 160, 160);

    if (m_mode == (int)BorderTraceRounded || m_mode == (int)BorderTraceRect || m_mode == (int)BorderTraceRectFlat || m_mode == (int)BorderSolidRounded || m_mode == (int)BorderSolidRoundedNoText || m_mode == (int)BorderSolidSoftRectNoText || m_mode == (int)BorderSolidVerticalMarks) {
        p.setPen(NoPen);
        p.setBrush(NoBrush);

        if (m_mode == (int)BorderSolidRounded || m_mode == (int)BorderSolidRoundedNoText) {
            // Same rounded-rect geometry as BorderTraceRounded (pill outline),
            // but slightly narrower.
            TQRect sb(0, 0, w, h);
            if (sb.width() < 2 || sb.height() < 2) return;

            {
                int newW = (w * 5) / 8;
                const int minW = h + 2;
                if (newW < minW) newW = minW;
                const int x0 = (w - newW) / 2;
                sb = TQRect(x0, 0, newW, h);
                if (sb.width() < 2 || sb.height() < 2) sb = body;
            }

            const int r = (sb.height() > 1) ? ((sb.height() - 1) / 2) : 0;
            if (r <= 0) return;

            const int margin = (m_mode == (int)BorderSolidRoundedNoText) ? 2 : 4;
            const int kr = r - margin;

            // Fill pill as 2 caps + center rect (same construction as AA version).
            p.setPen(NoPen);
            p.setBrush(bg);
            const int cy0 = sb.top() + r;
            const int cxL = sb.left() + r;
            const int cxR = sb.right() - r;
            p.drawEllipse(cxL - r, cy0 - r, r * 2 + 1, r * 2 + 1);
            p.drawEllipse(cxR - r, cy0 - r, r * 2 + 1, r * 2 + 1);
            if (cxR > cxL) p.drawRect(cxL, sb.top(), (cxR - cxL) + 1, sb.height());

            if (kr > 0) {
                const int cx0 = sb.left() + r;
                const int cx1 = sb.right() - r;
                const int cx = cx0 + tqt_iround((double)(cx1 - cx0) * prog);
                const TQColor fg = (m_mode == (int)BorderSolidRoundedNoText) ? TQColor(255, 255, 255) : tqt_toggle_solid_fg_color_(bg);
                p.setPen(NoPen);
                p.setBrush(fg);
                p.drawEllipse(cx - kr, cy0 - kr, kr * 2 + 1, kr * 2 + 1);
            }

            if (m_mode != (int)BorderSolidRoundedNoText) {
                p.setPen(tqt_toggle_solid_fg_color_(bg));
                if (m_checked) {
                    const TQRect tr(sb.x(), sb.y(), sb.width() / 2, sb.height());
                    p.drawText(tr, TQt::AlignCenter, "ON");
                } else {
                    const TQRect tr(sb.x() + sb.width() / 2, sb.y(), sb.width() - sb.width() / 2, sb.height());
                    p.drawText(tr, TQt::AlignCenter, "OFF");
                }
            }
            return;
        }

        if (m_mode == (int)BorderSolidSoftRectNoText) {
            TQRect sb(0, 0, w, h);
            if (sb.width() < 2 || sb.height() < 2) return;

            {
                int newW = (w * 5) / 8;
                const int minW = h + 2;
                if (newW < minW) newW = minW;
                const int x0 = (w - newW) / 2;
                sb = TQRect(x0, 0, newW, h);
                if (sb.width() < 2 || sb.height() < 2) sb = body;
            }

            const int ph = sb.height();
            if (ph <= 2) return;

            const int margin = 2;
            int side = ph - (margin * 2);
            if (side < 2) side = 2;

            int pr = ph / 6;
            if (pr < 2) pr = 2;
            if (pr > (ph / 2)) pr = ph / 2;

            int kr = pr;
            if (kr < 2) kr = 2;
            if (kr > (side / 2)) kr = side / 2;

            const int knobW = side;
            const int knobH = side;
            const int y0 = sb.top() + margin;
            const int xMin = sb.left() + margin;
            const int xMax = sb.right() - margin - knobW + 1;
            int xk = xMin + tqt_iround((double)(xMax - xMin) * prog);
            if (xk < xMin) xk = xMin;
            if (xk > xMax) xk = xMax;

            p.setPen(NoPen);
            p.setBrush(bg);
            {
                int rr = pr;
                const int rmax = ((sb.width() < sb.height()) ? sb.width() : sb.height()) / 2;
                if (rr < 0) rr = 0;
                if (rr > rmax) rr = rmax;

                if (rr <= 0) {
                    p.drawRect(sb);
                } else {
                    const int x0 = sb.left();
                    const int y0 = sb.top();
                    const int x1 = sb.right();
                    const int y1 = sb.bottom();
                    p.drawEllipse(x0, y0, rr * 2, rr * 2);
                    p.drawEllipse(x1 - rr * 2 + 1, y0, rr * 2, rr * 2);
                    p.drawEllipse(x1 - rr * 2 + 1, y1 - rr * 2 + 1, rr * 2, rr * 2);
                    p.drawEllipse(x0, y1 - rr * 2 + 1, rr * 2, rr * 2);

                    if (sb.width() > rr * 2) {
                        p.drawRect(x0 + rr, y0, sb.width() - rr * 2, sb.height());
                    }
                    if (sb.height() > rr * 2) {
                        p.drawRect(x0, y0 + rr, sb.width(), sb.height() - rr * 2);
                    }
                }
            }

            p.setPen(NoPen);
            p.setBrush(TQColor(255, 255, 255));
            {
                const TQRect kb(xk, y0, knobW, knobH);
                int rr = kr;
                const int rmax = ((kb.width() < kb.height()) ? kb.width() : kb.height()) / 2;
                if (rr < 0) rr = 0;
                if (rr > rmax) rr = rmax;

                if (rr <= 0) {
                    p.drawRect(kb);
                } else {
                    const int x0 = kb.left();
                    const int y0 = kb.top();
                    const int x1 = kb.right();
                    const int y1 = kb.bottom();
                    p.drawEllipse(x0, y0, rr * 2, rr * 2);
                    p.drawEllipse(x1 - rr * 2 + 1, y0, rr * 2, rr * 2);
                    p.drawEllipse(x1 - rr * 2 + 1, y1 - rr * 2 + 1, rr * 2, rr * 2);
                    p.drawEllipse(x0, y1 - rr * 2 + 1, rr * 2, rr * 2);

                    if (kb.width() > rr * 2) {
                        p.drawRect(x0 + rr, y0, kb.width() - rr * 2, kb.height());
                    }
                    if (kb.height() > rr * 2) {
                        p.drawRect(x0, y0 + rr, kb.width(), kb.height() - rr * 2);
                    }
                }
            }
            return;
        }

        if (m_mode == (int)BorderSolidVerticalMarks) {
            TQRect sb(0, 0, w, h);
            if (sb.width() < 2 || sb.height() < 2) return;

            const int r = (sb.width() > 1) ? ((sb.width() - 1) / 2) : 0;
            if (r <= 0) return;

            const int margin = 2;
            const int kr = r - margin;

            const int cx0 = sb.left() + r;
            const int cyT = sb.top() + r;
            const int cyB = sb.bottom() - r;

            // Pill fill: 2 caps + center rect.
            p.setPen(NoPen);
            p.setBrush(bg);
            p.drawEllipse(cx0 - r, cyT - r, r * 2 + 1, r * 2 + 1);
            p.drawEllipse(cx0 - r, cyB - r, r * 2 + 1, r * 2 + 1);
            if (cyB > cyT) p.drawRect(sb.left(), cyT, sb.width(), (cyB - cyT) + 1);

            // Markers: top line (ON) + bottom circle outline (OFF).
            {
                const TQColor mk(255, 255, 255);
                const int lw = (r > 10) ? 3 : 2;
                const int lineLen = sb.width() / 2;
                const int x0 = cx0 - lineLen / 2;
                const int x1 = cx0 + lineLen / 2;
                const int yLine = sb.top() + (r * 5) / 6 + 2;
                TQPen pen(mk, lw);
                pen.setCapStyle(TQt::RoundCap);
                p.setPen(pen);
                p.drawLine(x0, yLine, x1, yLine);

                const int orr = (r * 3) / 8;
                const int ocy = sb.bottom() - (r * 3) / 4 - 2;
                if (orr > 1) {
                    p.setPen(TQPen(mk, lw));
                    p.setBrush(NoBrush);
                    p.drawEllipse(cx0 - orr, ocy - orr, orr * 2, orr * 2);
                }
            }

            if (kr > 0) {
                const int cy = cyT + tqt_iround((double)(cyB - cyT) * prog);
                p.setPen(NoPen);
                p.setBrush(TQColor(255, 255, 255));
                p.drawEllipse(cx0 - kr, cy - kr, kr * 2 + 1, kr * 2 + 1);
            }
            return;
        }

        if (m_mode == (int)BorderTraceRounded) {
            const double prop = prog;
            const int inset = bd + 1;
            TQRect inner = bodyR;
            inner.addCoords(inset, inset, -inset, -inset);
            if (inner.width() < 2 || inner.height() < 2) inner = body;

            const double margin = 0.0;
            const double maxDelta = ((double)inner.width() - margin * 2.0) * 0.5;

            const double left = (double)inner.left() + margin + maxDelta * ((prop > 0.5 ? prop : 0.5) - 0.5) / 0.5;
            const double right = (double)inner.right() - margin - maxDelta * (0.5 - (prop < 0.5 ? prop : 0.5)) / 0.5;

            TQRect fg(tqt_iround(left), inner.top(), tqt_iround(right - left), inner.height());
            if (fg.width() > 0 && fg.height() > 0) {
                p.setPen(NoPen);
                p.setBrush(bg);
                const int th = fg.height();
                const int r = (th > 1) ? ((th - 1) / 2) : 0;
                if (r > 0) {
                    const int cy0 = fg.top() + r;
                    const int cx0 = fg.left() + r;
                    const int cx1 = fg.right() - r;
                    p.drawEllipse(cx0 - r, cy0 - r, r * 2, r * 2);
                    p.drawEllipse(cx1 - r, cy0 - r, r * 2, r * 2);
                    if (cx1 > cx0) p.drawRect(cx0, fg.top(), (cx1 - cx0), fg.height());
                }
            }
        } else if (m_mode == (int)BorderTraceRect) {
            const double prop = prog;
            const int inset = bd + 1;
            TQRect inner = body;
            inner.addCoords(inset, inset, -inset, -inset);
            if (inner.width() < 2 || inner.height() < 2) inner = body;

            const double fixedX = (double)bd + 2.0;
            const double hw = (double)inner.width() * 0.5;
            const double left = (double)inner.left() + hw * prop;
            const double right = left + hw;
            const double topLeft = left - fixedX * prop;
            const double bottomLeft = left + fixedX * prop;
            const double topRight = right + fixedX * (1.0 - prop);
            const double bottomRight = right - fixedX * (1.0 - prop);

            TQPointArray pts(4);
            pts.setPoint(0, tqt_iround(topLeft), inner.top());
            pts.setPoint(1, tqt_iround(bottomLeft), inner.bottom());
            pts.setPoint(2, tqt_iround(bottomRight), inner.bottom());
            pts.setPoint(3, tqt_iround(topRight), inner.top());

            p.setPen(NoPen);
            p.setBrush(bg);
            p.drawPolygon(pts);
        } else {
            const double prop = prog;
            const int inset = bd + 1;
            TQRect inner = body;
            inner.addCoords(inset, inset, -inset, -inset);
            if (inner.width() < 2 || inner.height() < 2) inner = body;

            const int hw = inner.width() / 2;
            const int left = inner.left() + tqt_iround((double)hw * prop);
            TQRect fg(left, inner.top(), hw, inner.height());
            if (fg.width() > 0 && fg.height() > 0) {
                p.setPen(NoPen);
                p.setBrush(bg);
                p.drawRect(fg);
            }
        }

        if (m_bdSize > 0) {
            if (m_mode == (int)BorderTraceRect || m_mode == (int)BorderTraceRectFlat || m_mode != (int)BorderTraceRounded) {
                TQPen penOff(m_off, m_bdSize);
                penOff.setCapStyle(TQt::RoundCap);
                penOff.setJoinStyle(TQt::RoundJoin);
                p.setPen(penOff);
                p.setBrush(NoBrush);
                if (m_mode == (int)BorderTraceRect || m_mode == (int)BorderTraceRectFlat) p.drawRect(body);
                else p.drawRoundRect(body, 1000, 1000);
            }
        }

        if (m_bdSize > 0) {
            const TQColor onTrace = (m_mode == (int)BorderTraceRounded) ? m_bd : m_on;
            if (onTrace != m_off) {
                TQPen penOff(m_off, m_bdSize);
                penOff.setCapStyle(TQt::RoundCap);
                penOff.setJoinStyle(TQt::RoundJoin);
                TQPen penOn(onTrace, m_bdSize);
                penOn.setCapStyle(TQt::RoundCap);
                penOn.setJoinStyle(TQt::RoundJoin);

                p.setPen(penOn);

                p.save();
                p.setClipRect((m_mode == (int)BorderTraceRounded) ? bodyR : body);

                if (m_mode == (int)BorderTraceRect || m_mode == (int)BorderTraceRectFlat) {
                    const double w0 = (double)body.width();
                    const double h0 = (double)body.height();
                    const double total = w0 * 2.0 + h0 * 2.0;
                    double curr = total * prog;

                    int x0 = body.left();
                    int y0 = body.top();
                    int x1 = body.right();
                    int y1 = body.bottom();

                    if (curr > 0.0) {
                        double len = h0;
                        double mv = (curr > len) ? len : curr;
                        p.drawLine(x0, y0, x0, y0 + tqt_iround(mv));
                        curr -= mv;
                    }
                    if (curr > 0.0) {
                        double len = w0;
                        double mv = (curr > len) ? len : curr;
                        p.drawLine(x0, y1, x0 + tqt_iround(mv), y1);
                        curr -= mv;
                    }
                    if (curr > 0.0) {
                        double len = h0;
                        double mv = (curr > len) ? len : curr;
                        p.drawLine(x1, y1, x1, y1 - tqt_iround(mv));
                        curr -= mv;
                    }
                    if (curr > 0.0) {
                        double len = w0;
                        double mv = (curr > len) ? len : curr;
                        p.drawLine(x1, y0, x1 - tqt_iround(mv), y0);
                        curr -= mv;
                    }
                } else if (m_mode == (int)BorderTraceRounded) {
                    const int r = rrR;
                    if (r > 0) {
                        const double rad = (double)r;
                        const double w0 = (double)bodyR.width();
                        const double total = 2.0 * 3.14159265358979323846 * rad + 2.0 * (w0 - 2.0 * rad);
                        double curr;

                        const int xL = bodyR.left();
                        const int xR = bodyR.right();
                        const int yT = bodyR.top();
                        const int yB = yT + (r * 2);

                        const int ax = xL;
                        const int ay = yT;
                        const int aw = r * 2 + 1;
                        const int ah = r * 2 + 1;

                        // OFF trace at 100% (same geometry as ON trace).
                        p.setPen(penOff);
                        p.setBrush(NoBrush);
                        curr = total;
                        if (curr > 0.0) {
                            const double len = rad * 3.14159265358979323846 * 0.5;
                            const double mv = (curr > len) ? len : curr;
                            const double ang = 90.0 * (mv / len);
                            p.drawArc(TQRect(ax, ay, aw, ah), 180 * 16, (int)(ang * 16.0));
                            curr -= mv;
                        }
                        if (curr > 0.0) {
                            const double len = (w0 - 2.0 * rad);
                            const double mv = (curr > len) ? len : curr;
                            p.drawLine(xL + r, yB, xL + r + tqt_iround(mv), yB);
                            curr -= mv;
                        }
                        if (curr > 0.0) {
                            const double len = rad * 3.14159265358979323846;
                            const double mv = (curr > len) ? len : curr;
                            const double ang = 180.0 * (mv / len);
                            p.drawArc(TQRect(xR - aw + 1, ay, aw, ah), 270 * 16, (int)(ang * 16.0));
                            curr -= mv;
                        }
                        if (curr > 0.0) {
                            const double len = (w0 - 2.0 * rad);
                            const double mv = (curr > len) ? len : curr;
                            p.drawLine(xR - r, yT, xR - r - tqt_iround(mv), yT);
                            curr -= mv;
                        }
                        if (curr > 0.0) {
                            const double len = rad * 3.14159265358979323846 * 0.5;
                            const double mv = (curr > len) ? len : curr;
                            const double ang = 90.0 * (mv / len);
                            p.drawArc(TQRect(ax, ay, aw, ah), 90 * 16, (int)(ang * 16.0));
                            curr -= mv;
                        }

                        // ON trace at prog.
                        p.setPen(penOn);
                        curr = total * prog;
                        if (curr > 0.0) {
                            const double len = rad * 3.14159265358979323846 * 0.5;
                            const double mv = (curr > len) ? len : curr;
                            const double ang = 90.0 * (mv / len);
                            p.drawArc(TQRect(ax, ay, aw, ah), 180 * 16, (int)(ang * 16.0));
                            curr -= mv;
                        }
                        if (curr > 0.0) {
                            const double len = (w0 - 2.0 * rad);
                            const double mv = (curr > len) ? len : curr;
                            p.drawLine(xL + r, yB, xL + r + tqt_iround(mv), yB);
                            curr -= mv;
                        }
                        if (curr > 0.0) {
                            const double len = rad * 3.14159265358979323846;
                            const double mv = (curr > len) ? len : curr;
                            const double ang = 180.0 * (mv / len);
                            p.drawArc(TQRect(xR - aw + 1, ay, aw, ah), 270 * 16, (int)(ang * 16.0));
                            curr -= mv;
                        }
                        if (curr > 0.0) {
                            const double len = (w0 - 2.0 * rad);
                            const double mv = (curr > len) ? len : curr;
                            p.drawLine(xR - r, yT, xR - r - tqt_iround(mv), yT);
                            curr -= mv;
                        }
                        if (curr > 0.0) {
                            const double len = rad * 3.14159265358979323846 * 0.5;
                            const double mv = (curr > len) ? len : curr;
                            const double ang = 90.0 * (mv / len);
                            p.drawArc(TQRect(ax, ay, aw, ah), 90 * 16, (int)(ang * 16.0));
                            curr -= mv;
                        }
                    }
                }

                p.restore();
            }
        }

        const TQRect leftRect(body.x(), body.y(), body.width() / 2, body.height());
        const TQRect rightRect(body.x() + body.width() / 2, body.y(), body.width() - body.width() / 2, body.height());

        p.setPen(textDim);
        p.drawText(leftRect, TQt::AlignCenter, "OFF");
        p.drawText(rightRect, TQt::AlignCenter, "ON");

        p.save();
        if (m_mode == (int)BorderTraceRounded) {
            const double prop = prog;
            const int inset = bd + 1;
            TQRect inner = body;
            inner.addCoords(inset, inset, -inset, -inset);
            if (inner.width() < 2 || inner.height() < 2) inner = body;

            const double margin = 0.0;
            const double maxDelta = ((double)inner.width() - margin * 2.0) * 0.5;
            const double left = (double)inner.left() + margin + maxDelta * ((prop > 0.5 ? prop : 0.5) - 0.5) / 0.5;
            const double right = (double)inner.right() - margin - maxDelta * (0.5 - (prop < 0.5 ? prop : 0.5)) / 0.5;
            TQRect clip(tqt_iround(left), inner.top(), tqt_iround(right - left), inner.height());
            p.setClipRect(clip);
        } else if (m_mode == (int)BorderTraceRect) {
            const double prop = prog;
            const double fixedX = (double)bd + 2.0;
            const int inset = bd + 1;
            TQRect inner = body;
            inner.addCoords(inset, inset, -inset, -inset);
            if (inner.width() < 2 || inner.height() < 2) inner = body;

            const double hw = (double)inner.width() * 0.5;
            const double left = (double)inner.left() + hw * prop;
            const double right = left + hw;
            const double topLeft = left - fixedX * prop;
            const double bottomLeft = left + fixedX * prop;
            const double topRight = right + fixedX * (1.0 - prop);
            const double bottomRight = right - fixedX * (1.0 - prop);

            TQPointArray pts(4);
            pts.setPoint(0, tqt_iround(topLeft), inner.top());
            pts.setPoint(1, tqt_iround(bottomLeft), inner.bottom());
            pts.setPoint(2, tqt_iround(bottomRight), inner.bottom());
            pts.setPoint(3, tqt_iround(topRight), inner.top());
            p.setClipRegion(TQRegion(pts));
        } else {
            const double prop = prog;
            const int inset = bd + 1;
            TQRect inner = body;
            inner.addCoords(inset, inset, -inset, -inset);
            if (inner.width() < 2 || inner.height() < 2) inner = body;

            const int hw = inner.width() / 2;
            const int left = inner.left() + tqt_iround((double)hw * prop);
            TQRect clip(left, inner.top(), hw, inner.height());
            p.setClipRect(clip);
        }

        p.setPen(m_fg);
        p.drawText(leftRect, TQt::AlignCenter, "OFF");
        p.drawText(rightRect, TQt::AlignCenter, "ON");
        p.restore();

        return;
    }
}

void TQtToggleSwitch::calculateGeometry() {
    const int w = width();
    const int h = height();
    const double bs = (double)m_bdSize;

    m_slideLeft = bs * 0.5;
    m_slideRight = (double)w - bs * 0.5;
}

void TQtToggleSwitch::startSwitchAnimation() {
    m_progStart = m_prog;
    m_progTarget = m_checked ? 1.0 : 0.0;
    m_animStart = TQTime::currentTime();
    m_animating = 1;
}

void TQtToggleSwitch::setProgressManual(double p) {
    m_prog = clamp01(p);
    update();
}

TQColor TQtToggleSwitch::blendedBgColor(double p) const {
    p = clamp01(p);
    const int r = (int)((double)m_off.red() + ((double)m_on.red() - (double)m_off.red()) * p);
    const int g = (int)((double)m_off.green() + ((double)m_on.green() - (double)m_off.green()) * p);
    const int b = (int)((double)m_off.blue() + ((double)m_on.blue() - (double)m_off.blue()) * p);
    return TQColor(r, g, b);
}

double TQtToggleSwitch::ease(int easing, double t) {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;

    switch (easing) {
        case (int)OutCirc: {
            const double u = t - 1.0;
            return sqrt(1.0 - u * u);
        }
        case (int)OutBack: {
            const double c1 = 1.70158;
            const double c3 = c1 + 1.0;
            const double u = t - 1.0;
            return 1.0 + c3 * u * u * u + c1 * u * u;
        }
        case (int)InOutCubic:
        default:
            if (t < 0.5) return 4.0 * t * t * t;
            return 1.0 - pow(-2.0 * t + 2.0, 3.0) * 0.5;
    }
}

void TQtToggleSwitch::onAnimTick() {
    int dirty = 0;

    if (m_animating) {
        const int dt = m_animStart.msecsTo(TQTime::currentTime());
        double t = (m_animDuration <= 0) ? 1.0 : ((double)dt / (double)m_animDuration);
        if (t >= 1.0) {
            t = 1.0;
            m_animating = 0;
        }

        const double e = ease(m_easing, t);
        m_prog = m_progStart + (m_progTarget - m_progStart) * e;
        dirty = 1;
    }

    if (dirty) update();
}
