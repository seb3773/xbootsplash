#ifndef ABOUT_DIALOG_H
#define ABOUT_DIALOG_H

#include <ntqdialog.h>

class TQLabel;
class TQPushButton;
class TQTimer;

class AboutDialog : public TQDialog {
    TQ_OBJECT

public:
    explicit AboutDialog(TQWidget *parent = 0, const char *name = 0);
    virtual ~AboutDialog();

    static void showAbout(TQWidget *parent);

    int currentFrameIndex() const { return m_currentFrame; }
    void stepFrameForTest();

protected:
    virtual bool eventFilter(TQObject *watched, TQEvent *e);

private slots:
    void onAnimTick();
    void restartAnimation();

private:
    void setupUI();

    TQLabel *m_iconLabel;
    TQLabel *m_textLabel;
    TQPushButton *m_okBtn;
    TQTimer *m_timer;
    int m_currentFrame;
};

#endif // ABOUT_DIALOG_H
