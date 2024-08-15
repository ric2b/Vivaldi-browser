// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "update_notifier/update_notifier_manager.h"

#include <Windows.h>

#include <AclAPI.h>
#include <Psapi.h>
#include <shellscalingapi.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/message_window.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/util/util_constants.h"
#include "components/language/core/browser/pref_names.h"
#include "components/version_info/version_info_values.h"
#include "ui/base/ui_base_paths.h"

#include "app/vivaldi_constants.h"
#include "base/vivaldi_switches.h"
#include "browser/init_sparkle.h"
#include "installer/win/detached_thread.h"
#include "installer/win/vivaldi_install_l10n.h"
#include "update_notifier/thirdparty/winsparkle/src/config.h"
#include "update_notifier/thirdparty/winsparkle/src/download.h"
#include "update_notifier/thirdparty/winsparkle/src/ui.h"
#include "update_notifier/thirdparty/winsparkle/src/updatedownloader.h"
#include "update_notifier/update_notifier_window.h"
#include "vivaldi/update_notifier/update_notifier_strings.h"

namespace vivaldi_update_notifier {

namespace {

constexpr base::win::i18n::LanguageSelector::LangToOffset
    kLanguageOffsetPairs[] = {
#define HANDLE_LANGUAGE(l_, o_) {L## #l_, o_},
        DO_LANGUAGES
#undef HANDLE_LANGUAGE
};

bool SafeGetTokenInformation(HANDLE token,
                             TOKEN_INFORMATION_CLASS token_information_class,
                             std::vector<uint8_t>* information) {
  DWORD size;
  information->clear();

  if (GetTokenInformation(token, token_information_class, nullptr, 0, &size) ==
          FALSE &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    return false;

  information->resize(size);
  return GetTokenInformation(token, token_information_class,
                             information->data(), size, &size) != FALSE;
}

struct ACLDeleter {
  void operator()(ACL* acl) { LocalFree(acl); }
};

bool MakeEventSecurityDescriptor(std::vector<uint8_t>* owner_buffer,
                                 std::vector<uint8_t>* primary_group_buffer,
                                 std::unique_ptr<ACL, ACLDeleter>* dacl,
                                 SECURITY_DESCRIPTOR* security_descriptor) {
  base::win::ScopedHandle process_token([] {
    HANDLE process_token_handle;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token_handle);
    return process_token_handle;
  }());

  if (!process_token.IsValid())
    return false;

  if (!SafeGetTokenInformation(process_token.Get(), TokenOwner, owner_buffer))
    return false;
  if (!SafeGetTokenInformation(process_token.Get(), TokenPrimaryGroup,
                               primary_group_buffer))
    return false;

  uint8_t system_sid[SECURITY_MAX_SID_SIZE];
  DWORD sid_size = sizeof(system_sid);
  CreateWellKnownSid(WinLocalSystemSid, nullptr, system_sid, &sid_size);
  uint8_t local_sid[SECURITY_MAX_SID_SIZE];
  sid_size = sizeof(local_sid);
  CreateWellKnownSid(WinLocalSid, nullptr, local_sid, &sid_size);
  uint8_t administrators_sid[SECURITY_MAX_SID_SIZE];
  sid_size = sizeof(administrators_sid);
  CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, administrators_sid,
                     &sid_size);

  EXPLICIT_ACCESS explicit_access[3];

