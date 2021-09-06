// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include <Windows.h>

#include <comutil.h>
#include <taskschd.h>
#include <wrl.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_executor.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/win_util.h"
#include "chrome/install_static/product_install_details.h"
#include "chrome/installer/util/util_constants.h"
#include "components/version_info/version_info_values.h"

#include "app/vivaldi_constants.h"
#include "base/vivaldi_switches.h"
#include "installer/util/vivaldi_install_util.h"
#include "update_notifier/thirdparty/winsparkle/src/config.h"
#include "update_notifier/update_notifier_manager.h"

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")

namespace vivaldi_update_notifier {

namespace {

constexpr HRESULT kNotFoundObjectError = 0x80070002;
constexpr HRESULT kAccessDeniedObjectError = 0x80070005;

// Time to delay the first update check after enabling. Note it cannot be 0 or
// event 1 second. The value is used to construct from the current time the
// daily time moment to run the notifier for Windows task scheduler. When the
// scheduler reacts on the enable event, that time moment for the current day
// may already be in the past if this delay is too short.
constexpr base::TimeDelta kEnableDelay = base::TimeDelta::FromSeconds(10);

enum class Subaction {
  kDisable,
  kEnable,
  kEnableIfUpgrade,
  kIsEnabled,
  kLaunchIfEnabled,
  kUnregister,
};

base::Optional<Subaction> FindIfSubaction(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(kDisable))
    return Subaction::kDisable;
  if (command_line.HasSwitch(kEnable))
    return Subaction::kEnable;
  if (command_line.HasSwitch(kEnableIfUpgrade))
    return Subaction::kEnableIfUpgrade;
  if (command_line.HasSwitch(kIsEnabled))
    return Subaction::kIsEnabled;
  if (command_line.HasSwitch(kLaunchIfEnabled))
    return Subaction::kLaunchIfEnabled;
  if (command_line.HasSwitch(kUnregister))
    return Subaction::kUnregister;
  return base::nullopt;
}

std::wstring TimeToSchedulerString(base::Time t) {
  base::Time::Exploded e;
  t.LocalExplode(&e);
  return base::StringPrintf(L"%04d-%02d-%02dT%02d:%02d:%02d", e.year, e.month,
                            e.day_of_month, e.hour, e.minute, e.second);
}

bool GetSchedulerServiceAndRoot(
    Microsoft::WRL::ComPtr<ITaskService>& pService,
    Microsoft::WRL::ComPtr<ITaskFolder>& pRootFolder) {
  HRESULT hr = CoCreateInstance(
      CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService,
      reinterpret_cast<void**>(pService.GetAddressOf()));
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "Failed to create an instance of ITaskService hr=0x%lx", hr);
    return false;
  }

  hr =
      pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf("ITaskService::Connect failed hr=0x%lx",
                                     hr);
    return false;
  }

  hr = pService->GetFolder(_bstr_t(L"\\"), pRootFolder.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf("ITaskService::GetFolder failed hr=0x%lx",
                                     hr);
    return false;
  }

  return true;
}

std::wstring GetSchedulerTaskName(const base::FilePath& exe_dir) {
  std::wstring task_name(kVivaldiScheduleTaskNamePrefix);
  task_name += '-';
  task_name += winsparkle::GetPathHash(exe_dir, false);
  return task_name;
}

bool GetSchedulerRegisteredTask(
    const base::FilePath& exe_dir,
    Microsoft::WRL::ComPtr<IRegisteredTask>& pRegisteredTask,
    bool& enabled) {
  DCHECK(!pRegisteredTask);
  DCHECK(!enabled);
  Microsoft::WRL::ComPtr<ITaskService> pService;
  Microsoft::WRL::ComPtr<ITaskFolder> pRootFolder;
  if (!GetSchedulerServiceAndRoot(pService, pRootFolder))
    return false;

  std::wstring task_name = GetSchedulerTaskName(exe_dir);
  HRESULT hr = pRootFolder->GetTask(_bstr_t(task_name.c_str()),
                                    pRegisteredTask.GetAddressOf());
  if (FAILED(hr)) {
    if (hr == kNotFoundObjectError || hr == kAccessDeniedObjectError) {
      // No scheduler task, treat as disabled. Access denied happes for users
      // that are not administrators for non-existing tasks.
      return true;
    }

    // Just report the error and proceed as if the task was not found as this is
    // not fatal.
    LOG(ERROR) << base::StringPrintf(
        "ITaskFolder::GetTask failed hr=0x%lx, treat the error as if the task "
        "was not found",
        hr);
    return true;
  }
  VARIANT_BOOL enabled_variant = VARIANT_FALSE;
  hr = pRegisteredTask->get_Enabled(&enabled_variant);
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "IRegisteredTask::get_Enabled failed hr=0x%lx", hr);
    return false;
  }
  if (enabled_variant != VARIANT_FALSE) {
    enabled = true;
  }
  return true;
}

