// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "update_notifier/update_notifier_switches.h"

namespace vivaldi_update_notifier {

// Cryptic on purpose; it is internal.
const char kCheckForUpdates[] = "c";

const char kEnableLogging[] = "enable-logging";

#if !defined(OFFICIAL_BUILD)
// Set the the directory to look for the installer for delta updates.
const char kDebugExeDir[] = "debug-exe-dir";

// The pause in seconds to wait before starting the first automated check.
const char kDebugFirstDelay[] = "debug-first-delay";

// Keep the process running even when automatic option to check for updates is
// disabled.
const char kDebugKeepRunning[] = "debug-keep-running";

// The path of setup.exe to use to install a delta download.
const char kDebugSetupExe[] = "debug-setup-exe";

// Set the version that will be used for update checks and delta downloads.
const char kDebugVersion[] = "debug-version";
#endif

const wchar_t kCheckForUpdatesEventName[] =
    L"Local\\Vivaldi/Update_notifier/Check_for_updates/";
const wchar_t kQuitEventName[] = L"Local\\Vivaldi/Update_notifier/Quit/";
const wchar_t kGlobalQuitEventName[] = L"Global\\Vivaldi/Update_notifier/Quit/";
const wchar_t kGlobalUniquenessCheckEventName[] =
    L"Global\\Vivaldi/Update_notifier/Unique/";

}  // namespace vivaldi_update_notifier