  // The SYSTEM user usually has full access to events
  explicit_access[0].grfAccessPermissions = GENERIC_ALL;
  explicit_access[0].grfAccessMode = SET_ACCESS;
  explicit_access[0].grfInheritance = NO_INHERITANCE;
  explicit_access[0].Trustee = {};
  explicit_access[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  explicit_access[0].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
  explicit_access[0].Trustee.ptstrName = reinterpret_cast<wchar_t*>(system_sid);

  // We want any notifiers running as any local user on the machine to
  // be able to listen to this.
  explicit_access[1].grfAccessPermissions = SYNCHRONIZE;
  explicit_access[1].grfAccessMode = SET_ACCESS;
  explicit_access[1].grfInheritance = NO_INHERITANCE;
  explicit_access[1].Trustee = {};
  explicit_access[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  explicit_access[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
  explicit_access[1].Trustee.ptstrName = reinterpret_cast<wchar_t*>(local_sid);

  // Vivaldi installers running as an administrator should be able to restart
  // all updaters.
  explicit_access[2].grfAccessPermissions = EVENT_MODIFY_STATE;
  explicit_access[2].grfAccessMode = SET_ACCESS;
  explicit_access[2].grfInheritance = NO_INHERITANCE;
  explicit_access[2].Trustee = {};
  explicit_access[2].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  explicit_access[2].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
  explicit_access[2].Trustee.ptstrName =
      reinterpret_cast<wchar_t*>(administrators_sid);

  ACL* dacl_out;
  SetEntriesInAcl(3, explicit_access, nullptr, &dacl_out);
  dacl->reset(dacl_out);

  if (InitializeSecurityDescriptor(security_descriptor,
                                   SECURITY_DESCRIPTOR_REVISION) == FALSE)
    return false;

  if (SetSecurityDescriptorOwner(
          security_descriptor,
          reinterpret_cast<TOKEN_OWNER*>(owner_buffer->data())->Owner,
          TRUE) == FALSE)
    return false;
  if (SetSecurityDescriptorGroup(
          security_descriptor,
          reinterpret_cast<TOKEN_PRIMARY_GROUP*>(primary_group_buffer->data())
              ->PrimaryGroup,
          TRUE) == FALSE)
    return false;
  if (SetSecurityDescriptorDacl(security_descriptor, TRUE, dacl_out, FALSE) ==
      FALSE)
    return false;

  return true;
}

base::win::ScopedHandle MakeGlobalEvent(const std::wstring& event_name) {
  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.bInheritHandle = FALSE;
  security_attributes.nLength = sizeof(security_attributes);

  // These buffers and the ACL MUST remain in memory until we are done with
  // the
  // security descriptor, because the security descriptor refers to their
  // content.
  std::vector<uint8_t> owner_buffer;
  std::vector<uint8_t> primary_group_buffer;
  std::unique_ptr<ACL, ACLDeleter> dacl;
  SECURITY_DESCRIPTOR security_descriptor;

  if (MakeEventSecurityDescriptor(&owner_buffer, &primary_group_buffer, &dacl,
                                  &security_descriptor)) {
    security_attributes.lpSecurityDescriptor = &security_descriptor;
  } else {
    // Fallback to using the default descriptor if we failed constructing one.
    security_attributes.lpSecurityDescriptor = nullptr;
  }

  base::win::ScopedHandle event_handle;
  for (int i = 0; i < 3; i++) {
    event_handle.Set(
        CreateEvent(&security_attributes, TRUE, FALSE, event_name.c_str()));
    if (event_handle.IsValid())
      break;
    event_handle.Set(OpenEvent(SYNCHRONIZE, FALSE, event_name.c_str()));
    if (event_handle.IsValid())
      break;
  }

  LOG_IF(ERROR, !event_handle.IsValid())
      << "Failed to listen for " << event_name;
  return event_handle;
}

// NOTE(jarle@vivaldi.com): High DPI enabling functions source code is borrowed
// from chrome_exe_main_win.cc.

bool SetProcessDpiAwarenessWrapper(PROCESS_DPI_AWARENESS value) {
  typedef HRESULT(WINAPI * SetProcessDpiAwarenessPtr)(PROCESS_DPI_AWARENESS);
  SetProcessDpiAwarenessPtr set_process_dpi_awareness_func =
      reinterpret_cast<SetProcessDpiAwarenessPtr>(GetProcAddress(
          GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessInternal"));
  if (set_process_dpi_awareness_func) {
    HRESULT hr = set_process_dpi_awareness_func(value);
    if (SUCCEEDED(hr)) {
      VLOG(1) << "SetProcessDpiAwareness succeeded.";
      return true;
    } else if (hr == E_ACCESSDENIED) {
      LOG(ERROR) << "Access denied error from SetProcessDpiAwareness. "
                    "Function called twice, or manifest was used.";
    }
  }
  return false;
}

void EnableHighDPISupport() {
  // Enable per-monitor DPI for Win10 or above instead of Win8.1 since Win8.1
  // does not have EnableChildWindowDpiMessage, necessary for correct non-client
  // area scaling across monitors.
  bool allowed_platform = base::win::GetVersion() >= base::win::Version::WIN10;
  PROCESS_DPI_AWARENESS process_dpi_awareness =
      allowed_platform ? PROCESS_PER_MONITOR_DPI_AWARE
                       : PROCESS_SYSTEM_DPI_AWARE;
  if (!SetProcessDpiAwarenessWrapper(process_dpi_awareness)) {
    SetProcessDPIAware();
  }
}

std::wstring ReadLocaleStateLanguage() {
  base::FilePath local_state_path;
  CHECK(base::PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path));
  std::string json_text;
  if (!base::ReadFileToString(local_state_path, &json_text)) {
    LOG(WARNING) << "Failed to read " << local_state_path;
    return std::wstring();
  }
  std::optional<base::Value> json = base::JSONReader::Read(json_text);
  if (!json) {
    LOG(WARNING) << "Failed to parse " << local_state_path << " as json";
    return std::wstring();
  }
  base::Value* value = json->GetDict().Find(language::prefs::kApplicationLocale);
  if (!value || !value->is_string())
    return std::wstring();
  std::string language = value->GetString();
  return base::UTF8ToWide(language);
}

}  // namespace

class UpdateNotifierManager::UpdateCheckThread
    : public vivaldi::DetachedThread {
 public:
  UpdateCheckThread() = default;

  void Run() override {
    Error error;
    std::unique_ptr<Appcast> appcast = CheckForUpdates(error);
    if (error) {
      LOG(ERROR) << error.log_message();
    }
    GetInstance().main_thread_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UpdateNotifierManager::OnUpdateCheckResult,
                                  base::Unretained(&GetInstance()),
                                  std::move(appcast), std::move(error)));
  }

  std::unique_ptr<Appcast> CheckForUpdates(Error& error) {
    if (error)
      return nullptr;

    GURL url = init_sparkle::GetAppcastUrl();
    LOG(INFO) << "Downloading an appcast from " << url.spec();
    downloader_.Connect(url, error);
    std::string appcast_xml = downloader_.FetchAll(error);
    if (error)
      return nullptr;
    if (appcast_xml.size() == 0) {
      error.set(Error::kFormat, "Appcast XML data incomplete.");
      return nullptr;
    }

    std::unique_ptr<Appcast> appcast = Appcast::Load(appcast_xml, error);
    if (!appcast)
      return nullptr;
    DCHECK(appcast->IsValid());
    if (!appcast->IsValid())
      return nullptr;

    return appcast;
  }

  FileDownloader downloader_;
};

class UpdateNotifierManager::UpdateDownloadThread
    : public vivaldi::DetachedThread,
      public DownloadUpdateDelegate {
 public:
  UpdateDownloadThread(JobId job_id, const Appcast& appcast)
      : job_id_(job_id), appcast_(appcast) {}

 protected:
  // DetachedThread implementation
  void Run() override {
    Error error;
    std::unique_ptr<InstallerLaunchData> launch_data =
        DownloadUpdate(appcast_, *this, error);
    if (error) {
      LOG(ERROR) << error.log_message();
    }
    GetInstance().main_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UpdateNotifierManager::OnUpdateDownloadResult,
                       base::Unretained(&GetInstance()), job_id_,
                       std::move(launch_data), std::move(error)));
  }