bool EnableSchedulerTask(const base::FilePath& exe_dir,
                         bool create_enabled = true) {
  Microsoft::WRL::ComPtr<ITaskService> pService;
  Microsoft::WRL::ComPtr<ITaskFolder> pRootFolder;
  if (!GetSchedulerServiceAndRoot(pService, pRootFolder))
    return false;

  Microsoft::WRL::ComPtr<ITaskDefinition> pTask;
  HRESULT hr = pService->NewTask(0, pTask.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf("ITaskService::NewTask failed hr=0x%lx",
                                     hr);
    return false;
  }
  Microsoft::WRL::ComPtr<IRegistrationInfo> pRegInfo;
  hr = pTask->get_RegistrationInfo(pRegInfo.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "ITaskDefinition::get_RegistrationInfo failed hr=0x%lx", hr);
    return false;
  }

  hr = pRegInfo->put_Author(_bstr_t(kVivaldiSubjectName));
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "IRegistrationInfo::put_Author failed hr=0x%lx", hr);
    return false;
  }

  Microsoft::WRL::ComPtr<ITriggerCollection> pTriggerCollection;
  hr = pTask->get_Triggers(&pTriggerCollection);
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf("ITask::get_Triggers failed hr=0x%lx", hr);
    return false;
  }

  Microsoft::WRL::ComPtr<ITrigger> pTrigger;
  hr = pTriggerCollection->Create(TASK_TRIGGER_DAILY, pTrigger.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "ITriggerCollection::Create failed hr=0x%lx", hr);
    return false;
  }
  pTriggerCollection.Reset();

  Microsoft::WRL::ComPtr<IDailyTrigger> pDailyTrigger;
  hr = pTrigger->QueryInterface(
      IID_IDailyTrigger,
      reinterpret_cast<void**>(pDailyTrigger.GetAddressOf()));
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "ITrigger::QueryInterface failed for IDailyTrigger hr=0x%lx", hr);
    return false;
  }
  pTrigger.Reset();

  hr = pDailyTrigger->put_Id(_bstr_t(L"DailyCheck"));
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf("IDailyTrigger::put_Id failed hr=0x%lx",
                                     hr);
    return false;
  }

  base::Time start_time = base::Time::Now() + kEnableDelay;
  hr = pDailyTrigger->put_StartBoundary(
      _bstr_t(TimeToSchedulerString(start_time).c_str()));
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "IDailyTrigger::put_StartBoundary failed hr=0x%lx", hr);
    return false;
  }

  // Repeat each day.
  hr = pDailyTrigger->put_DaysInterval(1);
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "IDailyTrigger::put_DaysInterval failed hr=0x%lx", hr);
    return false;
  }

  Microsoft::WRL::ComPtr<IActionCollection> pActionCollection;
  hr = pTask->get_Actions(pActionCollection.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "ITaskDefinition::get_Actions failed hr=0x%lx", hr);
    return false;
  }

  Microsoft::WRL::ComPtr<IAction> pAction;
  hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "IActionCollection::Create failed hr=0x%lx", hr);
    return false;
  }
  pActionCollection.Reset();

  Microsoft::WRL::ComPtr<IExecAction> pExecAction;
  hr = pAction->QueryInterface(
      IID_IExecAction, reinterpret_cast<void**>(pExecAction.GetAddressOf()));
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "IAction::QueryInterface failed for IExecAction hr=0x%lx", hr);
    return false;
  }
  pAction.Reset();

  base::CommandLine task_command =
      vivaldi::GetCommonUpdateNotifierCommand(&exe_dir);
  task_command.AppendSwitch(kFromScheduler);

  hr =
      pExecAction->put_Path(_bstr_t(task_command.GetProgram().value().c_str()));
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf("IExecAction::put_Path failed hr=0x%lx",
                                     hr);
    return false;
  }

  hr = pExecAction->put_Arguments(
      _bstr_t(task_command.GetArgumentsString().c_str()));
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "IExecAction::put_Arguments failed hr=0x%lx", hr);
    return false;
  }

  hr = pExecAction->put_WorkingDirectory(_bstr_t(exe_dir.value().c_str()));
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "IExecAction::put_WorkingDirectory failed hr=0x%lx", hr);
    return false;
  }

  Microsoft::WRL::ComPtr<ITaskSettings> pTaskSettings;
  hr = pTask->get_Settings(pTaskSettings.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "ITaskDefinition::get_Settings failed hr=0x%lx", hr);
    return false;
  }

  // Make sure that if the time to run happens during the suspend, the task will
  // still run after the wake up.
  hr = pTaskSettings->put_StartWhenAvailable(VARIANT_TRUE);
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "ITaskSettings::put_StartWhenAvailable failed hr=0x%lx", hr);
    return false;
  }

  // Make sure that we run both on the battery and plugged in.
  hr = pTaskSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "ITaskSettings::put_DisallowStartIfOnBatteries failed hr=0x%lx", hr);
    return false;
  }

  std::wstring task_name = GetSchedulerTaskName(exe_dir);

  Microsoft::WRL::ComPtr<IRegisteredTask> pRegisteredTask;
  hr = pRootFolder->RegisterTaskDefinition(
      _bstr_t(task_name.c_str()), pTask.Get(), TASK_CREATE_OR_UPDATE,
      _variant_t(), _variant_t(), TASK_LOGON_INTERACTIVE_TOKEN, _variant_t(L""),
      pRegisteredTask.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "ITaskFolder::RegisterTaskDefinition failed hr=0x%lx", hr);
    return false;
  }

  hr = pRegisteredTask->put_Enabled(create_enabled ? VARIANT_TRUE
                                                   : VARIANT_FALSE);
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf(
        "IRegisteredTask::put_Enabled failed hr=0x%lx", hr);
    return false;
  }

  if (create_enabled) {
    LOG(INFO) << "Update notifier was enabled as task " << task_name;
  } else {
    LOG(INFO) << "Update notifier task " << task_name
              << " was created but the was left as disabled";
  }
  return true;
}

