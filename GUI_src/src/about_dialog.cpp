#include "about_dialog.h"
#include "app_icons.h"
#include "version.h"

#include <ntqlayout.h>
#include <ntqlabel.h>
#include <ntqpushbutton.h>
#include <ntqtimer.h>
#include <ntqcursor.h>
#include <ntqtooltip.h>
#include <ntqevent.h>

AboutDialog::AboutDialog(TQWidget *parent, const char *name)
    : TQDialog(parent, name, true),
      m_iconLabel(0),
      m_textLabel(0),
      m_okBtn(0),
      m_timer(0),
      m_currentFrame(0)
{
    setCaption("About XBootsplash Studio");
    setIcon(appWindowIcon());
    setupUI();

    m_timer = new TQTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onAnimTick()));
    // 70ms per frame gives a smooth, clearly visible ~850ms blooming animation
    m_timer->start(70);
}

AboutDialog::~AboutDialog() {
    if (m_timer) {
        m_timer->stop();
    }
}

void AboutDialog::setupUI() {
    TQVBoxLayout *mainLayout = new TQVBoxLayout(this, 16, 16);

    TQHBoxLayout *contentLayout = new TQHBoxLayout(mainLayout, 16);

    // Animated Icon Label on the left
    m_iconLabel = new TQLabel(this);
    m_iconLabel->setFixedSize(64, 64);
    m_iconLabel->setPixmap(aboutAnimationFrame(0));
    m_iconLabel->setCursor(TQt::pointingHandCursor);
    m_iconLabel->setAlignment(TQt::AlignCenter);
    m_iconLabel->installEventFilter(this);
    TQToolTip::add(m_iconLabel, "Click to replay splash animation");
    contentLayout->addWidget(m_iconLabel, 0, TQt::AlignTop);

    // Rich Text on the right with WordBreak and 380px width (matching QMessageBox::about layout)
    TQString text = TQString("<h2>%1 v%2</h2>"
                            "<p><b>Linux Boot Splash Creator & Studio</b></p>"
                            "<p>By <b>%3</b> &bull; Built with TQt3</p>"
                            "<p>Provides real-time interactive positioning, full-screen hardware simulation without TTY switching, optimized freestanding builds, and packaging for ultra-fast Linux boot splashes.</p>"
                            "<p>Features the <b>UPKR</b> compression algorithm by <b>Dennis Ranke</b> and the <b>ZX0</b> compression algorithm by <b>Einar Saukas</b>.</p>")
                   .arg(XBOOTSPLASH_GUI_NAME)
                   .arg(XBOOTSPLASH_GUI_VERSION)
                   .arg(XBOOTSPLASH_GUI_AUTHOR);

    m_textLabel = new TQLabel(text, this);
    m_textLabel->setTextFormat(TQt::RichText);
    m_textLabel->setAlignment(TQt::AlignLeft | TQt::AlignTop | TQt::WordBreak);
    m_textLabel->setFixedWidth(380);
    contentLayout->addWidget(m_textLabel, 1);

    // Bottom Action Row - centered OK button exactly like standard QMessageBox::about
    TQHBoxLayout *btnLayout = new TQHBoxLayout(mainLayout, 6);
    btnLayout->addStretch(1);
    m_okBtn = new TQPushButton("&OK", this);
    m_okBtn->setDefault(true);
    connect(m_okBtn, SIGNAL(clicked()), this, SLOT(accept()));
    btnLayout->addWidget(m_okBtn);
    btnLayout->addStretch(1);
}

void AboutDialog::onAnimTick() {
    m_currentFrame++;
    if (m_currentFrame < aboutAnimationFrameCount()) {
        m_iconLabel->setPixmap(aboutAnimationFrame(m_currentFrame));
    } else {
        m_timer->stop();
    }
}

void AboutDialog::restartAnimation() {
    m_currentFrame = 0;
    m_iconLabel->setPixmap(aboutAnimationFrame(0));
    m_timer->start(70);
}

void AboutDialog::stepFrameForTest() {
    onAnimTick();
}

bool AboutDialog::eventFilter(TQObject *watched, TQEvent *e) {
    if (watched == m_iconLabel && e->type() == TQEvent::MouseButtonPress) {
        restartAnimation();
        return true;
    }
    return TQDialog::eventFilter(watched, e);
}

void AboutDialog::showAbout(TQWidget *parent) {
    AboutDialog dlg(parent);
    dlg.exec();
}

#include "about_dialog.moc"
