/*
 * vt_runner_helper.h - Helper for executing safe hardware VT bootsplash tests
 */

#ifndef VT_RUNNER_HELPER_H
#define VT_RUNNER_HELPER_H

#include <ntqstring.h>
#include <ntqwidget.h>

TQString findVtRunnerBinary(const TQString &projectRoot);

bool executeVtLiveTest(TQWidget *parent,
                       const TQString &projectRoot,
                       const TQString &binaryPath,
                       const TQString &displayName,
                       TQString &outLogOutput);

#endif // VT_RUNNER_HELPER_H
