// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_UPDATE_NOTIFIER_SWITCHES_H_
#define UPDATE_NOTIFIER_UPDATE_NOTIFIER_SWITCHES_H_

namespace vivaldi_update_notifier {

// Command line switches for the update notifier.
extern const char kCheckForUpdates[];
extern const char kEnableLogging[];

// Command line switches for debugging not supported in the official build.
extern const char kDebugExeDir[];
extern const char kDebugFirstDelay[];
extern const char kDebugKeepRunning[];
extern const char kDebugSetupExe[];
extern const char kDebugVersion[];

// Prefixes for event names for inter-process communications

extern const wchar_t kCheckForUpdatesEventName[];
extern const wchar_t kQuitEventName[];
extern const wchar_t kGlobalQuitEventName[];
extern const wchar_t kGlobalUniquenessCheckEventName[];

}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_UPDATE_NOTIFIER_SWITCHES_H_