bool DisableSchedulerTask(const base::FilePath& exe_dir) {
  bool enabled = false;
  Microsoft::WRL::ComPtr<IRegisteredTask> pRegisteredTask;
  if (!GetSchedulerRegisteredTask(exe_dir, pRegisteredTask, enabled))
    return false;
  if (enabled) {
    HRESULT hr = pRegisteredTask->put_Enabled(VARIANT_FALSE);
    if (FAILED(hr)) {
      LOG(ERROR) << base::StringPrintf(
          "IRegisteredTask::put_Enabled failed hr=0x%lx", hr);
      return false;
    }
  }
  LOG(INFO) << "Update notifier was disabled";
  return true;
}

// Check for older autorun settings for the update notifier.
bool IsUpdateNotifierEnabledAsAutorun(const base::FilePath& exe_dir,
                                      bool& enabled_for_some_installations) {
  DCHECK(!enabled_for_some_installations);
  std::wstring command;
  if (!base::win::ReadCommandFromAutoRun(
          HKEY_CURRENT_USER, vivaldi::kUpdateNotifierAutorunName, &command)) {
    return false;
  }
  enabled_for_some_installations = true;
  base::CommandLine cmdline = base::CommandLine::FromString(command);
  base::FilePath registry_exe = cmdline.GetProgram().NormalizePathSeparators();

  base::FilePath exe_path = vivaldi::GetUpdateNotifierPath(&exe_dir);
  return base::FilePath::CompareEqualIgnoreCase(registry_exe.value(),
                                                exe_path.value());
}

