// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/auto_update/auto_update_api.h"

#include <Windows.h>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/vivaldi_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/browser/upgrade_detector/build_state_observer.h"
#include "chrome/browser/upgrade_detector/installed_version_poller.h"
#include "extensions/api/auto_update/auto_update_status.h"
#include "extensions/tools/vivaldi_tools.h"

#include "app/vivaldi_constants.h"
#include "browser/launch_update_notifier.h"
#include "installer/util/vivaldi_install_util.h"
#include "update_notifier/update_notifier_switches.h"

#include "base/debug/stack_trace.h"

namespace extensions {

namespace {

void StartManualUpdateCheck() {
  base::CommandLine update_notifier_command =
      ::vivaldi::GetCommonUpdateNotifierCommand();
  update_notifier_command.AppendSwitch(
      ::vivaldi_update_notifier::kCheckForUpdates);
  ::vivaldi::LaunchNotifierProcess(update_notifier_command);
}

bool IsUpdateNotifierEnabledFromBrowser() {
  base::CommandLine cmdline = ::vivaldi::GetCommonUpdateNotifierCommand();
  cmdline.AppendSwitch(vivaldi_update_notifier::kIsEnabled);
  int exit_code = ::vivaldi::RunNotifierSubaction(cmdline);
  if (exit_code != 0 &&
      exit_code !=
          static_cast<int>(vivaldi_update_notifier::ExitCode::kDisabled)) {
    LOG(ERROR)
        << "Failed to query update notifier for enabled status, exit_code="
        << exit_code;
    return false;
  }
  return exit_code == 0;
}

bool DisableUpdateNotifierFromBrowser() {
  base::CommandLine cmdline = ::vivaldi::GetCommonUpdateNotifierCommand();
  cmdline.AppendSwitch(vivaldi_update_notifier::kDisable);
  int exit_code = ::vivaldi::RunNotifierSubaction(cmdline);
  if (exit_code != 0) {
    LOG(ERROR) << "Failed to disable update notifier, exit_code=" << exit_code;
    return false;
  }
  return true;
}

bool EnableUpdateNotifierFromBrowser() {
  base::CommandLine cmdline = ::vivaldi::GetCommonUpdateNotifierCommand();
  cmdline.AppendSwitch(vivaldi_update_notifier::kEnable);
  int exit_code = ::vivaldi::RunNotifierSubaction(cmdline);
  if (exit_code != 0) {
    LOG(ERROR) << "Failed to enable update notifier, exit_code=" << exit_code;
    return false;
  }
  return true;
}

}  // namespace

class AutoUpdateObserver : public BuildStateObserver {
 public:
  static AutoUpdateObserver& GetInstance() {
    static base::NoDestructor<AutoUpdateObserver> instance;
    return *instance;
  }

 private:
  void OnUpdate(const BuildState* build_state) override {
    std::optional<base::Version> version = build_state->installed_version();
    extensions::AutoUpdateAPI::SendWillInstallUpdateOnQuit(
        version.value_or(base::Version()));
  }

  InstalledVersionPoller installed_version_poller_{
      g_browser_process->GetBuildState()};
};

// static
void AutoUpdateAPI::InitUpgradeDetection() {
  // In case of multiple profiles.
  if (!g_browser_process->GetBuildState()->HasObserver(
          &AutoUpdateObserver::GetInstance())) {
    g_browser_process->GetBuildState()->AddObserver(
        &AutoUpdateObserver::GetInstance());
  }
}

// static
void AutoUpdateAPI::ShutdownUpgradeDetection() {
  g_browser_process->GetBuildState()->RemoveObserver(
      &AutoUpdateObserver::GetInstance());
}

ExtensionFunction::ResponseAction AutoUpdateCheckForUpdatesFunction::Run() {
  using vivaldi::auto_update::CheckForUpdates::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&StartManualUpdateCheck),
      base::BindOnce(&AutoUpdateCheckForUpdatesFunction::DeliverResult, this));
  return RespondLater();
}

