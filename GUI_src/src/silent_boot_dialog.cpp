#include "silent_boot_dialog.h"
#include "password_dialog.h"
#include "app_icons.h"

#include <ntqlayout.h>
#include <ntqlabel.h>
#include <ntqpushbutton.h>
#include <ntqlistview.h>
#include <ntqtextedit.h>
#include <ntqfile.h>
#include <ntqtextstream.h>
#include <ntqclipboard.h>
#include <ntqapplication.h>
#include <ntqmessagebox.h>
#include <ntqprocess.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

SilentBootDialog::SilentBootDialog(TQWidget *parent, const char *name)
    : TQDialog(parent, name, true),
      m_process(0),
      m_tempScriptPath(""),
      m_processOutput(""),
      m_sudoPassword("")
{
    setCaption("Silent Boot Configuration (GRUB)");
    setIcon(iconGrub());
    resize(780, 560);
    setupUI();
    refreshAnalysis();
}

SilentBootDialog::~SilentBootDialog() {
    cleanupTempScript();
    if (m_process && m_process->isRunning()) {
        m_process->kill();
        delete m_process;
    }
}

void SilentBootDialog::setupUI() {
    TQVBoxLayout *mainLayout = new TQVBoxLayout(this, 12, 8);

    TQLabel *headerLabel = new TQLabel(
        "<b><font size=\"+1\">Silent Boot Configuration Helper</font></b><br>"
        "<font color=\"#666666\">A silent boot suppresses kernel dmesg, systemd startup logs, "
        "and virtual terminal cursor blinking so they do not scroll or disrupt the bootsplash animation.</font>",
        this);
    headerLabel->setTextFormat(TQt::RichText);
    mainLayout->addWidget(headerLabel);

    m_statusSummaryLabel = new TQLabel(this);
    m_statusSummaryLabel->setTextFormat(TQt::RichText);
    mainLayout->addWidget(m_statusSummaryLabel);

    m_listView = new TQListView(this);
    m_listView->addColumn("Parameter", 150);
    m_listView->addColumn("Status in GRUB", 130);
    m_listView->addColumn("Active Kernel (/proc)", 150);
    m_listView->addColumn("Purpose / Description", 310);
    m_listView->setRootIsDecorated(false);
    m_listView->setAllColumnsShowFocus(true);
    m_listView->setSorting(-1);
    mainLayout->addWidget(m_listView, 1);

    TQLabel *lblGrub = new TQLabel("<b>Current GRUB_CMDLINE_LINUX_DEFAULT:</b>", this);
    mainLayout->addWidget(lblGrub);

    m_currentGrubEdit = new TQTextEdit(this);
    m_currentGrubEdit->setReadOnly(true);
    m_currentGrubEdit->setMaximumHeight(48);
    mainLayout->addWidget(m_currentGrubEdit);

    TQLabel *lblRec = new TQLabel("<b>Recommended GRUB line (merges missing parameters, preserves all custom hardware options):</b>", this);
    mainLayout->addWidget(lblRec);

    m_recommendedGrubEdit = new TQTextEdit(this);
    m_recommendedGrubEdit->setReadOnly(true);
    m_recommendedGrubEdit->setMaximumHeight(48);
    mainLayout->addWidget(m_recommendedGrubEdit);

    TQHBoxLayout *btnLayout = new TQHBoxLayout(mainLayout, 8);

    m_btnApply = new TQPushButton("Apply Recommended Settings to GRUB", this);
    btnLayout->addWidget(m_btnApply);
    connect(m_btnApply, SIGNAL(clicked()), this, SLOT(onApplyClicked()));

    m_btnCopy = new TQPushButton("Copy Recommended Flags", this);
    btnLayout->addWidget(m_btnCopy);
    connect(m_btnCopy, SIGNAL(clicked()), this, SLOT(onCopyClicked()));

    btnLayout->addStretch(1);

    m_btnClose = new TQPushButton("Close", this);
    btnLayout->addWidget(m_btnClose);
    connect(m_btnClose, SIGNAL(clicked()), this, SLOT(accept()));
}

void SilentBootDialog::cleanupTempScript() {
    if (!m_tempScriptPath.isEmpty() && TQFile::exists(m_tempScriptPath)) {
        unlink(m_tempScriptPath.latin1());
        m_tempScriptPath = "";
    }
}

