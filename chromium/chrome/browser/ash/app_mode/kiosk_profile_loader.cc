// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_profile_loader.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/syslog_logging.h"
#include "base/system/sys_info.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/app_mode/cancellable_job.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/retry_runner.h"
#include "chrome/browser/ash/login/auth/chrome_login_performer.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/login/auth/login_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"

namespace ash {

namespace {

enum class MountedState { kMounted, kNotMounted };

using CryptohomeMountStateCallback =
    base::OnceCallback<void(std::optional<MountedState> result)>;

bool IsTestOrLinuxChromeOS() {
  // This code should only run in Chrome OS, so not `IsRunningOnChromeOS()`
  // means it's either a test or linux-chromeos.
  return !base::SysInfo::IsRunningOnChromeOS();
}

KioskAppLaunchError::Error LoginFailureToKioskLaunchError(
    const AuthFailure& error) {
  switch (error.reason()) {
    case AuthFailure::COULD_NOT_MOUNT_TMPFS:
    case AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME:
      return KioskAppLaunchError::Error::kUnableToMount;
    case AuthFailure::DATA_REMOVAL_FAILED:
      return KioskAppLaunchError::Error::kUnableToRemove;
    case AuthFailure::USERNAME_HASH_FAILED:
      return KioskAppLaunchError::Error::kUnableToRetrieveHash;
    default:
      LOG(ERROR) << "KIOSK launch error because of AuthFailure::FailureReason: "
                 << error.reason();
      return KioskAppLaunchError::Error::kUnableToMount;
  }
}

std::optional<MountedState> ToResult(
    std::optional<user_data_auth::IsMountedReply> reply) {
  if (!reply.has_value()) {
    return std::nullopt;
  }
  if (IsTestOrLinuxChromeOS()) {
    // In tests and in linux-chromeos there is no real cryptohome, and the fake
    // one always replies with `is_mounted()` true. We override the reply so
    // Kiosk login can proceed.
    return MountedState::kNotMounted;
  }
  return reply->is_mounted() ? MountedState::kMounted
                             : MountedState::kNotMounted;
}

void CheckCryptohomeMountState(CryptohomeMountStateCallback on_done) {
  UserDataAuthClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      [](CryptohomeMountStateCallback on_done, bool service_is_ready) {
        if (!service_is_ready || !UserDataAuthClient::Get()) {
          return std::move(on_done).Run(std::nullopt);
        }

        UserDataAuthClient::Get()->IsMounted(
            user_data_auth::IsMountedRequest(),
            base::BindOnce(&ToResult).Then(std::move(on_done)));
      },
      std::move(on_done)));
}

// Checks the mount state of cryptohome and retries if the cryptohome service is
// not yet available.
std::unique_ptr<CancellableJob> CheckCryptohome(
    CryptohomeMountStateCallback on_done) {
  return RunUpToNTimes<MountedState>(
      /*n=*/5,
      /*job=*/base::BindRepeating(&CheckCryptohomeMountState),
      /*on_done=*/std::move(on_done));
}

class SigninPerformer : public LoginPerformer::Delegate, public CancellableJob {
 public:
  enum class LoginError { kPolicyLoadFailed, kAllowlistCheckFailed };
  using ErrorResult = std::variant<LoginError, AuthFailure>;
  using Result = base::expected<UserContext, ErrorResult>;
  using ResultCallback = base::OnceCallback<void(Result result)>;

  static std::unique_ptr<CancellableJob> Run(KioskAppType app_type,
                                             AccountId account_id,
                                             ResultCallback on_done) {
    auto handle = base::WrapUnique(new SigninPerformer(std::move(on_done)));

    switch (app_type) {
      case KioskAppType::kChromeApp:
        handle->login_performer_->LoginAsKioskAccount(account_id);
        break;
      case KioskAppType::kWebApp:
        handle->login_performer_->LoginAsWebKioskAccount(account_id);
        break;
    }

    return handle;
  }

  SigninPerformer(const SigninPerformer&) = delete;
  SigninPerformer& operator=(const SigninPerformer&) = delete;
  ~SigninPerformer() override = default;

