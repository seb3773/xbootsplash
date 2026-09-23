#include "tqtmessagelogwidget.h"

#include <ntqapplication.h>
#include <ntqclipboard.h>
#include <ntqfontmetrics.h>
#include <ntqpainter.h>
#include <ntqevent.h>
#include <ntqpopupmenu.h>

#include <string.h>

// CPU-bound, but typically small volumes.

// Complexity: O(1)
TQtMessageLogWidget::TQtMessageLogWidget(TQWidget* parent, const char* name)
    : TQScrollView(parent, name),
      m_lines(0),
      m_count(0),
      m_capacity(0),
      m_head(0),
      m_styles(0),
      m_styleCount(0),
      m_styleCapacity(0),
      m_historySize(0xFFFFFFFFu),
      m_minVisibleLines(1),
      m_minVisibleCols(1),
      m_alternating(0),
      m_lineSpacing(0),
      m_ascent(0),
      m_avgCharW(0),
      m_longestPx(0),
      m_autoScroll(1),
      m_selecting(0),
      m_selA(-1),
      m_selB(-1)
{
    setResizePolicy(TQScrollView::Manual);
    setHScrollBarMode(TQScrollView::Auto);
    setVScrollBarMode(TQScrollView::Auto);

    viewport()->setBackgroundMode(TQt::PaletteBase);

    updateMetrics_();
    updateContentsSize_();
}

// Complexity: O(n)
TQtMessageLogWidget::~TQtMessageLogWidget()
{
    if (m_lines) {
        delete[] m_lines;
        m_lines = 0;
    }
    if (m_styles) {
        delete[] m_styles;
        m_styles = 0;
    }
}

// Complexity: O(1)
void TQtMessageLogWidget::setHistorySize(unsigned int size)
{
    m_historySize = size;
    enforceHistory_();
    updateContentsSize_();
    updateContents();
}

// Complexity: O(1)
void TQtMessageLogWidget::setAutoScroll(bool on)
{
    m_autoScroll = on ? 1 : 0;
    if (m_autoScroll)
        scrollToBottom();
}

// Complexity: O(1)
bool TQtMessageLogWidget::autoScroll() const
{
    return m_autoScroll ? true : false;
}

// Complexity: O(1)
void TQtMessageLogWidget::scrollToBottom()
{
    if (verticalScrollBar())
        verticalScrollBar()->setValue(verticalScrollBar()->maxValue());
}

// Complexity: O(1)
unsigned int TQtMessageLogWidget::historySize() const
{
    return m_historySize;
}

// Complexity: O(1)
void TQtMessageLogWidget::setMinimumVisibleLines(unsigned int num)
{
    if (num < 1) num = 1;
    m_minVisibleLines = num;
    updateContentsSize_();
    updateContents();
}

// Complexity: O(1)
unsigned int TQtMessageLogWidget::minimumVisibleLines() const
{
    return m_minVisibleLines;
}

// Complexity: O(1)
void TQtMessageLogWidget::setMinimumVisibleColumns(unsigned int num)
{
    if (num < 1) num = 1;
    m_minVisibleCols = num;
    updateContentsSize_();
    updateContents();
}

// Complexity: O(1)
unsigned int TQtMessageLogWidget::minimumVisibleColumns() const
{
    return m_minVisibleCols;
}

// Complexity: O(1)
void TQtMessageLogWidget::setAlternatingRowColors(bool on)
{
    m_alternating = on ? 1 : 0;
    updateContents();
}

// Complexity: O(1)
bool TQtMessageLogWidget::alternatingRowColors() const
{
    return m_alternating ? true : false;
}

// Complexity: O(k)
int TQtMessageLogWidget::setupStyle(const TQRegExp& expr, const TQColor& background, const TQColor& foreground)
{
    if (m_styleCount >= m_styleCapacity) {
        unsigned int newCap = (m_styleCapacity ? (m_styleCapacity * 2) : 8);
        Style* ns = new Style[newCap];
        if (m_styles && m_styleCount) {
            for (unsigned int i = 0; i < m_styleCount; ++i) {
                ns[i] = m_styles[i];
            }
        }
        if (m_styles) delete[] m_styles;
        m_styles = ns;
        m_styleCapacity = newCap;
    }

    Style& st = m_styles[m_styleCount];
    st.expr = expr;
    st.bg = background;
    st.fg = foreground;
    st.bgSet = background.isValid() ? 1 : 0;
    st.fgSet = foreground.isValid() ? 1 : 0;

    ++m_styleCount;
    return (int)m_styleCount;
}

