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
#include "base/strings/utf_string_conversions.h"
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

std::wstring GetSchedulerTaskName() {
  std::wstring task_name(kVivaldiScheduleTaskNamePrefix);
  task_name += '-';
  task_name += winsparkle::GetExeDirHash(false);
  return task_name;
}

bool GetSchedulerRegisteredTask(
    Microsoft::WRL::ComPtr<IRegisteredTask>& pRegisteredTask,
    bool& enabled) {
  DCHECK(!pRegisteredTask);
  DCHECK(!enabled);
  Microsoft::WRL::ComPtr<ITaskService> pService;
  Microsoft::WRL::ComPtr<ITaskFolder> pRootFolder;
  if (!GetSchedulerServiceAndRoot(pService, pRootFolder))
    return false;

  std::wstring task_name = GetSchedulerTaskName();
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

bool EnableSchedulerTask(bool create_enabled = true) {
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
      vivaldi::GetCommonUpdateNotifierCommand(winsparkle::GetExeDir());
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

  hr = pExecAction->put_WorkingDirectory(
      _bstr_t(winsparkle::GetExeDir().value().c_str()));
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

  std::wstring task_name = GetSchedulerTaskName();

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

bool DisableSchedulerTask() {
  bool enabled = false;
  Microsoft::WRL::ComPtr<IRegisteredTask> pRegisteredTask;
  if (!GetSchedulerRegisteredTask(pRegisteredTask, enabled))
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

bool IsEnabledSchedulerTask(bool check_for_autorun, bool& enabled) {
  DCHECK(!enabled);
  Microsoft::WRL::ComPtr<IRegisteredTask> pRegisteredTask;
  bool ok = GetSchedulerRegisteredTask(pRegisteredTask, enabled);
  if (!ok)
    return false;
  if (pRegisteredTask || !check_for_autorun)
    return true;

  // No task was created, check if we have an older autorun registry entry.
  //
  // With the autorun we supported an auto update only for single installation
  // per user. With multiple installations enabling an update for one disabled
  // updates for others. With the task scheduler we support independent updates.
  // So always enable the task scheduler if any installation wants updates. The
  // user may disable the individual notifications later.
  if (!vivaldi::IsUpdateNotifierEnabledAsAutorunForAnyPath()) {
    if (!EnableSchedulerTask())
      return false;
    enabled = true;
  }

  // Remove the no longer needed autorun entry.
  vivaldi::DisableUpdateNotifierAsAutorun(winsparkle::GetExeDir());
  return true;
}

bool UnregisterSchedulerTask() {
  Microsoft::WRL::ComPtr<ITaskService> pService;
  Microsoft::WRL::ComPtr<ITaskFolder> pRootFolder;
  if (!GetSchedulerServiceAndRoot(pService, pRootFolder))
    return false;
  std::wstring task_name = GetSchedulerTaskName();
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
      if (!EnableSchedulerTask())
        return ExitCode::kError;
      break;
    case Subaction::kDisable:
      if (!DisableSchedulerTask())
        return ExitCode::kError;
      vivaldi::SendQuitUpdateNotifier(winsparkle::GetExeDir());
      break;
    case Subaction::kIsEnabled: {
      bool enabled = false;
      if (!IsEnabledSchedulerTask(/*check_for_autorun=*/false, enabled))
        return ExitCode::kError;
      if (!enabled)
        return ExitCode::kDisabled;
      break;
    }
    case Subaction::kLaunchIfEnabled: {
      bool enabled = false;
      if (!IsEnabledSchedulerTask(/*check_for_autorun=*/true, enabled))
        return ExitCode::kError;
      if (!enabled)
        return ExitCode::kDisabled;

      // Simulate invocation from the task scheduler.
      base::CommandLine::ForCurrentProcess()->AppendSwitch(kFromScheduler);
      fallthrough_to_main_action = true;
      break;
    }
    case Subaction::kUnregister: {
      bool ok = UnregisterSchedulerTask();
      if (!ok)
        return ExitCode::kError;
      break;
    }
  }

  return ExitCode::kOk;
}

ExitCode RunSubAction(Subaction action, bool& fallthrough_to_main_action) {
  DCHECK(!fallthrough_to_main_action);
  DLOG(INFO) << "Task scheduler subaction " << static_cast<int>(action);
  if (winsparkle::IsUsingTaskScheduler()) {
    return RunTaskSchedulerSubAction(action, fallthrough_to_main_action);
  }
  base::FilePath exe_dir = winsparkle::GetExeDir();
  switch (action) {
    case Subaction::kDisable:
    case Subaction::kUnregister: {
      if (!vivaldi::DisableUpdateNotifierAsAutorun(exe_dir))
        return ExitCode::kError;
      break;
    }
    case Subaction::kEnable: {
      if (!vivaldi::EnableUpdateNotifierWithAutorun(exe_dir))
        return ExitCode::kError;
      break;
    }
    case Subaction::kIsEnabled:
      if (!vivaldi::IsUpdateNotifierEnabledAsAutorun(exe_dir))
        return ExitCode::kDisabled;
      break;
    case Subaction::kLaunchIfEnabled: {
      if (!vivaldi::IsUpdateNotifierEnabledAsAutorun(exe_dir))
        return ExitCode::kDisabled;
      fallthrough_to_main_action = true;
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

  base::FilePath current_exe_path = vivaldi::GetPathOfCurrentExe();
  if (command_line.HasSwitch(vivaldi::constants::kVivaldiInstallDir)) {
    winsparkle::g_install_dir =
        command_line.GetSwitchValuePath(vivaldi::constants::kVivaldiInstallDir);
  }

  // Check for switch combinations that should not be used together.
  int unique_switches = 0;

  if (command_line.HasSwitch(kInstallMode)) {
    unique_switches++;
    winsparkle::g_install_mode = true;
  }
  if (!winsparkle::g_install_mode) {
    base::FilePath update_install_dir =
        winsparkle::g_install_dir.empty() ? current_exe_path.DirName().DirName()
                                          : winsparkle::g_install_dir;

    base::Optional<vivaldi::InstallType> existing_install_type =
        vivaldi::FindInstallType(update_install_dir);
#if defined(COMPONENT_BUILD)
    if (winsparkle::g_install_dir.empty()) {
      // A development build defaults to use an update mode if the install
      // directory is not given explicitly.
      DCHECK(!existing_install_type)
          << "Build directory should not have Application subfolder.";
      winsparkle::g_build_dir = current_exe_path.DirName();
      winsparkle::g_install_type = vivaldi::InstallType::kForCurrentUser;
    } else
#endif
        if (existing_install_type) {
      winsparkle::g_install_type = *existing_install_type;
      winsparkle::g_install_dir = std::move(update_install_dir);
    } else {
      // Running update_notifier.exe not from the installation exe directory
      // switches to the installation mode. If g_install_type is empty, we keep
      // it empty to fallback to the default installation directory.
      winsparkle::g_install_mode = true;
      unique_switches++;
    }
  }

  if (winsparkle::g_install_mode && !winsparkle::g_install_dir.empty()) {
    // If install mode points to an existing installation, default to the type
    // of that installation.
    base::Optional<vivaldi::InstallType> existing_install_type =
        vivaldi::FindInstallType(winsparkle::g_install_dir);
    if (existing_install_type) {
      winsparkle::g_install_type = *existing_install_type;
    }
  }

  if (!winsparkle::g_install_mode) {
    // Make sure to use a working directory not pointing to the
    // version-specific sub-directory of the installation. This way if the
    // installer cannot kill the notifier for some reason during the update,
    // at least it will still be possible to delete the old version-specific
    // directory.
    base::SetCurrentDirectory(winsparkle::GetExeDir());
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
    ExitCode exit_code = RunSubAction(*subaction, fallthrough_to_main_action);
    if (exit_code != ExitCode::kOk || !fallthrough_to_main_action)
      return exit_code;
  }

  if (!winsparkle::g_install_mode) {
    DCHECK(!winsparkle::GetExeDir().empty());
    winsparkle::g_app_version =
        vivaldi::GetInstallVersion(winsparkle::GetExeDir());
    DLOG(INFO) << "Installed Vivaldi version: " << winsparkle::g_app_version;
#if !defined(OFFICIAL_BUILD)
    if (command_line.HasSwitch(kDebugVersion)) {
      winsparkle::g_app_version =
          base::Version(command_line.GetSwitchValueASCII(kDebugVersion));
    }
#endif
  }
  base::AtExitManager at_exit;
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::UI);

  install_static::InitializeProductDetailsForPrimaryModule();

  return UpdateNotifierManager::GetInstance().RunNotifier();
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