 private:
  explicit SigninPerformer(ResultCallback on_done)
      : on_done_(std::move(on_done)),
        login_performer_(std::make_unique<ChromeLoginPerformer>(
            this,
            AuthEventsRecorder::Get())) {}

  // LoginPerformer::Delegate overrides:
  void OnAuthSuccess(const UserContext& user_context) override {
    // `LoginPerformer` manages its own lifecycle on success, release ownership.
    login_performer_->set_delegate(nullptr);
    std::ignore = login_performer_.release();

    std::move(on_done_).Run(user_context);
  }
  void OnAuthFailure(const AuthFailure& auth_error) override {
    KioskAppLaunchError::SaveCryptohomeFailure(auth_error);
    std::move(on_done_).Run(base::unexpected(auth_error));
  }
  void PolicyLoadFailed() override {
    std::move(on_done_).Run(base::unexpected(LoginError::kPolicyLoadFailed));
  }
  void AllowlistCheckFailed(const std::string& email) override {
    std::move(on_done_).Run(
        base::unexpected(LoginError::kAllowlistCheckFailed));
  }
  void OnOldEncryptionDetected(std::unique_ptr<UserContext> user_context,
                               bool has_incomplete_migration) override {
    NOTREACHED_NORETURN();
  }

  ResultCallback on_done_;
  std::unique_ptr<LoginPerformer> login_performer_;
};

bool IsRetriableError(const SigninPerformer::Result& result) {
  if (!result.has_value() &&
      std::holds_alternative<AuthFailure>(result.error())) {
    // Signal a retriable error if the cryptohome mount failed due to
    // corruption of the on-disk state. We always ask to "create" cryptohome
    // and the corrupted one was deleted under the hood.
    return std::get<AuthFailure>(result.error()).reason() ==
           AuthFailure::UNRECOVERABLE_CRYPTOHOME;
  }
  return false;
}

std::unique_ptr<CancellableJob> Signin(
    KioskAppType app_type,
    AccountId account_id,
    SigninPerformer::ResultCallback on_done) {
  return RunUpToNTimes<SigninPerformer::Result>(
      /*n=*/3,
      /*job=*/
      base::BindRepeating(
          [](KioskAppType app_type, AccountId account_id,
             RetryResultCallback<SigninPerformer::Result> on_result) {
            return SigninPerformer::Run(app_type, account_id,
                                        std::move(on_result));
          },
          app_type, account_id),
      /*should_retry=*/base::BindRepeating(&IsRetriableError),
      /*on_done=*/
      std::move(on_done));
}
KioskAppLaunchError::Error SigninErrorToKioskLaunchError(
    SigninPerformer::LoginError error) {
  switch (error) {
    case SigninPerformer::LoginError::kPolicyLoadFailed:
      return KioskAppLaunchError::Error::kPolicyLoadFailed;
    case SigninPerformer::LoginError::kAllowlistCheckFailed:
      return KioskAppLaunchError::Error::kUserNotAllowlisted;
  }
}

class SessionStarter : public CancellableJob,
                       public UserSessionManagerDelegate {
 public:
  using ResultCallback = base::OnceCallback<void(Profile& result)>;

  static std::unique_ptr<CancellableJob> Run(const UserContext& user_context,
                                             ResultCallback on_done) {
    auto handle = base::WrapUnique(new SessionStarter(std::move(on_done)));
    UserSessionManager::GetInstance()->StartSession(
        user_context, UserSessionManager::StartSessionType::kPrimary,
        /*has_auth_cookies=*/false,
        /*has_active_session=*/false,
        /*delegate=*/handle->weak_ptr_factory_.GetWeakPtr());
    return handle;
  }

  SessionStarter(const SessionStarter&) = delete;
  SessionStarter& operator=(const SessionStarter&) = delete;
  ~SessionStarter() override = default;

 private:
  explicit SessionStarter(ResultCallback on_done)
      : on_done_(std::move(on_done)) {}

  // UserSessionManagerDelegate implementation:
  void OnProfilePrepared(Profile* profile, bool browser_launched) override {
    std::move(on_done_).Run(CHECK_DEREF(profile));
  }

  base::WeakPtr<UserSessionManagerDelegate> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  ResultCallback on_done_;
  base::WeakPtrFactory<SessionStarter> weak_ptr_factory_{this};
};

