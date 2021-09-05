// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "update_notifier/update_notifier_switches.h"

namespace vivaldi_update_notifier {

// Cryptic on purpose; these are internal.
const char kCheckForUpdates[] = "c";
const char kQuit[] = "q";

#if !defined(OFFICIAL_BUILD)
// Set the the directory to look for the installer for delta updates.
const char kUpdateExeDir[] = "update-exe-dir";

// Keep the process running even when automatic option to check for updates is
// disabled.
const char kUpdateKeepRunning[] = "update-keep-running";

// Set the version that will be used for update checks and delta downloads.
const char kUpdateVersion[] = "update-version";

#endif
}  // namespace vivaldi_update_notifier