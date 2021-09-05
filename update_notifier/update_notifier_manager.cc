// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "update_notifier/update_notifier_manager.h"

#include <Windows.h>

#include <AclAPI.h>
#include <Psapi.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "app/vivaldi_constants.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "base/vivaldi_switches.h"
#include "base/win/message_window.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "browser/init_sparkle.h"
#include "chrome/common/chrome_paths.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/version_info/version_info_values.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "update_notifier/thirdparty/winsparkle/src/config.h"
#include "update_notifier/thirdparty/winsparkle/src/threads.h"
#include "update_notifier/thirdparty/winsparkle/src/ui.h"
#include "update_notifier/thirdparty/winsparkle/src/updatechecker.h"
#include "update_notifier/thirdparty/winsparkle/src/updatedownloader.h"
#include "update_notifier/update_notifier_switches.h"
#include "update_notifier/update_notifier_window.h"

using base::PathService;

namespace vivaldi_update_notifier {

namespace {

const wchar_t kVivaldiProductName[] = L"" PRODUCT_NAME;
const wchar_t kVivaldiProductVersion[] = L"" VIVALDI_VERSION;

const wchar_t kUpdateRegPath[] = L"Software\\Vivaldi\\AutoUpdate";

const wchar_t kGlobalRestartEventName[] =
    L"Global\\Vivaldi/Update_notifier/Restart/";
const wchar_t kGlobalQuitEventName[] = L"Global\\Vivaldi/Update_notifier/Quit/";

const wchar_t kQuitEventName[] = L"Local\\Vivaldi/Update_notifier/Quit/";
const wchar_t kCheckForUpdatesEventName[] =
    L"Local\\Vivaldi/Update_notifier/Check_for_updates/";

constexpr int kRestartEventActiveSleepInterval = 100;  // ms

constexpr base::TimeDelta kFirstAutomaticCheckDelay =
    base::TimeDelta::FromSeconds(20);

constexpr base::TimeDelta kPeriodicAutomaticCheckInterval =
    base::TimeDelta::FromDays(1);

UpdateNotifierManager* g_manager = nullptr;

base::string16 MakeEventName(const base::string16& event_name_base,
                             const base::FilePath& exe_dir) {
  base::string16 event_name(event_name_base);
  base::string16 normalized_path =
      exe_dir.NormalizePathSeparatorsTo(L'/').value();
  // See
  // https://web.archive.org/web/20130528052217/http://blogs.msdn.com/b/michkap/archive/2005/10/17/481600.aspx
  CharUpper(&normalized_path[0]);
  event_name += normalized_path;

  return event_name;
}

base::FilePath AddVersionToPathIfNeeded(const base::FilePath& path,
                                        const base::FilePath& exe_dir) {
  if (base::PathExists(path))
    return path;

  base::FilePath version_path = exe_dir.Append(kVivaldiProductVersion);
  if (exe_dir.AppendRelativePath(path, &version_path)) {
    return version_path;
  } else {
    return path;
  }
}

class ResourceBundleDelegate : public ui::ResourceBundle::Delegate {
 public:
  ResourceBundleDelegate(const base::FilePath& exe_dir) : exe_dir_(exe_dir) {}
  ~ResourceBundleDelegate() override {}

  base::FilePath GetPathForResourcePack(const base::FilePath& pack_path,
                                        ui::ScaleFactor scale_factor) override {
    return AddVersionToPathIfNeeded(pack_path, exe_dir_);
  }

  base::FilePath GetPathForLocalePack(const base::FilePath& pack_path,
                                      const std::string& locale) override {
    return AddVersionToPathIfNeeded(pack_path, exe_dir_);
  }

  gfx::Image GetImageNamed(int resource_id) override { return gfx::Image(); }
  gfx::Image GetNativeImageNamed(int resource_id) override {
    return gfx::Image();
  }

  base::RefCountedMemory* LoadDataResourceBytes(
      int resource_id,
      ui::ScaleFactor scale_factor) override {
    return nullptr;
  }

  bool GetRawDataResource(int resource_id,
                          ui::ScaleFactor scale_factor,
                          base::StringPiece* value) const override {
    return false;
  }