  // DownloadUpdateDelegate implementation
  void SendReport(const DownloadReport& report, Error& error) override {
    if (error)
      return;
    if (job_id_ != GetInstance().download_job_id_.load()) {
      error.set(Error::kCancelled);
      return;
    }
    if (report.kind == DownloadReport::kMoreData) {
      // only update at most 10 times/sec so that we don't flood the UI:
      clock_t now = clock();
      if (report.downloaded_length != report.content_length &&
          double(now - last_more_data_time_) / CLOCKS_PER_SEC < 0.1) {
        return;
      }
      last_more_data_time_ = now;
    } else {
      // Force sending the next kMoreData report.
      last_more_data_time_ = 0;
    }
    GetInstance().main_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UpdateNotifierManager::OnUpdateDownloadReport,
                       base::Unretained(&GetInstance()), job_id_,
                       std::move(report)));
  }

 private:
  JobId job_id_ = 0;
  Appcast appcast_;
  clock_t last_more_data_time_ = 0;
};

UpdateNotifierManager::UpdateNotifierManager() {}

UpdateNotifierManager::~UpdateNotifierManager() {
  // We should not be destructed ever.
  NOTREACHED();
}

// static
UpdateNotifierManager& UpdateNotifierManager::GetInstance() {
  static base::NoDestructor<UpdateNotifierManager> instance;
  return *instance;
}

