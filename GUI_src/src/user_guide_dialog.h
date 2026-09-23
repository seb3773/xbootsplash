/*
 * user_guide_dialog.h - Interactive Quick Start & User Guide dialog
 * by seb3773 - https://github.com/seb3773
 */

#ifndef USER_GUIDE_DIALOG_H
#define USER_GUIDE_DIALOG_H

#include <ntqdialog.h>
#include <ntqstring.h>

class TQListBox;
class TQTextBrowser;
class TQPushButton;
class TQLabel;

class UserGuideDialog : public TQDialog {
    TQ_OBJECT

public:
    explicit UserGuideDialog(TQWidget *parent = 0, const char *name = 0);
    virtual ~UserGuideDialog();

    void selectTopic(int index);
    void scrollContents(int x, int y);

private slots:
    void onTopicSelected(int index);
    void onPrevClicked();
    void onNextClicked();
    void onAnchorClicked(const TQString &href, const TQString &target);

private:
    void setupUI();
    void updateNavigationButtons();
    static TQString getChapterHtml(int index);

    TQListBox *m_topicList;
    TQTextBrowser *m_browser;
    TQPushButton *m_prevBtn;
    TQPushButton *m_nextBtn;
    TQPushButton *m_closeBtn;
    TQLabel *m_topicCounterLabel;
};

#endif // USER_GUIDE_DIALOG_H