  bool GetLocalizedString(int message_id,
                          base::string16* value) const override {
    return false;
  }

  base::Optional<std::string> LoadDataResourceString(int resource_id) override {
    return base::Optional<std::string>();
  }

 private:
  base::FilePath exe_dir_;
};

bool SafeGetTokenInformation(HANDLE token,
                             TOKEN_INFORMATION_CLASS token_information_class,
                             std::vector<uint8_t>* information) {
  DWORD size;
  information->clear();

  if (GetTokenInformation(token, token_information_class, NULL, 0, &size) ==
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
  CreateWellKnownSid(WinLocalSystemSid, NULL, system_sid, &sid_size);
  uint8_t local_sid[SECURITY_MAX_SID_SIZE];
  sid_size = sizeof(local_sid);
  CreateWellKnownSid(WinLocalSid, NULL, local_sid, &sid_size);
  uint8_t administrators_sid[SECURITY_MAX_SID_SIZE];
  sid_size = sizeof(administrators_sid);
  CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, administrators_sid,
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
  SetEntriesInAcl(3, explicit_access, NULL, &dacl_out);
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

base::WaitableEvent* MakeEvent(const base::string16& event_name_base,
                               const base::FilePath& exe_dir) {
  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.bInheritHandle = FALSE;
  security_attributes.nLength = sizeof(security_attributes);

  base::FilePath program_files_dir;
  PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir);

  // These buffers and the ACL MUST remain in memory until we are done with
  // the
  // security descriptor, because the security descriptor refers to their
  // content.
  std::vector<uint8_t> owner_buffer;
  std::vector<uint8_t> primary_group_buffer;
  std::unique_ptr<ACL, ACLDeleter> dacl;
  SECURITY_DESCRIPTOR security_descriptor;

  if (program_files_dir.IsParent(exe_dir)) {
    // ----System-wide installation----
    // TODO(julienp): This is completely the wrong way to detect a system-wide
    // installation, but it will work with default paths. The installer is
    // currently doing something similar anyway.
    if (MakeEventSecurityDescriptor(&owner_buffer, &primary_group_buffer, &dacl,
                                    &security_descriptor)) {
      security_attributes.lpSecurityDescriptor = &security_descriptor;
    } else {
      // Fallback to using the default descriptor if we failed constructing one.
      security_attributes.lpSecurityDescriptor = NULL;
    }
  } else {
    // On non-system-wide installations, we only need the running user to
    // be able to restart the notifier, so the default descriptor is fine
    security_attributes.lpSecurityDescriptor = NULL;
  }

  base::string16 event_name = MakeEventName(event_name_base, exe_dir);

  base::win::ScopedHandle event_handle;
  for (int i = 0; i < 3; i++) {
    if (event_handle.IsValid())
      break;
    event_handle.Set(
        CreateEvent(&security_attributes, TRUE, FALSE, event_name.c_str()));
    if (event_handle.IsValid())
      break;
    event_handle.Set(OpenEvent(SYNCHRONIZE, FALSE, event_name.c_str()));
  }

  if (event_handle.IsValid())
    return new base::WaitableEvent(std::move(event_handle));
  return nullptr;
}

bool PathProvider(int key, base::FilePath* result) {
  base::FilePath cur;
  switch (key) {
    case ui::DIR_LOCALES:
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("locales"));
      break;
    default:
      return false;
  }

  *result = cur;
  return true;
}

void InitWinsparkle(const init_sparkle::Config& init_config,
                    const base::CommandLine& command_line,
                    const base::FilePath& exe_dir,
                    winsparkle::UIDelegate& ui_delegate) {
  winsparkle::Config config;
  config.appcast_url = init_config.appcast_url;
  config.registry_path = kUpdateRegPath;
  config.locale = base::i18n::GetConfiguredLocale();
  config.app_name = kVivaldiProductName;
  config.app_version = kVivaldiProductVersion;
  config.exe_dir = exe_dir;
#if !defined(OFFICIAL_BUILD)
  base::string16 debug_version =
      command_line.GetSwitchValueNative(kUpdateVersion);
  if (!debug_version.empty()) {
    config.app_version = debug_version;
  }
#endif
  winsparkle::InitConfig(std::move(config));
  winsparkle::CleanDownloadLeftovers();
  winsparkle::UI::Init(ui_delegate);
}

}  // namespace

class UpdateNotifierManager::UpdateCheckThread
    : public winsparkle::DetachedThread {
 public:
  UpdateCheckThread(bool manual) : manual_(manual) {}

  void Run() override {
    winsparkle::Error error;
    std::unique_ptr<winsparkle::Appcast> appcast =
        winsparkle::CheckForUpdates(manual_, error);
    if (error) {
      winsparkle::LogError(error);
    }
    g_manager->main_thread_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UpdateNotifierManager::OnUpdateCheckResult,
                                  base::Unretained(g_manager),
                                  std::move(appcast), std::move(error)));
  }

  bool manual_ = false;
};

