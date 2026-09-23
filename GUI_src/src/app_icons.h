/*
 * app_icons.h - Embedded application window & control icons
 */

#ifndef APP_ICONS_H
#define APP_ICONS_H

#include <ntqpixmap.h>
#include <ntqwidget.h>
#include <ntqstring.h>
#include <ntqmessagebox.h>

TQPixmap appWindowIcon();
void setAppWindowIcon(TQWidget *widget);

TQPixmap authWindowIcon();
void setAuthWindowIcon(TQWidget *widget);

TQPixmap iconPlay();
TQPixmap iconLivePlay();
TQPixmap iconPause();
TQPixmap iconStop();
TQPixmap iconPrev();
TQPixmap iconNext();
TQPixmap iconRewind();
TQPixmap iconFastForward();
TQPixmap iconFullscreen();
TQPixmap iconGuides();
TQPixmap iconInstall();
TQPixmap iconUninstall();
TQPixmap iconPackage();
TQPixmap iconBuild();
TQPixmap iconPipette();
TQPixmap iconWarn();

// Dropdown menu icons
TQPixmap iconNew();
TQPixmap iconOpen();
TQPixmap iconSave();
TQPixmap iconSaveAs();
TQPixmap iconFrames();
TQPixmap iconImage();
TQPixmap iconPackageImport();
TQPixmap iconPackageExport();
TQPixmap iconQuit();
TQPixmap iconAbout();
TQPixmap iconGrub();
TQPixmap iconDeb();
TQPixmap iconQuickHelp();

// About dialog animation frames (about12 down to about02, then final full logo)
int aboutAnimationFrameCount();
TQPixmap aboutAnimationFrame(int index);

// Warning dialog with custom embedded warn icon on the left
int showWarning(TQWidget *parent, const TQString &caption, const TQString &text,
                int button0 = TQMessageBox::Ok, int button1 = 0, int button2 = 0);

int showWarning(TQWidget *parent, const TQString &caption, const TQString &text,
                const TQString &button0Text, const TQString &button1Text = TQString::null,
                const TQString &button2Text = TQString::null,
                int defaultButtonNumber = 0, int escapeButtonNumber = -1);

#endif // APP_ICONS_H
