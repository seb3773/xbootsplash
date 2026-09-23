#ifndef SPLASH_PASSWORD_DIALOG_H
#define SPLASH_PASSWORD_DIALOG_H

#include <ntqdialog.h>
#include <ntqstring.h>

class TQLineEdit;
class TQLabel;
class TQPushButton;

class SplashPasswordDialog : public TQDialog {
    TQ_OBJECT

public:
    explicit SplashPasswordDialog(TQWidget *parent = 0, const char *name = 0);
    virtual ~SplashPasswordDialog();

    TQString password() const { return m_verifiedPassword; }

    static bool getPassword(TQWidget *parent, TQString &outPassword);
    static void clearCachedPassword();
    static bool hasCachedPassword();
    static void setCachedPassword(const TQString &pwd);
    static bool verifyPassword(const TQString &pwd);

protected:
    virtual void showEvent(TQShowEvent *e);

private slots:
    void onValidate();

private:
    TQLineEdit *m_passwordEdit;
    TQLabel *m_errorLabel;
    TQPushButton *m_btnOk;
    TQPushButton *m_btnCancel;
    TQString m_verifiedPassword;

    static TQString s_cachedPassword;
    static bool s_hasCachedPassword;
};

#endif // SPLASH_PASSWORD_DIALOG_H