void UpdateNotifierManager::InitEvents(bool& already_runs) {
  DCHECK(!already_runs);
  main_thread_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

  // Create the check for updates event first as we use it both to ensure
  // uniqueness and to ask the initial process to do the check from another
  // instance upgrading if necessary an automatic gui-less check to manual form
  // with GUI.
  //
  // We use a local event even for system installs as it should be OK for
  // different users to run the notifiers at the same time, for example, to
  // manually check for a new version. In the worst case different users will
  // download the vivaldi update twice, but then the installer ensures that only
  // its single instance can run globally.
  std::wstring check_for_updates_event_name;
  if (g_mode == UpdateMode::kNetworkInstall) {
    // Only obe instance of the network installer per user.
    check_for_updates_event_name = kNetworkInstallerUniquenessEventName;
  } else {
    check_for_updates_event_name = vivaldi::GetUpdateNotifierEventName(
        kCheckForUpdatesEventPrefix, GetExeDir());
  }
  base::win::ScopedHandle check_for_updates_event_handle(
      CreateEvent(nullptr, TRUE, FALSE, check_for_updates_event_name.c_str()));
  DWORD create_event_error = ::GetLastError();
  PCHECK(check_for_updates_event_handle.IsValid());
  check_for_updates_event_.emplace(std::move(check_for_updates_event_handle));
  if (create_event_error == ERROR_ALREADY_EXISTS) {
    // The proces instance that checks for updates already runs.
    already_runs = true;
    return;
  }
  check_for_updates_event_watch_.StartWatching(
      &check_for_updates_event_.value(),
      base::BindOnce(&UpdateNotifierManager::OnCheckForUpdatesEvent,
                     base::Unretained(this)),
      main_thread_runner_);
  DLOG(INFO) << "Listening " << check_for_updates_event_name;

  if (g_mode != UpdateMode::kNetworkInstall) {
    // Update listen for quit events from the installer.
    std::wstring quit_event_name =
        vivaldi::GetUpdateNotifierEventName(kQuitEventPrefix, GetExeDir());
    base::win::ScopedHandle quit_event_handle(
        CreateEvent(nullptr, TRUE, FALSE, quit_event_name.c_str()));
    PCHECK(quit_event_handle.IsValid());
    quit_event_.emplace(std::move(quit_event_handle));
    quit_event_watch_.StartWatching(
        &quit_event_.value(),
        base::BindOnce(&UpdateNotifierManager::OnQuitEvent,
                       base::Unretained(this)),
        main_thread_runner_);
    DLOG(INFO) << "Listening " << quit_event_name;

    if (g_install_type == vivaldi::InstallType::kForAllUsers) {
      // For system-wide installations listen to the global event to exit the
      // notifier for any user during update or uninstall.
      std::wstring global_quit_event_name = vivaldi::GetUpdateNotifierEventName(
          kGlobalQuitEventPrefix, GetExeDir());
      base::win::ScopedHandle global_quit_handle =
          MakeGlobalEvent(global_quit_event_name);
      if (global_quit_handle.IsValid()) {
        global_quit_event_.emplace(std::move(global_quit_handle));
        global_quit_event_watch_.StartWatching(
            &global_quit_event_.value(),
            base::BindOnce(&UpdateNotifierManager::OnQuitEvent,
                           base::Unretained(this)),
            main_thread_runner_);
        DLOG(INFO) << "Listening " << global_quit_event_name;
      }
    }
  }
}