// Complexity: O(k)
int TQtMessageLogWidget::removeStyle(int id)
{
    if (id <= 0) return 0;
    const unsigned int idx = (unsigned int)(id - 1);
    if (idx >= m_styleCount) return 0;

    for (unsigned int i = idx + 1; i < m_styleCount; ++i) {
        m_styles[i - 1] = m_styles[i];
    }
    --m_styleCount;

    for (unsigned int i = 0; i < m_count; ++i) {
        const int sid = m_lines[i].styleId;
        if (sid == id) m_lines[i].styleId = 0;
        else if (sid > id) m_lines[i].styleId = sid - 1;
    }

    updateContents();
    return 1;
}

// Complexity: O(1)
void TQtMessageLogWidget::clearStyles()
{
    m_styleCount = 0;
    for (unsigned int i = 0; i < m_count; ++i) {
        Line* ln = lineAt_(i);
        if (ln) ln->styleId = 0;
    }
    updateContents();
}

// Complexity: O(n)
TQString TQtMessageLogWidget::text() const
{
    TQString out;
    for (unsigned int i = 0; i < m_count; ++i) {
        const Line* ln = lineAt_(i);
        if (!ln) continue;
        out += ln->text;
        if (i + 1 < m_count) out += "\n";
    }
    return out;
}

// Complexity: O(n)
void TQtMessageLogWidget::clear()
{
    m_count = 0;
    m_longestPx = 0;
    m_head = 0;

    m_selecting = 0;
    m_selA = -1;
    m_selB = -1;

    updateContentsSize_();
    updateContents();
}

// Complexity: amortized O(1)
void TQtMessageLogWidget::message(const TQString& msg)
{
    if (m_count >= m_capacity) {
        unsigned int newCap = (m_capacity ? (m_capacity * 2) : 128);
        Line* nl = new Line[newCap];

        for (unsigned int i = 0; i < m_count; ++i) {
            const Line* src = lineAt_(i);
            if (src) nl[i] = *src;
        }
        if (m_lines) delete[] m_lines;
        m_lines = nl;
        m_capacity = newCap;
        m_head = 0;
    }

    const unsigned int idx = (m_capacity ? ((m_head + m_count) % m_capacity) : 0);
    Line& ln = m_lines[idx];
    ln.text = msg;
    ln.styleId = highlightStyle_(msg);

    ++m_count;

    enforceHistory_();

    const int w = fontMetrics().width(msg);
    if (w > m_longestPx) m_longestPx = w;

    updateContentsSize_();

    if (m_autoScroll)
        scrollToBottom();

    updateContents();
}

// Complexity: O(visible lines)
void TQtMessageLogWidget::drawContents(TQPainter* p, int cx, int cy, int cw, int ch)
{
    if (!p) return;

    if (m_lineSpacing <= 0) updateMetrics_();

    const int lineH = m_lineSpacing;
    if (lineH <= 0) return;

    const int first = cy / lineH;
    int last = (cy + ch) / lineH;
    if ((cy + ch) % lineH) ++last;

    const int padX = 4;

    const TQColor baseBg = viewport()->colorGroup().base();
    const TQColor altBg = viewport()->colorGroup().background();
    const TQColor selBg(80, 120, 200);
    const TQColor selFg(255, 255, 255);

    if (first < 0) return;

    for (int i = first; i <= last; ++i) {
        const int y = i * lineH;
        const TQRect r(cx, y, cw, lineH);

        if (i < 0 || (unsigned int)i >= m_count) {
            p->fillRect(r, baseBg);
            continue;
        }

        const Line* lnp = lineAt_((unsigned int)i);
        if (!lnp) {
            p->fillRect(r, baseBg);
            continue;
        }
        const Line& ln = *lnp;

        int usedAlt = 0;

        if (m_alternating && (i & 1)) {
            p->fillRect(r, altBg);
            usedAlt = 1;
        } else {
            p->fillRect(r, baseBg);
        }

        if (ln.styleId > 0) {
            const unsigned int sid = (unsigned int)(ln.styleId - 1);
            if (sid < m_styleCount) {
                const Style& st = m_styles[sid];
                if (st.bgSet) {
                    p->fillRect(r, st.bg);
                    usedAlt = 0;
                }
                if (st.fgSet) p->setPen(st.fg);
                else p->setPen(viewport()->colorGroup().text());
            } else {
                p->setPen(viewport()->colorGroup().text());
            }
        } else {
            p->setPen(viewport()->colorGroup().text());
        }

        if (m_selA >= 0 && m_selB >= 0) {
            int a = m_selA;
            int b = m_selB;
            if (a > b) { int t = a; a = b; b = t; }
            if (i >= a && i <= b) {
                p->fillRect(r, selBg);
                p->setPen(selFg);
            }
        }

        (void)usedAlt;
        p->drawText(cx + padX, y + m_ascent, ln.text);
    }
}

