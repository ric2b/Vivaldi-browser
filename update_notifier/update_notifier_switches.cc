// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "update_notifier/update_notifier_switches.h"

namespace vivaldi_update_notifier {

const char kAutoCheck[] = "auto-check";
const char kBrowserStartup[] = "browser-startup";

// Cryptic on purpose; it is internal.
const char kCheckForUpdates[] = "c";

const char kFromScheduler[] = "from-scheduler";

const char kInstallMode[] = "install-mode";

const char kDisable[] = "disable";
const char kEnable[] = "enable";
const char kIsEnabled[] = "is-enabled";
const char kLaunchIfEnabled[] = "launch-if-enabled";
const char kUnregister[] = "unregister";

#if !defined(OFFICIAL_BUILD)
// Set the version that will be used for update checks and delta downloads.
const char kDebugVersion[] = "debug-version";
#endif

const wchar_t kCheckForUpdatesEventPrefix[] =
    L"Local\\Vivaldi/Update_notifier/Check_for_updates/";
const wchar_t kQuitEventPrefix[] = L"Local\\Vivaldi/Update_notifier/Quit/";
const wchar_t kGlobalQuitEventPrefix[] =
    L"Global\\Vivaldi/Update_notifier/Quit/";
const wchar_t kNetworkInstallerUniquenessEventName[] =
    L"Local\\Vivaldi/NetworkInstaller/Unique";

// The subject name in the Vivaldi certificate that signs the official builds.
const wchar_t kVivaldiSubjectName[] = L"Vivaldi Technologies AS";

#if !defined(OFFICIAL_BUILD)
// The subject name in self-signing certificates in test builds.
const wchar_t kVivaldiTestSubjectName[] = L"Vivaldi testbuild";
#endif

const wchar_t kVivaldiScheduleTaskNamePrefix[] = L"VivaldiUpdateCheck";

}  // namespace vivaldi_update_notifier