class UpdateNotifierManager::UpdateDownloadThread
    : public winsparkle::DetachedThread,
      public winsparkle::DownloadUpdateDelegate {
 public:
  UpdateDownloadThread(JobId job_id, const winsparkle::Appcast& appcast)
      : job_id_(job_id), appcast_(appcast) {}

 protected:
  // DetachedThread implementation
  void Run() override {
    winsparkle::Error error;
    std::unique_ptr<winsparkle::InstallerLaunchData> launch_data =
        winsparkle::DownloadUpdate(appcast_, *this, error);
    if (error) {
      winsparkle::LogError(error);
    }
    g_manager->main_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UpdateNotifierManager::OnUpdateDownloadResult,
                       base::Unretained(g_manager), job_id_,
                       std::move(launch_data), std::move(error)));
  }

  // DownloadUpdateDelegate implementation
  void SendReport(const winsparkle::DownloadReport& report,
                  winsparkle::Error& error) override {
    if (error)
      return;
    if (job_id_ != g_manager->download_job_id_.load()) {
      error.set(winsparkle::Error::kCancelled);
      return;
    }
    if (report.kind == winsparkle::DownloadReport::kMoreData) {
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
    g_manager->main_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UpdateNotifierManager::OnUpdateDownloadReport,
                       base::Unretained(g_manager), job_id_,
                       std::move(report)));
  }

 private:
  JobId job_id_ = 0;
  winsparkle::Appcast appcast_;
  clock_t last_more_data_time_ = 0;
};

UpdateNotifierManager::UpdateNotifierManager() {
  DCHECK(!g_manager);
  g_manager = this;

  base::PathService::Get(base::FILE_EXE, &exe_path_);
#if !defined(OFFICIAL_BUILD)
  base::string16 debug_exe_dir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          kUpdateExeDir);
  if (!debug_exe_dir.empty()) {
    exe_path_ = base::FilePath(debug_exe_dir).Append(exe_path_.BaseName());
  }
#endif
  base::FilePath exe_dir = exe_path_.DirName();

  global_restart_event_.reset(MakeEvent(kGlobalRestartEventName, exe_dir));
  if (global_restart_event_.get()) {
    // If the restart event is active at this point, it is probably because it
    // was set, then we restarted and it hasn't been unset yet. Let's just wait
    // it out.
    while (global_restart_event_->IsSignaled())
      Sleep(kRestartEventActiveSleepInterval);

    global_restart_event_watch_.StartWatching(
        global_restart_event_.get(),
        base::Bind(&UpdateNotifierManager::OnEventTriggered,
                   base::Unretained(this)),
        base::ThreadTaskRunnerHandle::Get());
  }

  global_quit_event_.reset(MakeEvent(kGlobalQuitEventName, exe_dir));
  if (global_quit_event_.get()) {
    global_quit_event_watch_.StartWatching(
        global_quit_event_.get(),
        base::Bind(&UpdateNotifierManager::OnEventTriggered,
                   base::Unretained(this)),
        base::ThreadTaskRunnerHandle::Get());
  }

  base::win::ScopedHandle quit_event_handle;
  quit_event_handle.Set(CreateEvent(
      NULL, TRUE, FALSE, MakeEventName(kQuitEventName, exe_dir).c_str()));
  if (quit_event_handle.IsValid()) {
    quit_event_.reset(new base::WaitableEvent(std::move(quit_event_handle)));

    quit_event_watch_.StartWatching(
        quit_event_.get(),
        base::Bind(&UpdateNotifierManager::OnEventTriggered,
                   base::Unretained(this)),
        base::ThreadTaskRunnerHandle::Get());
  }

  base::win::ScopedHandle check_for_updates_event_handle;
  check_for_updates_event_handle.Set(
      CreateEvent(NULL, TRUE, FALSE,
                  MakeEventName(kCheckForUpdatesEventName, exe_dir).c_str()));
  if (check_for_updates_event_handle.IsValid()) {
    check_for_updates_event_.reset(
        new base::WaitableEvent(std::move(check_for_updates_event_handle)));

    check_for_updates_event_watch_.StartWatching(
        check_for_updates_event_.get(),
        base::Bind(&UpdateNotifierManager::OnEventTriggered,
                   base::Unretained(this)),
        base::ThreadTaskRunnerHandle::Get());
  }
}