bool EnableSchedulerTaskIfUpgrade(const base::FilePath& exe_dir) {
  bool enabled = false;
  Microsoft::WRL::ComPtr<IRegisteredTask> pRegisteredTask;
  if (!GetSchedulerRegisteredTask(exe_dir, pRegisteredTask, enabled))
    return false;
  if (pRegisteredTask) {
    // The task already exists, leave it alone to keep enabled or disabled
    // status.
    DLOG(INFO) << "Task " << GetSchedulerTaskName(exe_dir) << " already exists";
    return true;
  }

  bool enabled_for_some_installations = false;
  if (IsUpdateNotifierEnabledAsAutorun(exe_dir,
                                       enabled_for_some_installations)) {
    // Upgrade Autorun-based notifier to the new Scheduler form.
    if (!EnableSchedulerTask(exe_dir))
      return false;
    // The result is false when the autorun entry does not exist.
    if (base::win::RemoveCommandFromAutoRun(
            HKEY_CURRENT_USER, vivaldi::kUpdateNotifierAutorunName)) {
      LOG(INFO) << "Removed autorun entry for "
                << vivaldi::kUpdateNotifierAutorunName;
    }
  } else {
    // With older autorun-based update checks enabling them for one installation
    // disabled them for others. So in this case enable the update checks as we
    // do not know if the user really wanted to disable the checks. In other
    // cases when the autorun is not set at all assume that the user really
    // disabled the notifier. In this case just create disabled task.
    bool create_enabled = enabled_for_some_installations;
    if (!EnableSchedulerTask(exe_dir, create_enabled))
      return false;
  }

  return true;
}

bool IsEnabledSchedulerTask(const base::FilePath& exe_dir, bool& enabled) {
  DCHECK(!enabled);
  Microsoft::WRL::ComPtr<IRegisteredTask> pRegisteredTask;
  bool ok = GetSchedulerRegisteredTask(exe_dir, pRegisteredTask, enabled);
  if (!ok)
    return false;
  if (pRegisteredTask)
    return true;
  DCHECK(!enabled);
  if (winsparkle::g_install_type != vivaldi::InstallType::kForAllUsers)
    return true;

  // We may run as non-administrator. We should check for the older autorun
  // settings. For single-user installs this is done during installation, see
  // EnableSchedulerTaskIfUpgrade.
  bool enabled_for_some_installations = false;
  if (IsUpdateNotifierEnabledAsAutorun(exe_dir,
                                       enabled_for_some_installations)) {
    // Upgrade Autorun-based notifier to the new Scheduler form.
    if (!EnableSchedulerTask(exe_dir))
      return false;
    enabled = true;
    // The result is false when the autorun entry does not exist.
    if (base::win::RemoveCommandFromAutoRun(
            HKEY_CURRENT_USER, vivaldi::kUpdateNotifierAutorunName)) {
      LOG(INFO) << "Removed autorun entry for "
                << vivaldi::kUpdateNotifierAutorunName;
    }
  }
  return true;
}

bool UnregisterSchedulerTask(const base::FilePath& exe_dir) {
  Microsoft::WRL::ComPtr<ITaskService> pService;
  Microsoft::WRL::ComPtr<ITaskFolder> pRootFolder;
  if (!GetSchedulerServiceAndRoot(pService, pRootFolder))
    return false;
  std::wstring task_name = GetSchedulerTaskName(exe_dir);
  HRESULT hr = pRootFolder->DeleteTask(_bstr_t(task_name.c_str()), 0);
  if (FAILED(hr)) {
    if (hr == kNotFoundObjectError) {
      DLOG(INFO) << "Scheduler task " << task_name << " does not exist";
    } else {
      LOG(ERROR) << base::StringPrintf(
          "ITaskFolder::DeleteTask failed hr=0x%lx", hr);
      return false;
    }
  }
  return true;
}