// Complexity: O(1)
void TQtMessageLogWidget::resizeEvent(TQResizeEvent* e)
{
    TQScrollView::resizeEvent(e);
    updateMetrics_();
    updateContentsSize_();
    e = 0;
}

// Complexity: O(1)
void TQtMessageLogWidget::keyPressEvent(TQKeyEvent* e)
{
    if (!e) {
        TQScrollView::keyPressEvent(e);
        return;
    }

    if ((e->state() & ControlButton) != 0) {
        if (e->key() == Key_C) {
            if (m_selA >= 0 && m_selB >= 0)
                copySelection_();
            else
                copyAll_();
            e->accept();
            return;
        }
    }

    TQScrollView::keyPressEvent(e);
}

void TQtMessageLogWidget::contentsMousePressEvent(TQMouseEvent* e)
{
    if (!e) return;

    if (e->button() == LeftButton) {
        const int ln = lineAtContentsY_(e->y());
        if (ln >= 0 && (unsigned int)ln < m_count) {
            m_selecting = 1;
            m_selA = ln;
            m_selB = ln;
            updateContents();
            e->accept();
            return;
        }
    }

    TQScrollView::contentsMousePressEvent(e);
}

void TQtMessageLogWidget::contentsMouseMoveEvent(TQMouseEvent* e)
{
    if (!e) return;
    if (!m_selecting) {
        TQScrollView::contentsMouseMoveEvent(e);
        return;
    }

    const int ln = lineAtContentsY_(e->y());
    if (ln >= 0 && (unsigned int)ln < m_count) {
        if (m_selB != ln) {
            m_selB = ln;
            updateContents();
        }
    }
    e->accept();
}

void TQtMessageLogWidget::contentsMouseReleaseEvent(TQMouseEvent* e)
{
    if (!e) return;
    if (m_selecting && e->button() == LeftButton) {
        m_selecting = 0;
        normalizeSelection_();
        updateContents();
        e->accept();
        return;
    }

    TQScrollView::contentsMouseReleaseEvent(e);
}

void TQtMessageLogWidget::contentsContextMenuEvent(TQContextMenuEvent* e)
{
    if (!e) return;

    const int ln = lineAtContentsY_(e->y());

    TQPopupMenu m(this);
    const int idCopyLine = m.insertItem("Copy line");
    const int idCopySel  = m.insertItem("Copy selection");
    const int idCopyAll  = m.insertItem("Copy all");
    m.insertSeparator();
    const int idClear    = m.insertItem("Clear");

    if (ln < 0 || (unsigned int)ln >= m_count)
        m.setItemEnabled(idCopyLine, false);
    if (!(m_selA >= 0 && m_selB >= 0))
        m.setItemEnabled(idCopySel, false);

    const int choice = m.exec(e->globalPos());
    if (choice == idCopyLine) copyLine_(ln);
    else if (choice == idCopySel) copySelection_();
    else if (choice == idCopyAll) copyAll_();
    else if (choice == idClear) clear();

    e->accept();
}

void TQtMessageLogWidget::updateMetrics_()
{
    const TQFontMetrics fm(font());
    m_lineSpacing = fm.lineSpacing();
    m_ascent = fm.ascent();

    m_avgCharW = fm.width('M');
    if (m_avgCharW <= 0) m_avgCharW = 8;

    if (m_lineSpacing <= 0) m_lineSpacing = 14;
    if (m_ascent <= 0) m_ascent = 11;
}

