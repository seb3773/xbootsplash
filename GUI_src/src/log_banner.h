#ifndef LOG_BANNER_H
#define LOG_BANNER_H

#include <ntqstring.h>

namespace SplashLog {

// Standard banner width: 70 characters
static const int DefaultBannerWidth = 70;

// Format an operation header banner:
// e.g. "----[ BUILD ]---------------------------------------------------------"
inline TQString banner(const TQString &tag, int width = DefaultBannerWidth) {
    TQString prefix = TQString("----[ %1 ]").arg(tag);
    int dashes = width - (int)prefix.length();
    if (dashes < 3) dashes = 3;
    return prefix + TQString().fill('-', dashes);
}

// Format an operation success footer banner:
// e.g. "----[ ✓ DONE ]----------------------------------------------------------"
inline TQString doneBanner(int width = DefaultBannerWidth) {
    return banner(TQString::fromUtf8("✓ DONE"), width);
}

// Format an operation failure footer banner:
// e.g. "----[ ✗ FAILED ]--------------------------------------------------------"
inline TQString failedBanner(int width = DefaultBannerWidth) {
    return banner(TQString::fromUtf8("✗ FAILED"), width);
}

// Format a result banner based on boolean success flag
inline TQString resultBanner(bool success, int width = DefaultBannerWidth) {
    return success ? doneBanner(width) : failedBanner(width);
}

} // namespace SplashLog

#endif // LOG_BANNER_H