UpdateNotifierManager::~UpdateNotifierManager() {
  // We should not be destructed ever.
  NOTREACHED();
  if (already_exists_ != NULL)
    ::CloseHandle(already_exists_);
}

bool UpdateNotifierManager::RunNotifier() {
  DCHECK(g_manager == this);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  bool check_for_updates_with_ui = command_line.HasSwitch(kCheckForUpdates);
  bool should_quit = command_line.HasSwitch(kQuit);

  if (IsNotifierAlreadyRunning()) {
    // NOTE(jarle@vivaldi.com): These events will be sent to another running
    // instance of the update notifier, then our process will exit.
    if (should_quit) {
      if (quit_event_.get()) {
        quit_event_->Signal();
      }
    } else if (check_for_updates_with_ui) {
      if (check_for_updates_event_.get()) {
        check_for_updates_event_->Signal();
      }
    }

    return true;
  }

  if (should_quit)
    return true;

  main_thread_runner_ = base::ThreadTaskRunnerHandle::Get();

  PathService::RegisterProvider(PathProvider, ui::PATH_START, ui::PATH_END);
  chrome::RegisterPathProvider();

  l10n_util::OverrideLocaleWithUILanguageList();

  base::FilePath local_state_path;
  CHECK(PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path));
  scoped_refptr<JsonPrefStore> local_state =
      new JsonPrefStore(local_state_path, nullptr, main_thread_runner_);
  local_state->ReadPrefs();

  const base::Value* locale_value = nullptr;
  std::string locale;
  if (local_state->GetValue(language::prefs::kApplicationLocale, &locale_value))
    locale_value->GetAsString(&locale);

  base::FilePath exe_dir = exe_path_.DirName();

  ResourceBundleDelegate resource_bundle_delegate(exe_dir);

  const std::string loaded_locale =
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          locale, &resource_bundle_delegate,
          ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  if (loaded_locale.empty())
    return false;

  init_sparkle::Config sparkle_init_config =
      init_sparkle::GetConfig(command_line);
  if (sparkle_init_config.show_appcast) {
    ::MessageBoxA(NULL, sparkle_init_config.appcast_url.spec().c_str(),
                  "Vivaldi Update URL:", MB_ICONINFORMATION | MB_SETFOREGROUND);
  }
  InitWinsparkle(sparkle_init_config, command_line, exe_dir, *this);

  update_notifier_window_.reset(new UpdateNotifierWindow());

  if (!update_notifier_window_->Init())
    return false;

  bool force_startup_automated_check = false;
  if (check_for_updates_with_ui || sparkle_init_config.show_appcast) {
    // If the notifier is started with a manual appcast URL, assume the
    // intention is to started an automated check as soon as possible.
    StartUpdateCheck(check_for_updates_with_ui);
  } else {
    // We are started to periodically check for updates in the background with
    // the default appcast URL. This happens either after the user runs vivaldi
    // for the first time after the installation, or when the user selected
    // the automated check in the options, or Windows's autostart launched the
    // notifier upon the user login. We want in such cases to run the background
    // check shortly even if it was already run very recently.
    force_startup_automated_check = true;
  }

  // Schedule periodic background update checks.
  main_thread_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UpdateNotifierManager::CheckForUpdatesPeriodically,
                     base::Unretained(this), force_startup_automated_check),
      kFirstAutomaticCheckDelay);

  run_loop_.Run();

  update_notifier_window_.reset();

  // Delete downloaded data if any. We must do it manually as we do not
  // run destructors on exit.
  launch_data_ = nullptr;

  return true;
}

