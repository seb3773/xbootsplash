#ifndef SPLASH_BUILD_ENGINE_H
#define SPLASH_BUILD_ENGINE_H

#include <ntqobject.h>
#include <ntqstring.h>
#include <ntqprocess.h>
#include <ntqcolor.h>

class SplashBuildEngine : public TQObject {
    TQ_OBJECT

public:
    struct BuildParams {
        int displayMode;
        TQString framesPath;
        TQString bgImagePath;
        int offsetX;
        int offsetY;
        int bgOffsetX;
        int bgOffsetY;
        int frameDelay;
        int loopMode;
        int loopStart;
        int minBootLoops;
        bool invertFrames;
        TQColor bgColor;
        TQString targetResolution;
        TQString binaryName;
        bool useDrm;
        TQString compressionMethod;
        bool useZx0;
        int superCompression; /* 0 = none, 1 = ZX0, 2 = UPKR */
        TQString projectRoot;
    };

    explicit SplashBuildEngine(TQObject *parent = 0, const char *name = 0);
    virtual ~SplashBuildEngine();

    bool isBuilding() const { return m_isBuilding; }
    void startBuild(const BuildParams &params);
    void cancelBuild();

signals:
    void logMessage(const TQString &msg);
    void buildStarted();
    void buildStatusChanged(const TQString &status);
    void buildFinished(bool success, const TQString &binaryPath, unsigned long sizeBytes);

private slots:
    void onProcessReadyReadStdout();
    void onProcessReadyReadStderr();
    void onProcessExited();

private:
    void runNextStep();
    void log(const TQString &msg);
    void fail(const TQString &reason);

    enum Step {
        StepNone = 0,
        StepCompileGenerator,
        StepRunGenerator,
        StepCompileBinary,
        StepDone
    };

    bool copyFile(const TQString &src, const TQString &dst);

    BuildParams m_params;
    bool m_isBuilding;
    Step m_currentStep;
    TQProcess *m_process;
    TQString m_binaryPath;
    TQString m_tempBuildDir;
};

#endif // SPLASH_BUILD_ENGINE_H