void SilentBootDialog::loadCurrentConfig() {
    m_items.clear();
    m_missingFlags.clear();

    // 1. Read /proc/cmdline
    TQString cmdline = "";
    TQFile procFile("/proc/cmdline");
    if (procFile.open(IO_ReadOnly)) {
        TQTextStream ts(&procFile);
        cmdline = ts.read();
        procFile.close();
    }
    TQStringList cmdTokens = TQStringList::split(' ', cmdline.stripWhiteSpace());

    // 2. Read /etc/default/grub
    m_currentGrubLine = "";
    TQString generalGrubLine = "";
    TQFile grubFile("/etc/default/grub");
    if (grubFile.open(IO_ReadOnly)) {
        TQTextStream ts(&grubFile);
        TQString line;
        while (!ts.atEnd()) {
            line = ts.readLine();
            TQString trimmed = line.stripWhiteSpace();
            if (trimmed.startsWith("GRUB_CMDLINE_LINUX_DEFAULT=")) {
                int eq = trimmed.find('=');
                TQString val = trimmed.mid(eq + 1).stripWhiteSpace();
                if ((val.startsWith("\"") && val.endsWith("\"")) ||
                    (val.startsWith("'") && val.endsWith("'"))) {
                    val = val.mid(1, val.length() - 2);
                }
                m_currentGrubLine = val;
            } else if (trimmed.startsWith("GRUB_CMDLINE_LINUX=")) {
                int eq = trimmed.find('=');
                TQString val = trimmed.mid(eq + 1).stripWhiteSpace();
                if ((val.startsWith("\"") && val.endsWith("\"")) ||
                    (val.startsWith("'") && val.endsWith("'"))) {
                    val = val.mid(1, val.length() - 2);
                }
                generalGrubLine = val;
            }
        }
        grubFile.close();
    }
    TQString combinedGrub = m_currentGrubLine + " " + generalGrubLine;
    TQStringList grubTokens = TQStringList::split(' ', combinedGrub.stripWhiteSpace());

    // Definition of the Key Parameters (6 required for silent boot, 1 optional legacy flag)
    struct DefParam {
        const char *name;
        const char *prefix;
        const char *recommended;
        const char *desc;
        bool isPrefix;
        bool isOptional;
    } defs[7] = {
        {"quiet", "quiet", "quiet", "Suppresses normal kernel informational messages", false, false},
        {"loglevel", "loglevel=", "loglevel=0", "Silences kernel dmesg console output (0 or <= 3)", true, false},
        {"systemd.show_status", "systemd.show_status=", "systemd.show_status=0", "Hides systemd [ OK ] service startup status messages", true, false},
        {"rd.systemd.show_status", "rd.systemd.show_status=", "rd.systemd.show_status=false", "Hides systemd status messages during early initramfs", true, false},
        {"rd.udev.log_level", "rd.udev.log_level=", "rd.udev.log_level=0", "Silences udev device discovery logs in initramfs (0 or <= 3)", true, false},
        {"vt.cur_default", "vt.cur_default=", "vt.cur_default=1", "Disables virtual terminal blinking cursor", true, false},
        {"splash", "splash", "splash", "Legacy bootsplash indicator (optional, not required by xbootsplash)", false, true}
    };

    for (int i = 0; i < 7; ++i) {
        SilentBootItem item;
        item.name = defs[i].name;
        item.prefix = defs[i].prefix;
        item.recommended = defs[i].recommended;
        item.description = defs[i].desc;
        item.activeCmdline = false;
        item.configuredGrub = false;
        item.isOptional = defs[i].isOptional;

        // Check in /proc/cmdline
        for (TQStringList::Iterator it = cmdTokens.begin(); it != cmdTokens.end(); ++it) {
            TQString t = *it;
            if (defs[i].isPrefix) {
                if (t.startsWith(defs[i].prefix)) {
                    item.activeCmdline = true;
                    break;
                }
            } else {
                if (t == defs[i].prefix) {
                    item.activeCmdline = true;
                    break;
                }
            }
        }

        // Check in GRUB
        for (TQStringList::Iterator it = grubTokens.begin(); it != grubTokens.end(); ++it) {
            TQString t = *it;
            if (defs[i].isPrefix) {
                if (t.startsWith(defs[i].prefix)) {
                    item.configuredGrub = true;
                    item.currentValue = t;
                    break;
                }
            } else {
                if (t == defs[i].prefix) {
                    item.configuredGrub = true;
                    item.currentValue = t;
                    break;
                }
            }
        }

        if (!item.configuredGrub && !item.isOptional) {
            m_missingFlags << item.recommended;
        }

        m_items.append(item);
    }

    // Compute recommended full GRUB line
    TQString rec = m_currentGrubLine;
    for (TQStringList::Iterator it = m_missingFlags.begin(); it != m_missingFlags.end(); ++it) {
        if (!rec.isEmpty()) rec += " ";
        rec += *it;
    }
    m_recommendedGrubLine = rec;
}