bool UpdateNotifierManager::IsNotifierAlreadyRunning() {
  DCHECK(!already_exists_);
  base::string16 event_name = MakeEventName(L"Global\\", exe_path_);

  already_exists_ = ::CreateEvent(NULL, TRUE, TRUE, event_name.c_str());
  int error = ::GetLastError();
  return (error == ERROR_ALREADY_EXISTS || error == ERROR_ACCESS_DENIED);
}

bool UpdateNotifierManager::IsNotifierEnabled() {
  base::string16 notifier_path_string(L"\"" + exe_path_.value() + L"\"");

  base::string16 cmd;
  bool enabled =
      base::win::ReadCommandFromAutoRun(
          HKEY_CURRENT_USER, ::vivaldi::kUpdateNotifierAutorunName, &cmd) &&
      (base::FilePath::CompareEqualIgnoreCase(cmd, notifier_path_string) ||
       base::FilePath::CompareEqualIgnoreCase(cmd, exe_path_.value()));
#if !defined(OFFICIAL_BUILD)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kUpdateKeepRunning)) {
    enabled = true;
  }
#endif
  return enabled;
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

void UpdateNotifierManager::CheckForUpdatesPeriodically(bool force_check) {
  const base::Time last_check_time =
      base::Time::FromTimeT(winsparkle::GetLastUpdateCheckTime());
  const base::Time current_time = base::Time::Now();

  // Only check for updates in reasonable intervals.
  if (force_check ||
      current_time - last_check_time >= kPeriodicAutomaticCheckInterval) {
    StartUpdateCheck(/*with_ui=*/false);
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UpdateNotifierManager::CheckForUpdatesPeriodically,
                     base::Unretained(this), false),
      kPeriodicAutomaticCheckInterval);
}

void UpdateNotifierManager::StartUpdateCheck(bool with_ui) {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());
  if (active_winsparkle_ui_) {
    winsparkle::UI::BringToFocus();
    return;
  }
  if (with_ui) {
    // If active_version_check_ is true, we upgrade the automated check to a
    // manual form.
    active_winsparkle_ui_ = true;
    winsparkle::UI::NotifyCheckingUpdates();
  }

  if (active_version_check_)
    return;

  active_version_check_ = true;
  winsparkle::DetachedThread::Start(
      std::make_unique<UpdateCheckThread>(with_ui));
}

void UpdateNotifierManager::OnUpdateCheckResult(
    std::unique_ptr<winsparkle::Appcast> appcast,
    winsparkle::Error error) {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());
  if (!active_version_check_) {
    // This is possible if the user closed UI while the check was in progress.
    return;
  }
  active_version_check_ = false;

  // If the previous automated update check has found an update so appcast_ is
  // not null, the user ignored it and the new check generated an error, we want
  // to notify the user again using the old appcast_. So replace appcast_ only
  // if we got a new update info.
  if (appcast) {
    appcast_ = std::move(appcast);
  } else if (appcast_ && error) {
    error = winsparkle::Error();
  }
  if (active_winsparkle_ui_) {
    winsparkle::UI::NotifyUpdateCheckDone(appcast_.get(), error);
  } else if (appcast_) {
    update_notifier_window_->ShowNotification(appcast_->Version);
  } else {
    FinishCheck();
  }
}

