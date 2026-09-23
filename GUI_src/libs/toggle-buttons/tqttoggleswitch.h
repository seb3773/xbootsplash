#ifndef TQTTOGGLESWITCH_H
#define TQTTOGGLESWITCH_H

#include <ntqwidget.h>
#include <ntqcolor.h>
#include <ntqstring.h>
#include <ntqtimer.h>
#include <ntqdatetime.h>

class TQtToggleSwitch : public TQWidget {
    TQ_OBJECT
public:
    enum Mode {
        BorderTraceRounded = 0,
        BorderTraceRect = 1,
        BorderTraceRectFlat = 2,
        BorderSolidRounded = 3,
        BorderSolidRoundedNoText = 4,
        BorderSolidSoftRectNoText = 5,
        BorderSolidVerticalMarks = 6
    };

    enum Easing {
        InOutCubic = 0,
        OutCirc = 1,
        OutBack = 2
    };

    TQtToggleSwitch(TQWidget* parent = 0, const char* name = 0);
    TQtToggleSwitch(int mode, TQWidget* parent = 0, const char* name = 0);
    TQtToggleSwitch(int mode, bool checked, TQWidget* parent = 0, const char* name = 0);

    bool isChecked() const;
    bool getState() const;

    void setMode(int mode);
    int mode() const;

    void setSuitableHeight(int h);

    void setForeground(const TQColor& c);
    void setBackground(const TQColor& on, const TQColor& off);
    void setBorder(const TQColor& c, int size);

    void setAnimationDuration(int ms);
    void setAnimationEasing(int easing);

    const TQColor& foregroundColor() const;
    const TQColor& onColor() const;
    const TQColor& offColor() const;
    const TQColor& borderColor() const;
    int borderSize() const;

    TQSize minimumSizeHint() const;
    TQSize sizeHint() const;

signals:
    void stateChanged(bool state);

public slots:
    void setState(bool state);
    void setStateWithoutSignal(bool state);
    void toggleState();
    void toggleStateWithoutSignal();

protected:
    void resizeEvent(TQResizeEvent*);
    void mousePressEvent(TQMouseEvent*);
    void mouseMoveEvent(TQMouseEvent*);
    void mouseReleaseEvent(TQMouseEvent*);
    void paintEvent(TQPaintEvent*);

private slots:
    void onAnimTick();

private:
    void calculateGeometry();

    void startSwitchAnimation();

    void setProgressManual(double p);

    TQColor blendedBgColor(double p) const;

    static inline double clamp01(double v) {
        if (v < 0.0) return 0.0;
        if (v > 1.0) return 1.0;
        return v;
    }

    static double ease(int easing, double t);

private:
    int m_mode;
    int m_checked;

    TQColor m_fg;
    TQColor m_on;
    TQColor m_off;
    TQColor m_bd;
    int m_bdSize;

    int m_animDuration;
    int m_easing;

    double m_prog;
    double m_progStart;
    double m_progTarget;

    TQTime m_animStart;

    int m_animating;

    int m_pressed;
    int m_moved;
    int m_dragging;
    int m_moveTargetState;

    int m_prevX;
    TQPoint m_pressPos;

    double m_slideLeft;
    double m_slideRight;

    TQTimer m_timer;
};

#endif
