#ifndef EMBEDDED_ENGINE_EXTRACTOR_H
#define EMBEDDED_ENGINE_EXTRACTOR_H

#include <ntqstring.h>

class EmbeddedEngineExtractor {
public:
    // Create an ephemeral build directory in volatile RAM (/run/user/<uid>/ or /tmp/)
    static TQString createEphemeralBuildDir(TQString *errorMsg = 0);

    // Extract embedded engine sources into the specified directory
    static bool extractTo(const TQString &targetDir, TQString *errorMsg = 0);

    // Safely remove the ephemeral build directory
    static void cleanupBuildDir(const TQString &targetDir);
};

#endif // EMBEDDED_ENGINE_EXTRACTOR_H
