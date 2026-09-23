#ifndef TQT_MESSAGE_LOG_WIDGET_H
#define TQT_MESSAGE_LOG_WIDGET_H

#include <ntqscrollview.h>
#include <ntqstring.h>
#include <ntqcolor.h>
#include <ntqregexp.h>

class TQtMessageLogWidget : public TQScrollView {
    TQ_OBJECT
public:
    // Complexity: O(1)
    explicit TQtMessageLogWidget(TQWidget* parent = 0, const char* name = 0);
    // Complexity: O(n)
    virtual ~TQtMessageLogWidget();

    // Complexity: O(1)
    void setHistorySize(unsigned int size);
    // Complexity: O(1)
    unsigned int historySize() const;

    // Complexity: O(1)
    void setMinimumVisibleLines(unsigned int num);
    // Complexity: O(1)
    unsigned int minimumVisibleLines() const;

    // Complexity: O(1)
    void setMinimumVisibleColumns(unsigned int num);
    // Complexity: O(1)
    unsigned int minimumVisibleColumns() const;

    // Complexity: O(1)
    void setAlternatingRowColors(bool on);
    // Complexity: O(1)
    bool alternatingRowColors() const;

    // Complexity: O(k) per line where k = style count
    int setupStyle(const TQRegExp& expr, const TQColor& background, const TQColor& foreground);
    // Complexity: O(k)
    int removeStyle(int id);
    // Complexity: O(1)
    void clearStyles();

    // Complexity: O(n)
    TQString text() const;

    // Complexity: O(1)
    void setAutoScroll(bool on);
    // Complexity: O(1)
    bool autoScroll() const;
    // Complexity: O(1)
    void scrollToBottom();

public slots:
    // Complexity: O(n)
    void clear();
    // Complexity: amortized O(1)
    virtual void message(const TQString& msg);

protected:
    // Complexity: O(visible lines)
    void drawContents(TQPainter* p, int cx, int cy, int cw, int ch);
    // Complexity: O(1)
    void resizeEvent(TQResizeEvent* e);
    // Complexity: O(1)
    void keyPressEvent(TQKeyEvent* e);

    // Complexity: O(1)
    void contentsMousePressEvent(TQMouseEvent* e);
    // Complexity: O(1)
    void contentsMouseMoveEvent(TQMouseEvent* e);
    // Complexity: O(1)
    void contentsMouseReleaseEvent(TQMouseEvent* e);
    // Complexity: O(1)
    void contentsContextMenuEvent(TQContextMenuEvent* e);

private:
    struct Line;
    void updateMetrics_();
    void enforceHistory_();
    void updateContentsSize_();
    int highlightStyle_(const TQString& s) const;
    void copyAll_() const;
    void copyLine_(int line) const;
    void copySelection_() const;
    int lineAtContentsY_(int y) const;
    void normalizeSelection_();
    const Line* lineAt_(unsigned int i) const;
    Line* lineAt_(unsigned int i);

private:
    struct Style {
        TQRegExp expr;
        TQColor bg;
        TQColor fg;
        int bgSet;
        int fgSet;
    };

    struct Line {
        TQString text;
        int styleId;
    };

    Line* m_lines;
    unsigned int m_count;
    unsigned int m_capacity;

    unsigned int m_head;

    Style* m_styles;
    unsigned int m_styleCount;
    unsigned int m_styleCapacity;

    unsigned int m_historySize;
    unsigned int m_minVisibleLines;
    unsigned int m_minVisibleCols;

    int m_alternating;

    int m_lineSpacing;
    int m_ascent;
    int m_avgCharW;

    int m_longestPx;

    int m_autoScroll;

    int m_selecting;
    int m_selA;
    int m_selB;
};

#endif
