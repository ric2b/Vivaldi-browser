// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"

#include <Windows.h>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/vivaldi_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/browser/upgrade_detector/build_state_observer.h"
#include "chrome/browser/upgrade_detector/installed_version_poller.h"

#include "app/vivaldi_constants.h"
#include "browser/launch_update_notifier.h"
#include "installer/util/vivaldi_install_util.h"
#include "update_notifier/update_notifier_switches.h"

#include "base/debug/stack_trace.h"

namespace extensions {

class AutoUpdateObserver : public BuildStateObserver {
 public:
  AutoUpdateObserver()
      : installed_version_poller_(g_browser_process->GetBuildState()) {
    g_browser_process->GetBuildState()->AddObserver(this);
  }

  static AutoUpdateObserver& GetInstance() {
    static base::NoDestructor<AutoUpdateObserver> instance;
    return *instance;
  }

 private:
  void OnUpdate(const BuildState* build_state) override {
    extensions::AutoUpdateAPI::SendWillInstallUpdateOnQuit();
  }

  InstalledVersionPoller installed_version_poller_;
};

// static
void AutoUpdateAPI::InitUpgradeDetection() {
  AutoUpdateObserver::GetInstance();
}

ExtensionFunction::ResponseAction AutoUpdateCheckForUpdatesFunction::Run() {
  using vivaldi::auto_update::CheckForUpdates::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::CommandLine update_notifier_command =
      ::vivaldi::GetCommonUpdateNotifierCommand(profile);
  update_notifier_command.AppendSwitch(
      ::vivaldi_update_notifier::kCheckForUpdates);
  ::vivaldi::LaunchNotifierProcess(update_notifier_command);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AutoUpdateIsUpdateNotifierEnabledFunction::Run() {
  namespace Results = vivaldi::auto_update::IsUpdateNotifierEnabled::Results;

  bool enabled = ::vivaldi::IsUpdateNotifierEnabled(nullptr);
  return RespondNow(ArgumentList(Results::Create(enabled)));
}

ExtensionFunction::ResponseAction
AutoUpdateEnableUpdateNotifierFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  base::CommandLine update_notifier_command =
      ::vivaldi::GetCommonUpdateNotifierCommand(profile);
  ::vivaldi::EnableUpdateNotifier(update_notifier_command);
  ::vivaldi::LaunchNotifierProcess(update_notifier_command);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AutoUpdateDisableUpdateNotifierFunction::Run() {
  ::vivaldi::DisableUpdateNotifier(nullptr);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AutoUpdateGetAutoInstallUpdatesFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction
AutoUpdateSetAutoInstallUpdatesFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

}  // namespace extensions