void SilentBootDialog::refreshAnalysis() {
    loadCurrentConfig();

    m_listView->clear();

    int totalRequired = 0;
    int configuredCount = 0;
    TQListViewItem *lastItem = 0;
    for (TQValueList<SilentBootItem>::Iterator it = m_items.begin(); it != m_items.end(); ++it) {
        SilentBootItem item = *it;
        if (!item.isOptional) {
            totalRequired++;
            if (item.configuredGrub) configuredCount++;
        }

        TQString grubStatus = item.configuredGrub ? "  [OK] Configured" : (item.isOptional ? "  [--] Optional" : "  [!] Missing");
        TQString procStatus = item.activeCmdline ? "  [OK] Active" : (item.isOptional ? "  [--] Inactive (Optional)" : "  [--] Inactive");

        lastItem = new TQListViewItem(m_listView, lastItem, item.name, grubStatus, procStatus, item.description);
    }

    if (configuredCount == totalRequired) {
        m_statusSummaryLabel->setText(
            TQString("<font color=\"#008800\"><b>Status: %1/%2 Silent Boot parameters configured in GRUB. "
                     "Your boot is fully optimized for a clean splash.</b></font>")
                .arg(configuredCount).arg(totalRequired));
        m_btnApply->setEnabled(false);
        m_btnApply->setText("All Recommended Settings Already in GRUB");
    } else {
        m_statusSummaryLabel->setText(
            TQString("<font color=\"#cc6600\"><b>Status: %1/%2 Silent Boot parameters configured in GRUB. "
                     "%3 parameter(s) missing to guarantee clean splash without logs scrolling.</b></font>")
                .arg(configuredCount)
                .arg(totalRequired)
                .arg(totalRequired - configuredCount));
        m_btnApply->setEnabled(true);
        m_btnApply->setText("Apply Recommended Settings to GRUB");
    }

    m_currentGrubEdit->setText(m_currentGrubLine);
    m_recommendedGrubEdit->setText(m_recommendedGrubLine);
}

void SilentBootDialog::onCopyClicked() {
    TQString flagsToCopy;
    if (m_missingFlags.isEmpty()) {
        flagsToCopy = "quiet loglevel=0 systemd.show_status=0 rd.systemd.show_status=false rd.udev.log_level=0 vt.cur_default=1";
    } else {
        flagsToCopy = m_missingFlags.join(" ");
    }

    TQClipboard *cb = TQApplication::clipboard();
    if (cb) {
        cb->setText(flagsToCopy, TQClipboard::Clipboard);
    }
    TQMessageBox::information(this, "Flags Copied",
                              "The following flags were copied to your clipboard:\n\n" + flagsToCopy);
}