/* static */
void UpdateNotifierManager::OnNotificationAcceptance() {
  if (g_manager->active_winsparkle_ui_) {
    // This can happen when the automated check detected an update and
    // notified the user. The user ignored that and rather triggered a manual
    // check. Then, when the manual UI runs, the user went back to the
    // original notification and activated it. Just bring the UI to focus
    // then.
    winsparkle::UI::BringToFocus();
    return;
  }
  if (!g_manager->appcast_)
    return;

  // Activate the UI and jump into the show update info section.
  g_manager->active_winsparkle_ui_ = true;
  winsparkle::UI::NotifyUpdateCheckDone(g_manager->appcast_.get(),
                                        winsparkle::Error());
}

void UpdateNotifierManager::StartDownload() {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());
  if (!appcast_)
    return;
  if (active_download_)
    return;
  active_download_ = true;
  JobId job_id = download_job_id_.load();
  if (launch_data_ && launch_data_->version == appcast_->Version) {
    // The user closed the update UI when the UI was about to start the
    // installer. On the next check re-use the download.
    OnUpdateDownloadResult(job_id, std::move(launch_data_),
                           winsparkle::Error());
    return;
  }
  winsparkle::DetachedThread::Start(
      std::make_unique<UpdateDownloadThread>(job_id, *appcast_));
}

void UpdateNotifierManager::OnUpdateDownloadReport(
    JobId job_id,
    winsparkle::DownloadReport report) {
  if (job_id != download_job_id_.load()) {
    // The download was cancelled.
    return;
  }
  DCHECK(active_download_);
  if (active_winsparkle_ui_) {
    winsparkle::UI::NotifyDownloadProgress(report);
  }
}

void UpdateNotifierManager::OnUpdateDownloadResult(
    JobId job_id,
    std::unique_ptr<winsparkle::InstallerLaunchData> launch_data,
    winsparkle::Error error) {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());
  if (job_id != download_job_id_.load()) {
    // The download was cancelled.
    return;
  }
  DCHECK(active_download_);
  active_download_ = false;

  // If the user canceled the installation after the download and the next check
  // gave an error, show the results of the previous successfull download.
  if (launch_data) {
    launch_data_ = std::move(launch_data);
  } else if (launch_data_ && error) {
    error = winsparkle::Error();
  }
  if (active_winsparkle_ui_) {
    winsparkle::UI::NotifyDownloadResult(error);
  } else {
    FinishCheck();
  }
}

void UpdateNotifierManager::LaunchInstaller() {
  DCHECK(main_thread_runner_->RunsTasksInCurrentSequence());
  if (!launch_data_)
    return;

  winsparkle::Error error;
  winsparkle::RunInstaller(std::move(launch_data_), error);
  if (active_winsparkle_ui_) {
    winsparkle::UI::NotifyStartedInstaller(error);
  } else {
    FinishCheck();
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

  if (!IsNotifierEnabled()) {
    // The process was invoked just to show the manual check UI. Quit.
    run_loop_.Quit();
  }
}

void UpdateNotifierManager::OnEventTriggered(
    base::WaitableEvent* waitable_event) {
  if (waitable_event == global_restart_event_.get()) {
    run_loop_.Quit();
    // restart only if the update notifier is enabled
    if (IsNotifierEnabled()) {
      // Use the real exe_path, nor exe_path_ as the latter can be fake when
      // debugging.
      base::FilePath exe_path =
          base::CommandLine::ForCurrentProcess()->GetProgram();

      // we don't want to inherit command line options for the notifier,
      // so generate a new cmd line
      base::CommandLine cmd_line(exe_path);
      base::LaunchProcess(cmd_line, base::LaunchOptions());
    }
  } else if (waitable_event == quit_event_.get() ||
             waitable_event == global_quit_event_.get()) {
    run_loop_.Quit();
  } else if (waitable_event == check_for_updates_event_.get()) {
    check_for_updates_event_->Reset();
    check_for_updates_event_watch_.StartWatching(
        check_for_updates_event_.get(),
        base::Bind(&UpdateNotifierManager::OnEventTriggered,
                   base::Unretained(this)),
        base::ThreadTaskRunnerHandle::Get());
    StartUpdateCheck(true);
  } else {
    NOTREACHED();
  }
}

}  // namespace vivaldi_update_notifier
