// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef UPDATE_NOTIFIER_UPDATE_NOTIFIER_SWITCHES_H_
#define UPDATE_NOTIFIER_UPDATE_NOTIFIER_SWITCHES_H_

namespace vivaldi_update_notifier {

enum class ExitCode {
  kOk = 0,

  // Generic error
  kError = 1,

  // The notifier process already runs.
  kAlreadyRuns = 20,

  // Automatic updates are disabled.
  kDisabled = 21,
};

// Command line switches for the update notifier.

// Automatic update check.
extern const char kAutoCheck[];

// Mark the update check running on the browser startup.
extern const char kBrowserStartup[];

// Manually check for update.
extern const char kCheckForUpdates[];

// Mark invocation from Windows Task Scheduler.
extern const char kFromScheduler[];

extern const char kInstallMode[];

// Switches for subactions
extern const char kDisable[];
extern const char kEnable[];
extern const char kIsEnabled[];
extern const char kLaunchIfEnabled[];
extern const char kUnregister[];

// Command line switches for debugging not supported in the official build.
extern const char kDebugVersion[];

// Prefixes for event names for inter-process communications
extern const wchar_t kCheckForUpdatesEventPrefix[];
extern const wchar_t kQuitEventPrefix[];
extern const wchar_t kGlobalQuitEventPrefix[];
extern const wchar_t kNetworkInstallerUniquenessEventName[];

extern const wchar_t kVivaldiSubjectName[];
extern const wchar_t kVivaldiTestSubjectName[];

extern const wchar_t kVivaldiScheduleTaskNamePrefix[];

}  // namespace vivaldi_update_notifier

#endif  // UPDATE_NOTIFIER_UPDATE_NOTIFIER_SWITCHES_H_