void SilentBootDialog::onApplyClicked() {
    if (m_missingFlags.isEmpty()) {
        TQMessageBox::information(this, "Silent Boot", "All recommended Silent Boot parameters are already present in /etc/default/grub.");
        return;
    }

    TQString password;
    if (getuid() != 0) {
        if (!SplashPasswordDialog::getPassword(this, password)) {
            return; // Cancelled
        }
    }
    m_sudoPassword = password;

    cleanupTempScript();

    char tempTemplate[] = "/tmp/xbs_silent_boot_XXXXXX.sh";
    int fd = mkstemp(tempTemplate);
    if (fd < 0) {
        TQMessageBox::critical(this, "Error", "Failed to create temporary script file.");
        return;
    }
    m_tempScriptPath = tempTemplate;

    TQString scriptContent =
        "#!/bin/bash\n"
        "set -e\n"
        "GRUB_FILE=\"/etc/default/grub\"\n"
        "if [ ! -f \"$GRUB_FILE\" ]; then\n"
        "    echo \"[ERROR] /etc/default/grub not found\" >&2\n"
        "    exit 1\n"
        "fi\n\n"
        "BACKUP=\"${GRUB_FILE}.bak.$(date +%Y%m%d_%H%M%S)\"\n"
        "cp -a \"$GRUB_FILE\" \"$BACKUP\"\n"
        "echo \"[1/2] Backup created: $BACKUP\"\n\n"
        "rollback() {\n"
        "    echo \"[ROLLBACK] Error occurred! Restoring $GRUB_FILE from backup...\" >&2\n"
        "    cp -a \"$BACKUP\" \"$GRUB_FILE\" 2>/dev/null || true\n"
        "    echo \"[ROLLBACK] Configuration restored to prior state.\" >&2\n"
        "}\n"
        "trap rollback ERR\n\n"
        "python3 -c '\n"
        "import re, sys\n"
        "path = \"/etc/default/grub\"\n"
        "flags = sys.argv[1].split()\n"
        "with open(path, \"r\") as f:\n"
        "    content = f.read()\n"
        "m = re.search(r\"^(GRUB_CMDLINE_LINUX_DEFAULT\\s*=\\s*[\\\"'\\'])(.*?)([\\\"'\\'])\", content, re.MULTILINE)\n"
        "if m:\n"
        "    prefix, val, suffix = m.group(1), m.group(2), m.group(3)\n"
        "    tokens = val.split()\n"
        "    to_add = []\n"
        "    for fl in flags:\n"
        "        pfx = fl.split(\"=\")[0]\n"
        "        if not any(t == fl or t.startswith(pfx + \"=\") for t in tokens):\n"
        "            to_add.append(fl)\n"
        "    if to_add:\n"
        "        new_val = (val + \" \" + \" \".join(to_add)).strip()\n"
        "        new_content = content[:m.start()] + prefix + new_val + suffix + content[m.end():]\n"
        "        with open(path, \"w\") as f:\n"
        "            f.write(new_content)\n"
        "        print(\"Injected into GRUB: \" + \" \".join(to_add))\n"
        "' \"$1\"\n\n"
        "echo \"[2/2] Regenerating GRUB bootloader configuration...\"\n"
        "if command -v update-grub >/dev/null 2>&1; then\n"
        "    update-grub\n"
        "elif command -v grub-mkconfig >/dev/null 2>&1; then\n"
        "    grub-mkconfig -o /boot/grub/grub.cfg\n"
        "fi\n"
        "trap - ERR\n"
        "echo \"SUCCESS: GRUB configuration updated successfully.\"\n";

    write(fd, scriptContent.latin1(), scriptContent.length());
    close(fd);
    chmod(m_tempScriptPath.latin1(), 0755);

    m_btnApply->setEnabled(false);
    m_btnApply->setText("Applying changes (update-grub)...");

    if (m_process) {
        delete m_process;
        m_process = 0;
    }
    m_process = new TQProcess(this);
    connect(m_process, SIGNAL(readyReadStdout()), this, SLOT(onProcessReadyRead()));
    connect(m_process, SIGNAL(readyReadStderr()), this, SLOT(onProcessReadyRead()));
    connect(m_process, SIGNAL(processExited()), this, SLOT(onProcessExited()));

    TQStringList args;
    if (getuid() != 0) {
        args << "sudo" << "-S" << "-p" << "";
    }
    args << m_tempScriptPath;
    args << m_missingFlags.join(" ");

    m_process->setArguments(args);
    m_processOutput = "";

    if (!m_process->start()) {
        TQMessageBox::critical(this, "Execution Error", "Failed to start process with sudo.");
        cleanupTempScript();
        refreshAnalysis();
        return;
    }

    if (getuid() != 0 && !m_sudoPassword.isEmpty()) {
        m_process->writeToStdin(m_sudoPassword + "\n");
    }
}

void SilentBootDialog::onProcessReadyRead() {
    if (!m_process) return;
    TQString out = m_process->readStdout();
    if (!out.isEmpty()) m_processOutput += out;
    TQString err = m_process->readStderr();
    if (!err.isEmpty()) m_processOutput += err;
}

void SilentBootDialog::onProcessExited() {
    int exitCode = m_process ? m_process->exitStatus() : -1;
    cleanupTempScript();

    if (exitCode == 0) {
        TQMessageBox::information(
            this,
            "Success",
            "Silent Boot parameters were successfully merged into /etc/default/grub!\n\n"
            "An automatic backup was created before modification.\n"
            "update-grub has updated the bootloader.\n"
            "The new settings will take effect on the next boot.");
    } else {
        TQMessageBox::critical(
            this,
            "Error",
            "Failed to update GRUB.\n\n"
            "Output:\n" + m_processOutput);
    }

    refreshAnalysis();
}

#include "silent_boot_dialog.moc"

