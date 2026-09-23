#include "password_dialog.h"
#include "app_icons.h"

#include <ntqlayout.h>
#include <ntqlabel.h>
#include <ntqlineedit.h>
#include <ntqpushbutton.h>
#include <ntqapplication.h>
#include <ntqstyle.h>
#include <ntqcursor.h>
#include <ntqimage.h>

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>

TQString SplashPasswordDialog::s_cachedPassword = "";
bool SplashPasswordDialog::s_hasCachedPassword = false;

void SplashPasswordDialog::clearCachedPassword() {
    s_cachedPassword = "";
    s_hasCachedPassword = false;
}

bool SplashPasswordDialog::hasCachedPassword() {
    return s_hasCachedPassword;
}

void SplashPasswordDialog::setCachedPassword(const TQString &pwd) {
    s_cachedPassword = pwd;
    s_hasCachedPassword = true;
}

SplashPasswordDialog::SplashPasswordDialog(TQWidget *parent, const char *name)
    : TQDialog(parent, name, true),
      m_verifiedPassword("")
{
    setCaption("Authentication Required");
    setAuthWindowIcon(this);
    setFixedWidth(380);

    TQVBoxLayout *mainLayout = new TQVBoxLayout(this, 16, 12);

    // Header layout with icon and explanation
    TQHBoxLayout *headerLayout = new TQHBoxLayout(mainLayout, 12);

    TQLabel *iconLabel = new TQLabel(this);
    TQPixmap icon = authWindowIcon();
    if (icon.width() > 44 || icon.height() > 44) {
        icon = icon.convertToImage().smoothScale(44, 44, TQImage::ScaleMin);
    }
    iconLabel->setPixmap(icon);
    iconLabel->setFixedSize(44, 44);
    iconLabel->setAlignment(TQt::AlignTop | TQt::AlignHCenter);
    headerLayout->addWidget(iconLabel);

    TQLabel *descLabel = new TQLabel(
        "<b><font size='+1'>Administrator Privileges</font></b><br>"
        "<font color='#555555'>Root access is required for hardware (VT)<br>"
        "and system operations.</font><br><br>"
        "<b>Password (sudo):</b>",
        this);
    descLabel->setAlignment(TQt::AlignVCenter | TQt::AlignLeft);
    headerLayout->addWidget(descLabel, 1);

    // Password field
    m_passwordEdit = new TQLineEdit(this);
    m_passwordEdit->setEchoMode(TQLineEdit::Password);
    m_passwordEdit->setMinimumHeight(26);
    connect(m_passwordEdit, SIGNAL(returnPressed()), this, SLOT(onValidate()));
    mainLayout->addWidget(m_passwordEdit);

    // Error label (initially hidden)
    m_errorLabel = new TQLabel(this);
    m_errorLabel->setPaletteForegroundColor(TQColor(210, 40, 40));
    m_errorLabel->hide();
    mainLayout->addWidget(m_errorLabel);

    // Buttons
    TQHBoxLayout *btnLayout = new TQHBoxLayout(mainLayout, 8);
    btnLayout->addStretch(1);

    m_btnCancel = new TQPushButton("Cancel", this);
    connect(m_btnCancel, SIGNAL(clicked()), this, SLOT(reject()));
    btnLayout->addWidget(m_btnCancel);

    m_btnOk = new TQPushButton("Authenticate", this);
    m_btnOk->setDefault(true);
    connect(m_btnOk, SIGNAL(clicked()), this, SLOT(onValidate()));
    btnLayout->addWidget(m_btnOk);
}

SplashPasswordDialog::~SplashPasswordDialog() {
}

void SplashPasswordDialog::showEvent(TQShowEvent *e) {
    TQDialog::showEvent(e);
    m_passwordEdit->setFocus();
}

bool SplashPasswordDialog::getPassword(TQWidget *parent, TQString &outPassword) {
    if (getuid() == 0) {
        outPassword = "";
        return true;
    }

    // 1. Check session cache first: if already authenticated in this session, reuse directly
    if (s_hasCachedPassword) {
        outPassword = s_cachedPassword;
        // Keep sudo timestamp alive in background so child processes/subcommands also benefit
        if (!s_cachedPassword.isEmpty()) {
            FILE *fp = popen("sudo -S -p '' -v 2>/dev/null", "w");
            if (fp) {
                TQCString pass = s_cachedPassword.local8Bit();
                fwrite(pass.data(), 1, pass.length(), fp);
                fwrite("\n", 1, 1, fp);
                pclose(fp);
            }
        }
        return true;
    }

    // 2. Check if passwordless sudo (NOPASSWD) works on this machine
    if (system("sudo -n true 2>/dev/null") == 0) {
        s_cachedPassword = "";
        s_hasCachedPassword = true;
        outPassword = "";
        return true;
    }

    // 3. Prompt user with dialog
    SplashPasswordDialog dlg(parent);
    if (dlg.exec() == TQDialog::Accepted) {
        s_cachedPassword = dlg.password();
        s_hasCachedPassword = true;
        outPassword = s_cachedPassword;
        return true;
    }

    return false;
}

bool SplashPasswordDialog::verifyPassword(const TQString &pwd) {
    if (getuid() == 0) return true;

    if (pwd.isEmpty()) {
        return (system("sudo -n true 2>/dev/null") == 0);
    }

    // Note: DO NOT pass -k here! -k wipes out the timestamp cache!
    // We use sudo -S -p '' -v which refreshes sudo's credential timestamp in /var/run/sudo/ts
    FILE *fp = popen("sudo -S -p '' -v 2>/dev/null", "w");
    bool success = false;
    if (fp) {
        TQCString pass = pwd.local8Bit();
        fwrite(pass.data(), 1, pass.length(), fp);
        fwrite("\n", 1, 1, fp);
        fflush(fp);
        int status = pclose(fp);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            success = true;
        }
    }

    return success;
}

void SplashPasswordDialog::onValidate() {
    TQString pwd = m_passwordEdit->text();

    // Check if passwordless sudo is allowed on this machine
    if (pwd.isEmpty()) {
        if (system("sudo -n true 2>/dev/null") == 0) {
            m_verifiedPassword = "";
            s_cachedPassword = "";
            s_hasCachedPassword = true;
            accept();
            return;
        }
        m_errorLabel->setText("Please enter your password.");
        m_errorLabel->show();
        m_passwordEdit->setFocus();
        return;
    }

    m_btnOk->setEnabled(false);
    m_btnCancel->setEnabled(false);
    m_passwordEdit->setEnabled(false);
    TQApplication::setOverrideCursor(TQt::WaitCursor);

    bool ok = verifyPassword(pwd);

    TQApplication::restoreOverrideCursor();
    m_btnOk->setEnabled(true);
    m_btnCancel->setEnabled(true);
    m_passwordEdit->setEnabled(true);

    if (ok) {
        m_verifiedPassword = pwd;
        s_cachedPassword = pwd;
        s_hasCachedPassword = true;
        accept();
    } else {
        m_errorLabel->setText("Incorrect password. Please try again.");
        m_errorLabel->show();
        m_passwordEdit->clear();
        m_passwordEdit->setFocus();
    }
}

#include "password_dialog.moc"