void TQtMessageLogWidget::enforceHistory_()
{
    if (m_historySize == 0xFFFFFFFFu) return;
    if (m_historySize < 1) {
        m_count = 0;
        m_longestPx = 0;
        m_head = 0;
        m_selA = -1;
        m_selB = -1;
        return;
    }

    unsigned int dropped = 0;
    while (m_count > m_historySize) {
        if (m_count) {
            m_head = (m_capacity ? ((m_head + 1) % m_capacity) : 0);
            --m_count;
            ++dropped;
        } else {
            break;
        }
    }

    if (dropped) {
        if (m_selA >= 0) m_selA -= (int)dropped;
        if (m_selB >= 0) m_selB -= (int)dropped;
        if (m_selA < 0 || m_selB < 0 || m_selA >= (int)m_count || m_selB >= (int)m_count) {
            m_selA = -1;
            m_selB = -1;
            m_selecting = 0;
        }

        int longest = 0;
        for (unsigned int i = 0; i < m_count; ++i) {
            const Line* ln = lineAt_(i);
            if (!ln) continue;
            const int w = fontMetrics().width(ln->text);
            if (w > longest) longest = w;
        }
        m_longestPx = longest;
    }
}

void TQtMessageLogWidget::updateContentsSize_()
{
    if (m_lineSpacing <= 0) updateMetrics_();

    unsigned int visibleLines = m_count;
    if (visibleLines < m_minVisibleLines) visibleLines = m_minVisibleLines;

    int w = m_longestPx;
    const int minw = (int)m_minVisibleCols * m_avgCharW;
    if (w < minw) w = minw;

    w += 12;

    int h = (int)visibleLines * m_lineSpacing;
    if (h < m_lineSpacing) h = m_lineSpacing;

    if (w < 1) w = 1;
    if (h < 1) h = 1;

    resizeContents(w, h);
    updateScrollBars();
}

int TQtMessageLogWidget::highlightStyle_(const TQString& s) const
{
    if (!m_styles || !m_styleCount) return 0;

    for (unsigned int i = 0; i < m_styleCount; ++i) {
        if (m_styles[i].expr.search(s) >= 0) return (int)(i + 1);
    }
    return 0;
}

void TQtMessageLogWidget::copyAll_() const
{
    TQClipboard* cb = TQApplication::clipboard();
    if (!cb) return;
    cb->setText(text());
}

void TQtMessageLogWidget::copyLine_(int line) const
{
    if (line < 0 || (unsigned int)line >= m_count) return;
    const Line* ln = lineAt_((unsigned int)line);
    if (!ln) return;

    TQClipboard* cb = TQApplication::clipboard();
    if (!cb) return;
    cb->setText(ln->text);
}

void TQtMessageLogWidget::copySelection_() const
{
    if (m_selA < 0 || m_selB < 0) return;
    int a = m_selA;
    int b = m_selB;
    if (a > b) { int t = a; a = b; b = t; }
    if (a < 0) a = 0;
    if (b >= (int)m_count) b = (int)m_count - 1;

    TQString out;
    for (int i = a; i <= b; ++i) {
        const Line* ln = lineAt_((unsigned int)i);
        if (!ln) continue;
        out += ln->text;
        if (i != b) out += "\n";
    }

    TQClipboard* cb = TQApplication::clipboard();
    if (!cb) return;
    cb->setText(out);
}

int TQtMessageLogWidget::lineAtContentsY_(int y) const
{
    if (m_lineSpacing <= 0) return -1;
    if (y < 0) return -1;
    return y / m_lineSpacing;
}

void TQtMessageLogWidget::normalizeSelection_()
{
    if (m_selA < 0 || m_selB < 0) {
        m_selA = -1;
        m_selB = -1;
        return;
    }

    if (m_selA == m_selB) return;
    if (m_selA > m_selB) {
        const int t = m_selA;
        m_selA = m_selB;
        m_selB = t;
    }
}

const TQtMessageLogWidget::Line* TQtMessageLogWidget::lineAt_(unsigned int i) const
{
    if (!m_lines || !m_capacity) return 0;
    if (i >= m_count) return 0;
    const unsigned int idx = (m_head + i) % m_capacity;
    return &m_lines[idx];
}

TQtMessageLogWidget::Line* TQtMessageLogWidget::lineAt_(unsigned int i)
{
    if (!m_lines || !m_capacity) return 0;
    if (i >= m_count) return 0;
    const unsigned int idx = (m_head + i) % m_capacity;
    return &m_lines[idx];
}