ExitCode UpdateNotifierManager::RunNotifier() {
  bool already_runs = false;
  InitEvents(already_runs);
  if (already_runs) {
    LOG(INFO) << "Notifier already runs, will quit";
    if (g_mode == UpdateMode::kManualCheck) {
      // NOTE(jarle@vivaldi.com): These events will be sent to another running
      // instance of the update notifier, then our process will exit.
      if (!::SetEvent(check_for_updates_event_->handle())) {
        PLOG(ERROR) << "Failed SetEvent()";
        return ExitCode::kError;
      }
    }
    return ExitCode::kAlreadyRuns;
  }

  EnableHighDPISupport();
  chrome::RegisterPathProvider();

  vivaldi::InitInstallerLanguage(kLanguageOffsetPairs,
                                 g_mode == UpdateMode::kNetworkInstall
                                     ? nullptr
                                     : &ReadLocaleStateLanguage);
  UI::Init(*this);

  // When we run the first time after been enabled from the installer, this
  // may fail as the installer may still be running preventing to remove its
  // setup.exe file. But then we will remove the leftovers the next time we
  // run in 24 hours. This is not an issue on subsequent updates when the
  // notifier was already enabled as then the notifier will check for updates
  // either 24 hours or on the browser startup. In both cases the installer
  // will exit at that point.
  //
  // TODO(igor@vivaldi.com): Consider waiting for the installer process to
  // finish and delete the leftovers then.
  CleanDownloadLeftovers();

  StartUpdateCheck();

  run_loop_.Run();

  update_notifier_window_.reset();

  // Delete downloaded data if any. We must do it manually as we do not
  // run destructors on exit.
  launch_data_.reset();

  return ExitCode::kOk;
}

void UpdateNotifierManager::WinsparkleStartDownload() {
  main_thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UpdateNotifierManager::StartDownload,
                                base::Unretained(this)));
}

void UpdateNotifierManager::WinsparkleLaunchInstaller() {
  main_thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UpdateNotifierManager::LaunchInstaller,
                                base::Unretained(this)));
}

void UpdateNotifierManager::WinsparkleOnUIClose() {
  main_thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UpdateNotifierManager::FinishCheck,
                                base::Unretained(this)));
}

void UpdateNotifierManager::StartUpdateCheck() {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());

  check_start_time_ = base::Time::Now();
  LOG(INFO) << "Starting a new update check, mode=" << static_cast<int>(g_mode);
  if (active_winsparkle_ui_) {
    DCHECK_EQ(g_mode, UpdateMode::kManualCheck);
    UI::BringToFocus();
    return;
  }
  if (g_mode == UpdateMode::kManualCheck) {
    active_winsparkle_ui_ = true;
    if (active_download_ || launch_data_) {
      // We are upgrading to a manual form an automated check that is
      // downloading or showing an installation notification. Show the
      // WinSparkle UI while continueing to download or hold launch data. If the
      // user agree to install, we will re-use the downloaded data instead of
      // fetching them again.
      DCHECK(appcast_);
      if (appcast_) {
        UI::NotifyUpdateCheckDone(appcast_.get(), Error(), false);
        return;
      }
    }
    UI::NotifyCheckingUpdates();
  }

  if (active_version_check_)
    return;

  active_version_check_ = true;

  auto update_check = std::make_unique<UpdateCheckThread>();
  if (WithVersionCheckUI(g_mode)) {
    // Manual check should always connect to the server and bypass any
    // caching. This is good for finding updates that are too new to propagate
    // through caches yet.
    update_check->downloader_.DisableCaching();
  }
  vivaldi::DetachedThread::Start(std::move(update_check));
}