void AutoUpdateCheckForUpdatesFunction::DeliverResult() {
  Respond(NoArguments());
}

ExtensionFunction::ResponseAction
AutoUpdateIsUpdateNotifierEnabledFunction::Run() {
  if (::vivaldi::IsStandaloneBrowser()) {
    bool enabled = ::vivaldi::IsStandaloneAutoUpdateEnabled();
    DeliverResult(enabled);
    return AlreadyResponded();
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&IsUpdateNotifierEnabledFromBrowser),
      base::BindOnce(&AutoUpdateIsUpdateNotifierEnabledFunction::DeliverResult,
                     this));
  return RespondLater();
}

void AutoUpdateIsUpdateNotifierEnabledFunction::DeliverResult(bool enabled) {
  namespace Results = vivaldi::auto_update::IsUpdateNotifierEnabled::Results;

  Respond(ArgumentList(Results::Create(enabled)));
}

ExtensionFunction::ResponseAction
AutoUpdateEnableUpdateNotifierFunction::Run() {
  if (::vivaldi::IsStandaloneBrowser()) {
    ::vivaldi::EnableStandaloneAutoUpdate();
    DeliverResult(true);
    return AlreadyResponded();
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&EnableUpdateNotifierFromBrowser),
      base::BindOnce(&AutoUpdateEnableUpdateNotifierFunction::DeliverResult,
                     this));
  return RespondLater();
}

void AutoUpdateEnableUpdateNotifierFunction::DeliverResult(bool success) {
  namespace Results = vivaldi::auto_update::EnableUpdateNotifier::Results;

  Respond(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction
AutoUpdateDisableUpdateNotifierFunction::Run() {
  if (::vivaldi::IsStandaloneBrowser()) {
    ::vivaldi::DisableStandaloneAutoUpdate();
    DeliverResult(true);
    return AlreadyResponded();
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&DisableUpdateNotifierFromBrowser),
      base::BindOnce(&AutoUpdateDisableUpdateNotifierFunction::DeliverResult,
                     this));
  return RespondLater();
}

void AutoUpdateDisableUpdateNotifierFunction::DeliverResult(bool success) {
  namespace Results = vivaldi::auto_update::DisableUpdateNotifier::Results;

  Respond(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction
AutoUpdateInstallUpdateAndRestartFunction::Run() {
  ::vivaldi::RestartBrowser();
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

ExtensionFunction::ResponseAction AutoUpdateGetLastCheckTimeFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction AutoUpdateGetUpdateStatusFunction::Run() {
  auto on_pending_update_result =
      [](scoped_refptr<AutoUpdateGetUpdateStatusFunction> f,
         std::optional<base::Version> version) {
        std::optional<AutoUpdateStatus> status;
        std::string version_string;
        if (!version) {
          status = AutoUpdateStatus::kNoUpdate;
        } else if (!version->IsValid()) {
          status = std::nullopt;
        } else {
          status = AutoUpdateStatus::kWillInstallUpdateOnQuit;
          version_string = version->GetString();
        }
        f->SendResult(status, std::move(version_string), std::string());
      };

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&::vivaldi::GetPendingUpdateVersion, base::FilePath()),
      base::BindOnce(on_pending_update_result,
                     scoped_refptr<AutoUpdateGetUpdateStatusFunction>(this)));

  return RespondLater();
}

bool AutoUpdateHasAutoUpdatesFunction::HasAutoUpdates() {
  // Silent auto updates are not supported on windows system installs unless a
  // very experimental --vsu flag is passed to the browser.
  return ::vivaldi::GetBrowserInstallType() !=
             ::vivaldi::InstallType::kForAllUsers ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kVivaldiSilentUpdate);
}

ExtensionFunction::ResponseAction AutoUpdateNeedsCodecRestartFunction::Run() {
  namespace Results = vivaldi::auto_update::NeedsCodecRestart::Results;
  return RespondNow(ArgumentList(Results::Create(false)));
}

}  // namespace extensions