void LogErrorToSyslog(KioskAppLaunchError::Error error) {
  switch (error) {
    case KioskAppLaunchError::Error::kCryptohomedNotRunning:
      SYSLOG(ERROR) << "Cryptohome not available when loading Kiosk profile.";
      break;
    case KioskAppLaunchError::Error::kAlreadyMounted:
      SYSLOG(ERROR) << "Cryptohome already mounted when loading Kiosk profile.";
      break;
    case KioskAppLaunchError::Error::kUserNotAllowlisted:
      SYSLOG(ERROR) << "LoginPerformer disallowed Kiosk user sign in.";
      break;
    default:
      SYSLOG(ERROR) << "Unexpected error " << (int)error;
  }
}

}  // namespace

std::unique_ptr<CancellableJob> LoadProfile(
    const AccountId& app_account_id,
    KioskAppType app_type,
    KioskProfileLoader::ResultCallback on_done) {
  return KioskProfileLoader::Run(app_account_id, app_type, std::move(on_done));
}

std::unique_ptr<CancellableJob> KioskProfileLoader::Run(
    const AccountId& app_account_id,
    KioskAppType app_type,
    ResultCallback on_done) {
  auto loader = base::WrapUnique(
      new KioskProfileLoader(app_account_id, app_type, std::move(on_done)));
  loader->CheckCryptohomeIsNotMounted();
  return loader;
}

KioskProfileLoader::KioskProfileLoader(const AccountId& app_account_id,
                                       KioskAppType app_type,
                                       ResultCallback on_done)
    : account_id_(app_account_id),
      app_type_(app_type),
      on_done_(std::move(on_done)) {}

KioskProfileLoader::~KioskProfileLoader() = default;

void KioskProfileLoader::CheckCryptohomeIsNotMounted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_ = CheckCryptohome(base::BindOnce(
      [](KioskProfileLoader* self, std::optional<MountedState> result) {
        if (!result.has_value()) {
          return self->ReturnError(
              KioskAppLaunchError::Error::kCryptohomedNotRunning);
        }
        switch (result.value()) {
          case MountedState::kNotMounted:
            return self->LoginAsKioskAccount();
          case MountedState::kMounted:
            return self->ReturnError(
                KioskAppLaunchError::Error::kAlreadyMounted);
        }
      },
      // Safe because `this` owns `current_step_`
      base::Unretained(this)));
}

void KioskProfileLoader::LoginAsKioskAccount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_ = Signin(
      app_type_, account_id_,
      /*on_done=*/
      base::BindOnce(
          [](KioskProfileLoader* self, SigninPerformer::Result result) {
            if (result.has_value()) {
              return self->PrepareProfile(result.value());
            } else if (auto* error = std::get_if<SigninPerformer::LoginError>(
                           &result.error())) {
              return self->ReturnError(SigninErrorToKioskLaunchError(*error));
            } else if (auto* auth_failure =
                           std::get_if<AuthFailure>(&result.error())) {
              return self->ReturnError(
                  LoginFailureToKioskLaunchError(*auth_failure));
            }
            NOTREACHED_NORETURN();
          },
          // Safe because `this` owns `current_step_`
          base::Unretained(this)));
}

void KioskProfileLoader::PrepareProfile(const UserContext& user_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_ = SessionStarter::Run(
      user_context, base::BindOnce(&KioskProfileLoader::ReturnSuccess,
                                   // Safe because `this` owns `current_step_`
                                   base::Unretained(this)));
}

void KioskProfileLoader::ReturnSuccess(Profile& profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_.reset();
  std::move(on_done_).Run(&profile);
}

void KioskProfileLoader::ReturnError(KioskAppLaunchError::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_step_.reset();
  LogErrorToSyslog(result);
  std::move(on_done_).Run(base::unexpected(std::move(result)));
}

}  // namespace ash
