/*
 * package_inspector_dialog.h - Dedicated package inspector, animated preview & action dialog
 */

#ifndef PACKAGE_INSPECTOR_DIALOG_H
#define PACKAGE_INSPECTOR_DIALOG_H

#include <ntqdialog.h>
#include <ntqstring.h>
#include <ntqpixmap.h>
#include "package_manager.h"

class TQLabel;
class TQPushButton;
class TQMovie;
class SplashInstallerEngine;

class PackagePreviewWidget : public TQWidget {
    TQ_OBJECT

public:
    PackagePreviewWidget(TQWidget *parent = 0);
    virtual ~PackagePreviewWidget();

    void setMovie(const TQString &path);
    void setPixmap(const TQPixmap &pm);
    void clear();
    bool hasContent() const;
    bool isMovie() const;

public slots:
    void onMovieUpdated(const TQRect &);
    void onMovieResized(const TQSize &);

protected:
    virtual void paintEvent(TQPaintEvent *);

private:
    TQMovie *m_movie;
    TQPixmap m_staticPixmap;
};

class PackageInspectorDialog : public TQDialog {
    TQ_OBJECT

public:
    PackageInspectorDialog(const TQString &packagePath,
                           SplashPackageManager *pkgMgr,
                           SplashInstallerEngine *installer,
                           const TQString &projectRoot,
                           TQWidget *parent = 0,
                           const char *name = 0);
    virtual ~PackageInspectorDialog();

    bool isPackageLoaded() const { return m_isLoaded; }

signals:
    void statusMessage(const TQString &msg);

private slots:
    void onTestLive();
    void onInstallPackage();
    void onExtractPackage();
    void onExportDeb();

private:
    TQString m_packagePath;
    SplashPackageManager *m_pkgMgr;
    SplashInstallerEngine *m_installer;
    TQString m_projectRoot;
    TQString m_tmpDir;

    bool m_isLoaded;
    SplashPackageManager::PackageMetadata m_meta;
    TQString m_previewPath;
    TQString m_binaryName;

    PackagePreviewWidget *m_previewWidget;
    TQPushButton *m_btnTestLive;
    TQPushButton *m_btnInstall;
    TQPushButton *m_btnExtract;
    TQPushButton *m_btnExportDeb;
    TQPushButton *m_btnClose;

    void setupUI();
    void cleanupTempDir();
};

#endif // PACKAGE_INSPECTOR_DIALOG_H
