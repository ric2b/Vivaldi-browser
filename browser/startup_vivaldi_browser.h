// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_STARTUP_VIVALDI_BROWSER_H_
#define BROWSER_STARTUP_VIVALDI_BROWSER_H_

#include <vector>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/command_line.h"
#endif // IS_WIN
#include "base/supports_user_data.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

class Profile;

namespace vivaldi {

extern const char kVivaldiStartupTabUserDataKey[];

bool LaunchVivaldi(const base::CommandLine& command_line,
                   const base::FilePath& cur_dir,
                   StartupProfileInfo profile_info);
bool AddVivaldiNewPage(bool welcome_run_none, std::vector<GURL>* startup_urls);

#if BUILDFLAG(IS_WIN)
void DoCleanShutdownIfNeeded(const base::CommandLine& command_line);
#endif // IS_WIN

class VivaldiStartupTabUserData : public base::SupportsUserData::Data {
 public:
  VivaldiStartupTabUserData(bool start_as_active)
      : start_as_active_(start_as_active) {}
  bool start_as_active() { return start_as_active_; }

 private:
  bool start_as_active_ = false;
};

}  // namespace vivaldi

#endif  // BROWSER_STARTUP_VIVALDI_BROWSER_H_