void UpdateNotifierManager::OnUpdateCheckResult(
    std::unique_ptr<Appcast> appcast,
    Error error) {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());

  // If the user has previously chosen "Skip version", the following automated
  // update check should skip it. But a new manual check should still show
  // this version to allow the user to reconsider. This is the semantics in
  // Sparkle for Mac.
  if (appcast && !WithVersionCheckUI(g_mode)) {
    base::Version toSkip(ReadRegistryIem(RegistryItem::kSkipThisVersion));
    if (toSkip.IsValid() && toSkip == appcast->Version) {
      LOG(INFO) << "No update to the version " << appcast->Version
                << ": explicit skipped by the user";
      appcast.reset();
    }
  }

  if (appcast && g_app_version.IsValid()) {
    // Check if our version is out of date.
    if (appcast->Version <= g_app_version) {
      LOG(INFO) << "No update: update version " << appcast->Version
                << " <= installed version " << g_app_version;
      appcast.reset();
    }
  }

  if (!active_version_check_) {
    // This is possible if the user closed UI while the check was in progress.
    return;
  }
  active_version_check_ = false;

  if (appcast && g_app_version.IsValid()) {
    LOG(INFO) << "Newer version is available: " << appcast->Version << " > "
              << g_app_version;
  }

  // If the previous automated update check has found an update so appcast_ is
  // not null, the user ignored it and the new check generated an error, we want
  // to notify the user again using the old appcast_. So replace appcast_ only
  // if we got a new update info.
  if (appcast) {
    appcast_ = std::move(appcast);
  } else if (appcast_ && error) {
    error = Error();
  }
  if (active_winsparkle_ui_) {
    bool pending_update = false;
    if (!appcast_ && !error) {
      // The user has the latest version installed. If it is in a pending state
      // waiting for the browser to restart, inform the user about it.
      if (vivaldi::GetPendingUpdateVersion(GetExeDir())) {
        pending_update = true;
      }
    }
    UI::NotifyUpdateCheckDone(appcast_.get(), error, pending_update);
    if (appcast_ && g_mode == UpdateMode::kNetworkInstall) {
      // For the network installer start the download immediately.
      StartDownload();
    }
  } else if (!appcast_) {
    FinishCheck();
  } else if (WithDownloadUI(g_mode)) {
    ShowUpdateNotification(appcast_->Version);
  } else if (launch_data_ && launch_data_->version == appcast_->Version) {
    // We can be here if we downloaded data, presented the install
    // notification to the user but it was ignored and we run the next
    // periodic check. Re-use already downloaded data and ask the user to
    // confirm the installation again.
    if (g_mode == UpdateMode::kSilentUpdate) {
      GetInstance().LaunchInstaller();
    } else {
      ShowUpdateNotification(launch_data_->version);
    }
  } else {
    StartDownload();
  }
}

/* static */
void UpdateNotifierManager::OnNotificationAcceptance() {
  if (GetInstance().active_winsparkle_ui_) {
    // This can happen when the automated check detected an update and
    // notified the user. The user ignored that and rather triggered a manual
    // check. Then, when the manual UI runs, the user went back to the
    // original notification and activated it. Just bring the UI to focus
    // then.
    UI::BringToFocus();
    return;
  }

  if (g_mode != UpdateMode::kSilentUpdate) {
    if (!GetInstance().appcast_)
      return;

    // Activate the UI and jump into the show update info section.
    GetInstance().active_winsparkle_ui_ = true;
    UI::NotifyUpdateCheckDone(GetInstance().appcast_.get(), Error(), false);
    return;
  }
  GetInstance().LaunchInstaller();
}

void UpdateNotifierManager::StartDownload() {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());

  if (!appcast_)
    return;
  if (active_download_)
    return;
  if (active_winsparkle_ui_ && launch_data_) {
    // Resend the notification about a successful download to UI.
    UI::NotifyDownloadResult(Error());
    return;
  }
  active_download_ = true;
  JobId job_id = download_job_id_.load();
  if (launch_data_ && launch_data_->version == appcast_->Version) {
    // The user closed the update UI when the UI was about to start the
    // installer. On the next check re-use the download.
    OnUpdateDownloadResult(job_id, std::move(launch_data_), Error());
    return;
  }
  vivaldi::DetachedThread::Start(
      std::make_unique<UpdateDownloadThread>(job_id, *appcast_));
}

void UpdateNotifierManager::OnUpdateDownloadReport(JobId job_id,
                                                   DownloadReport report) {
  if (job_id != download_job_id_.load()) {
    // The download was cancelled.
    return;
  }
  DCHECK(active_download_);
  if (active_winsparkle_ui_) {
    UI::NotifyDownloadProgress(report);
  }
}

