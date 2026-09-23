#ifndef SILENT_BOOT_DIALOG_H
#define SILENT_BOOT_DIALOG_H

#include <ntqdialog.h>
#include <ntqstring.h>
#include <ntqstringlist.h>
#include <ntqvaluelist.h>

class TQListView;
class TQListViewItem;
class TQLabel;
class TQPushButton;
class TQTextEdit;
class TQProcess;

struct SilentBootItem {
    TQString name;
    TQString prefix;
    TQString recommended;
    TQString description;
    bool activeCmdline;
    bool configuredGrub;
    bool isOptional;
    TQString currentValue;
};

class SilentBootDialog : public TQDialog {
    TQ_OBJECT

public:
    explicit SilentBootDialog(TQWidget *parent = 0, const char *name = 0);
    virtual ~SilentBootDialog();

    void refreshAnalysis();

private slots:
    void onApplyClicked();
    void onCopyClicked();
    void onProcessReadyRead();
    void onProcessExited();

private:
    void setupUI();
    void loadCurrentConfig();
    void cleanupTempScript();

    TQListView *m_listView;
    TQTextEdit *m_currentGrubEdit;
    TQTextEdit *m_recommendedGrubEdit;
    TQLabel *m_statusSummaryLabel;
    TQPushButton *m_btnApply;
    TQPushButton *m_btnCopy;
    TQPushButton *m_btnClose;

    TQValueList<SilentBootItem> m_items;
    TQString m_currentGrubLine;
    TQString m_recommendedGrubLine;
    TQStringList m_missingFlags;

    TQProcess *m_process;
    TQString m_tempScriptPath;
    TQString m_processOutput;
    TQString m_sudoPassword;
};

#endif // SILENT_BOOT_DIALOG_H