ExitCode RunTaskSchedulerSubAction(Subaction subaction,
                                   const base::FilePath& exe_dir,
                                   bool& fallthrough_to_main_action) {
  DCHECK(!fallthrough_to_main_action);
  // This is based on
  // https://docs.microsoft.com/en-us/windows/win32/taskschd/daily-trigger-example--c---

  // Initialize COM and its security.
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);

  HRESULT hr = ::CoInitializeSecurity(
      nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr);
  if (FAILED(hr)) {
    LOG(ERROR) << base::StringPrintf("CoInitializeSecurity failed hr=0x%lx",
                                     hr);
    return ExitCode::kError;
  }

  switch (subaction) {
    case Subaction::kEnable:
      if (!EnableSchedulerTask(exe_dir))
        return ExitCode::kError;
      break;
    case Subaction::kEnableIfUpgrade:
      if (!EnableSchedulerTaskIfUpgrade(exe_dir))
        return ExitCode::kError;
      break;
    case Subaction::kDisable:
      if (!DisableSchedulerTask(exe_dir))
        return ExitCode::kError;
      vivaldi::QuitAllUpdateNotifiers(exe_dir, /*quit_old=*/false);
      break;
    case Subaction::kIsEnabled:
    case Subaction::kLaunchIfEnabled: {
      bool enabled = false;
      if (!IsEnabledSchedulerTask(exe_dir, enabled))
        return ExitCode::kError;
      if (!enabled)
        return ExitCode::kDisabled;
      if (subaction == Subaction::kLaunchIfEnabled) {
        // Simulate invocation from the task scheduler.
        base::CommandLine::ForCurrentProcess()->AppendSwitch(kFromScheduler);
        fallthrough_to_main_action = true;
      }
      break;
    }
    case Subaction::kUnregister: {
      bool ok = UnregisterSchedulerTask(exe_dir);
      vivaldi::QuitAllUpdateNotifiers(exe_dir, /*quit_old=*/false);
      if (!ok)
        return ExitCode::kError;
      break;
    }
  }

  return ExitCode::kOk;
}

ExitCode RunSubAction(Subaction action,
                      const base::FilePath& exe_dir,
                      bool& fallthrough_to_main_action) {
  DCHECK(!fallthrough_to_main_action);
  DLOG(INFO) << "Task scheduler subaction " << static_cast<int>(action);
  if (kUseTaskScheduler) {
    return RunTaskSchedulerSubAction(action, exe_dir,
                                     fallthrough_to_main_action);
  }
  switch (action) {
    case Subaction::kDisable: {
      if (!vivaldi::DisableUpdateNotifier(&exe_dir))
        return ExitCode::kError;
      break;
    }
    case Subaction::kEnable: {
      base::CommandLine cmdline =
          vivaldi::GetCommonUpdateNotifierCommand(&exe_dir);
      if (!vivaldi::EnableUpdateNotifier(cmdline))
        return ExitCode::kError;
      if (!vivaldi::LaunchNotifierProcess(cmdline))
        return ExitCode::kError;
      break;
    }
    case Subaction::kEnableIfUpgrade:
      LOG(ERROR) << "Unsupported switch --" << kEnableIfUpgrade;
      return ExitCode::kError;
    case Subaction::kIsEnabled:
      if (!vivaldi::IsUpdateNotifierEnabled(&exe_dir))
        return ExitCode::kDisabled;
      break;
    case Subaction::kLaunchIfEnabled: {
      if (!vivaldi::IsUpdateNotifierEnabled(&exe_dir))
        return ExitCode::kDisabled;
      fallthrough_to_main_action = true;
      break;
    }
    case Subaction::kUnregister: {
      bool ok = vivaldi::DisableUpdateNotifier(&exe_dir);
      vivaldi::QuitAllUpdateNotifiers(exe_dir, /*quit_old=*/false);
      if (!ok)
        return ExitCode::kError;
      break;
    }
  }
  return ExitCode::kOk;
}

void InitLog(const base::CommandLine& command_line) {
  bool enabled = false;
  std::wstring log_file;
  if (command_line.HasSwitch(installer::switches::kEnableLogging)) {
    enabled = true;
    log_file =
        command_line.GetSwitchValueNative(installer::switches::kEnableLogging);
  } else if (command_line.HasSwitch(switches::kVivaldiUpdateURL)) {
    // For convenience activate logging with --vuu to the default file.
    enabled = true;
  }

  if (!enabled) {
    // Unless disabled explicitly Chromium logs by default to default.log in the
    // exe directory.
    logging::LoggingSettings settings;
    settings.logging_dest = logging::LOG_NONE;
    logging::InitLogging(settings);
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    return;
  }

  if (log_file.empty()) {
    base::FilePath temp_dir;
    if (base::GetTempDir(&temp_dir)) {
      log_file = temp_dir.value();
      log_file += L"\\vivaldi_installer.log";
    }
  }
  base::RouteStdioToConsole(false);

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_NONE;
  if (DCHECK_IS_ON()) {
    settings.logging_dest |= logging::LOG_TO_SYSTEM_DEBUG_LOG;
  }
  settings.logging_dest |= logging::LOG_TO_STDERR;
  if (!log_file.empty()) {
    settings.logging_dest |= logging::LOG_TO_FILE;
    settings.log_file_path = log_file.c_str();
  }
  logging::InitLogging(settings);
}

}  // namespace

