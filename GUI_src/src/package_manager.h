#ifndef SPLASH_PACKAGE_MANAGER_H
#define SPLASH_PACKAGE_MANAGER_H

#include <ntqobject.h>
#include <ntqstring.h>
#include <ntqmap.h>

class SplashPackageManager : public TQObject {
    TQ_OBJECT

public:
    struct PackageMetadata {
        TQString pkgVersion;
        TQString splashName;
        TQString author;
        TQString notes;
        TQString backend;
        TQString resolution;
        int displayMode;
        TQString bgColor;
        int frameW;
        int frameH;
        int nframes;
        int frameDelay;
        int fps;
        int offsetX;
        int offsetY;
        int bgOffsetX;
        int bgOffsetY;
        TQString binarySha256;
        TQString frameCrc;
        TQString binaryName;
        unsigned long binarySize;
        TQString compression;

        PackageMetadata() :
            displayMode(0),
            frameW(0),
            frameH(0),
            nframes(0),
            frameDelay(0),
            fps(0),
            offsetX(0),
            offsetY(0),
            bgOffsetX(0),
            bgOffsetY(0),
            binarySize(0) {}
    };

    explicit SplashPackageManager(TQObject *parent = 0, const char *name = 0);
    virtual ~SplashPackageManager();

    bool exportPackage(const TQString &binaryPath,
                       const PackageMetadata &meta,
                       const TQString &outputDir,
                       TQString &outPackagePath,
                       TQString &outError);

    bool readPackageMetadata(const TQString &packagePath,
                            PackageMetadata &outMeta,
                            TQString &outPreviewPath,
                            TQString &outBinaryName,
                            TQString &outError,
                            TQString *outTmpDir = 0);

    bool extractPackage(const TQString &packagePath,
                        const TQString &destDir,
                        TQString &outBinaryPath,
                        TQString &outError);

    bool exportDebianPackage(const TQString &binaryPath,
                             const PackageMetadata &meta,
                             const TQString &outputFilePath,
                             bool forShutdown,
                             TQString &outDebPath,
                             TQString &outError);

    bool convertPackageToDeb(const TQString &packagePath,
                             const TQString &outputFilePath,
                             bool forShutdown,
                             TQString &outDebPath,
                             TQString &outError);
};

#endif // SPLASH_PACKAGE_MANAGER_H