void UpdateNotifierManager::OnUpdateDownloadResult(
    JobId job_id,
    std::unique_ptr<InstallerLaunchData> launch_data,
    Error error) {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());
  if (job_id != download_job_id_.load()) {
    // The download was cancelled.
    return;
  }
  DCHECK(active_download_);
  active_download_ = false;

  // If the user canceled the installation after the download and the next
  // download gave an error, show the results of the previous successfull
  // download.
  if (launch_data) {
    launch_data_ = std::move(launch_data);
  } else if (launch_data_ && error) {
    error = Error();
  }
  if (active_winsparkle_ui_) {
    if (g_mode != UpdateMode::kNetworkInstall || error) {
      UI::NotifyDownloadResult(error);
    } else {
      // The network installer launches the installer immediately.
      LaunchInstaller();
    }
  } else if (launch_data_ && !WithDownloadUI(g_mode)) {
    if (g_mode == UpdateMode::kSilentUpdate) {
      LaunchInstaller();
    } else {
      ShowUpdateNotification(launch_data_->version);
    }
  } else {
    FinishCheck();
  }
}

void UpdateNotifierManager::LaunchInstaller() {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());
  if (!launch_data_)
    return;

  Error error;
  base::Process process = RunInstaller(std::move(launch_data_), error);
  if (active_winsparkle_ui_) {
    // Close Winsparkle UI.
    UI::NotifyStartedInstaller(error);
  } else if (error && g_mode != UpdateMode::kSilentUpdate) {
    // Notify the user about the launch error.
    active_winsparkle_ui_ = true;
    UI::NotifyStartedInstaller(error);
  } else {
    FinishCheck();
  }

  // For the update case we cleanup the download when the installer starts the
  // update notifier from the same exe path again. But for the network installer
  // there will be no new invocation from the same exe path. So wait for the
  // process to finish and remove the main installer then.
  if (g_mode == UpdateMode::kNetworkInstall && !error) {
    int exit_code = 0;
    bool success = process.WaitForExit(&exit_code);
    if (!success) {
      LOG(ERROR) << "Failed to wait for the installer to finish";
    } else if (exit_code != 0) {
      LOG(ERROR) << "Installer exit with non-zero exit_code " << exit_code;
    }
    CleanDownloadLeftovers();
  }
}

void UpdateNotifierManager::FinishCheck() {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());

  active_winsparkle_ui_ = false;
  if (active_version_check_) {
    active_version_check_ = false;
  }
  if (active_download_) {
    // Cancel a background download if any.
    ++download_job_id_;
    active_download_ = false;
  }
  base::TimeDelta check_duration = base::Time::Now() - check_start_time_;
  LOG(INFO) << "Update check finished in " << check_duration;

  run_loop_.Quit();
}

void UpdateNotifierManager::OnQuitEvent(base::WaitableEvent* waitable_event) {
  DCHECK(quit_event_);
  DCHECK(waitable_event == &quit_event_.value() ||
         (global_quit_event_ && waitable_event == &global_quit_event_.value()));

  // Do not reset the event. We want to keep this event on until all event
  // instances are destroyed either implicitly due to the process exit or
  // explicitly when the process that triggered it closes the event handle.
  LOG(INFO) << "Exit due to a quit event";
  run_loop_.Quit();
}

void UpdateNotifierManager::OnCheckForUpdatesEvent(
    base::WaitableEvent* waitable_event) {
  DCHECK_EQ(waitable_event, &check_for_updates_event_.value());
  check_for_updates_event_->Reset();
  check_for_updates_event_watch_.StartWatching(
      &check_for_updates_event_.value(),
      base::BindOnce(&UpdateNotifierManager::OnCheckForUpdatesEvent,
                     base::Unretained(this)),
      main_thread_runner_);

  // Make sure if we run an automatic update check it will be switched to the
  // manual mode.
  g_mode = UpdateMode::kManualCheck;
  StartUpdateCheck();
}

void UpdateNotifierManager::ShowUpdateNotification(
    const base::Version& version) {
  if (!update_notifier_window_) {
    update_notifier_window_ = std::make_unique<UpdateNotifierWindow>();
  }
  update_notifier_window_->ShowNotification(
      base::UTF8ToWide(version.GetString()));
}

}  // namespace vivaldi_update_notifier
