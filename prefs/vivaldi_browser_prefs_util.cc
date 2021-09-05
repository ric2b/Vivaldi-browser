// Copyright (c) 2020 Vivaldi Technologies

#include "prefs/vivaldi_browser_prefs_util.h"

#include "apps/switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "components/version_info/version_info.h"

namespace vivaldi {

// Sets up a path to a file in source code. Will only return a value
// for developer builds and when using load-and-launch-app.
bool GetDeveloperFilePath(const base::FilePath::StringPieceType& filename,
                          base::FilePath* file) {
#if !defined(OS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(apps::kLoadAndLaunchApp) &&
      !version_info::IsOfficialBuild()) {
    // In a development build allow MakeAbsoluteFilePath on UI thread for
    // convinience.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::FilePath file_from_switch =
        command_line->GetSwitchValuePath(apps::kLoadAndLaunchApp);
    *file = base::MakeAbsoluteFilePath(file_from_switch).Append(filename);
    return true;
  }
#endif  // !OS_ANDROID
  return false;
}

}  // vivaldi