ExitCode WinMainImpl(HINSTANCE instance, HINSTANCE prev) {
  base::CommandLine::Init(0, nullptr);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  InitLog(command_line);

  // Make the first log entry for the new process visibly separated.
  LOG(INFO) << "\n\n"
            << "*** " << VIVALDI_VERSION << " ***";
  LOG(INFO) << command_line.GetCommandLineString();

  // Check for switch combinations that should not be used together.
  int unique_switches = 0;

  base::FilePath exe_dir;
  if (command_line.HasSwitch(kInstallMode)) {
    unique_switches++;
    winsparkle::g_install_mode = true;
  } else {
    base::FilePath exe_path = vivaldi::GetPathOfCurrentExe();
    std::wstring exe_name = exe_path.BaseName().value();
    if (_wcsicmp(exe_name.c_str(),
                 vivaldi::constants::kVivaldiUpdateNotifierExe) != 0 &&
        _wcsicmp(exe_name.c_str(),
                 vivaldi::constants::kVivaldiUpdateNotifierOldExe) != 0) {
      // We are not called update notifier, assume a network installation mode.
      winsparkle::g_install_mode = true;
    } else {
      exe_dir = exe_path.DirName();
#if !defined(OFFICIAL_BUILD)
      std::wstring debug_exe_dir =
          command_line.GetSwitchValueNative(kDebugExeDir);
      if (!debug_exe_dir.empty()) {
        exe_dir = vivaldi::NormalizeInstallExeDirectory(
            base::FilePath(debug_exe_dir));
      }
#endif

      // Make sure to use a working directory not pointing to the
      // version-specific sub-directory of the installation. This way if the
      // installer cannot kill the notifier for some reason during the update,
      // at least it will still be possible to delete the old version-specific
      // directory.
      base::SetCurrentDirectory(exe_dir);

      base::Optional<vivaldi::InstallType> existing_install_type =
          vivaldi::FindInstallType(exe_dir.DirName());
      if (existing_install_type) {
        winsparkle::g_install_type = *existing_install_type;
      }
    }
  }

  if (winsparkle::g_install_mode) {
    // The network installer is always a manual action by the user.
    winsparkle::g_manual_check = true;
  }

  if (command_line.HasSwitch(switches::kVivaldiSilentUpdate)) {
    unique_switches++;
    if (winsparkle::g_install_type != vivaldi::InstallType::kForCurrentUser) {
      LOG(INFO)
          << "Ignoring --" << switches::kVivaldiSilentUpdate
          << " as it is not supported for stanalone or system installations";
    } else {
      winsparkle::g_silent_update = true;
    }
  }

  if (command_line.HasSwitch(kCheckForUpdates)) {
    unique_switches++;
    winsparkle::g_manual_check = true;
  }

  base::Optional<Subaction> subaction = FindIfSubaction(command_line);
  if (subaction) {
    unique_switches++;
  }

  if (unique_switches > 1) {
    LOG(ERROR) << "Incompatible combination of switches";
    return ExitCode::kError;
  }

  if (subaction) {
    bool fallthrough_to_main_action = false;
    ExitCode exit_code =
        RunSubAction(*subaction, exe_dir, fallthrough_to_main_action);
    if (exit_code != ExitCode::kOk || !fallthrough_to_main_action)
      return exit_code;
  }

  base::AtExitManager at_exit;
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::UI);

  install_static::InitializeProductDetailsForPrimaryModule();

  return UpdateNotifierManager::GetInstance().RunNotifier(std::move(exe_dir));
}

}  // namespace vivaldi_update_notifier

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  vivaldi_update_notifier::ExitCode exit_code =
      vivaldi_update_notifier::WinMainImpl(instance, prev);
  LOG(INFO) << "exit_code=" << static_cast<int>(exit_code);

  // Directly call ExitProcess() to skip any C++ global destructors in wxWidgets
  // and terminate all threads immediately.
  ExitProcess(static_cast<int>(exit_code));
}
