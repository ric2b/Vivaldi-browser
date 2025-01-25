// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "browser/startup_vivaldi_browser.h"

#include <string>

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/command_line.h"
#include "browser/launch_update_notifier.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/buildflags/buildflags.h"
#include "prefs/vivaldi_pref_names.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "browser/init_sparkle.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/vivaldi_switches.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_data_service.h"
#include "chrome/browser/sessions/session_data_service_factory.h"
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "components/keep_alive_registry/keep_alive_registry.h"
#endif  // IS_WIN

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::Extension;
#endif

namespace vivaldi {

const char kVivaldiStartupTabUserDataKey[] = "VivaldiStartupTab";

namespace {
// Start copied from chrome\browser\ui\startup\startup_browser_creator_impl.cc

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility functions ----------------------------------------------------------

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(koz): Consolidate this function and remove the special casing.
const Extension* GetPlatformApp(Profile* profile,
                                const std::string& extension_id) {
  const Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::EVERYTHING);
  return extension && extension->is_platform_app() ? extension : NULL;
}

void RecordCmdLineAppHistogram(extensions::Manifest::Type app_type) {
  extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_CMD_LINE_APP,
                                  app_type);
}
#endif
// End copied
}  // anonymous namespace

GURL GetVivaldiNewTabURL() {
  return GURL(vivaldi::kVivaldiNewTabURL);
}

bool LaunchVivaldi(const base::CommandLine& command_line,
                   const base::FilePath& cur_dir,
                   StartupProfileInfo profile_info) {
  if (!IsVivaldiRunning(command_line))
    return false;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Default is now launching the Vivaldi App.
  // TODO(sverrir@vivaldi.com): Remove
  const Extension* extension = GetPlatformApp(profile_info.profile, vivaldi::kVivaldiAppId);
  // If |vivaldi::kVivaldiAppId| is a disabled platform app we handle it
  // specially here, otherwise it will be handled below.
  if (extension) {
    RecordCmdLineAppHistogram(extensions::Manifest::TYPE_PLATFORM_APP);
  }
#endif
  LaunchUpdateNotifier(profile_info.profile);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  apps::AppLaunchParams params(
      extension->name(), apps::LaunchContainer::kLaunchContainerNone,
      WindowOpenDisposition::NEW_WINDOW,
      apps::LaunchSource::kFromChromeInternal);
  params.command_line = command_line;
  params.current_directory = cur_dir;

  ::OpenApplicationWithReenablePrompt(profile_info.profile, std::move(params));

  return true;
#else
  return false;
#endif
}

bool AddVivaldiNewPage(bool welcome_run_none, std::vector<GURL>* startup_urls) {
  if (!IsVivaldiRunning())
    return false;

  if (welcome_run_none) {
    // Don't open speed dial if we will open first run
    startup_urls->push_back(GetVivaldiNewTabURL());
  }

  return true;
}

#if BUILDFLAG(IS_WIN)
void DoCleanShutdownIfNeeded(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kCleanShutdown)) {
    // Make sure we save current session.
    for (Browser* browser : *BrowserList::GetInstance()) {
      browser->profile()->SaveSessionState();
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
      auto* session_data_service =
          SessionDataServiceFactory::GetForProfile(browser->profile());
      if (session_data_service) {
        session_data_service->SetForceKeepSessionState();
      }
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)
    }
    // This will not show the "close window", "exit Vivaldi" and "running
    // downloads" dialogs.
    KeepAliveRegistry::GetInstance()->SetIsShuttingDown(true);
  }
}
#endif  // BUILDFLAG(IS_WIN)



}  // namespace vivaldi
