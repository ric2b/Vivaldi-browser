// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/chrome_user_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/users/affiliation.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/external_data/handlers/crostini_ansible_playbook_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/preconfigured_desk_templates_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/print_servers_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/printers_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/user_avatar_image_external_data_handler.h"
#include "chrome/browser/ash/policy/external_data/handlers/wallpaper_image_external_data_handler.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/timezone/timezone_resolver.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "components/account_id/account_id.h"
#include "components/crash/core/common/crash_key.h"
#include "components/policy/core/common/cloud/affiliation.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/wm/core/wm_core_switches.h"

namespace ash {

// TODO(b/278643115) Remove the using when moved.
namespace prefs {
using user_manager::prefs::kDeviceLocalAccountPendingDataRemoval;
using user_manager::prefs::kDeviceLocalAccountsWithSavedData;
using user_manager::prefs::kRegularUsersPref;
}  // namespace prefs

namespace {

using ::content::BrowserThread;

policy::MinimumVersionPolicyHandler* GetMinimumVersionPolicyHandler() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetMinimumVersionPolicyHandler();
}

user_manager::UserManager::EphemeralModeConfig CreateEphemeralModeConfig(
    ash::CrosSettings* cros_settings) {
  DCHECK(cros_settings);

  bool ephemeral_users_enabled = false;
  // Only `ChromeUserManagerImpl` is allowed to directly use this setting. All
  // other clients have to use `UserManager::IsEphemeralAccountId()` function to
  // get ephemeral mode for account ID. Such rule is needed because there are
  // new policies(e.g.kiosk ephemeral mode) that overrides behaviour of
  // the current setting for some accounts.
  cros_settings->GetBoolean(ash::kAccountsPrefEphemeralUsersEnabled,
                            &ephemeral_users_enabled);

  std::vector<AccountId> ephemeral_accounts, non_ephemeral_accounts;

  const auto accounts = policy::GetDeviceLocalAccounts(cros_settings);
  for (const auto& account : accounts) {
    switch (account.ephemeral_mode) {
      case policy::DeviceLocalAccount::EphemeralMode::kEnable:
        ephemeral_accounts.push_back(AccountId::FromUserEmail(account.user_id));
        break;
      case policy::DeviceLocalAccount::EphemeralMode::kDisable:
        non_ephemeral_accounts.push_back(
            AccountId::FromUserEmail(account.user_id));
        break;
      case policy::DeviceLocalAccount::EphemeralMode::kUnset:
      case policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy:
        // Do nothing.
        break;
    }
  }

  return user_manager::UserManager::EphemeralModeConfig(
      ephemeral_users_enabled, std::move(ephemeral_accounts),
      std::move(non_ephemeral_accounts));
}

}  // namespace

// static
std::unique_ptr<ChromeUserManagerImpl>
ChromeUserManagerImpl::CreateChromeUserManager() {
  return base::WrapUnique(new ChromeUserManagerImpl());
}

ChromeUserManagerImpl::ChromeUserManagerImpl()
    : UserManagerBase(
          std::make_unique<UserManagerDelegateImpl>(),
          base::SingleThreadTaskRunner::HasCurrentDefault()
              ? base::SingleThreadTaskRunner::GetCurrentDefault()
              : nullptr,
          g_browser_process ? g_browser_process->local_state() : nullptr,
          CrosSettings::Get()),
      device_local_account_policy_service_(nullptr) {
  // UserManager instance should be used only on UI thread.
  // (or in unit tests)
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  DeviceSettingsService::Get()->AddObserver(this);
  if (ProfileManager* profile_manager = g_browser_process->profile_manager()) {
    profile_manager_observation_.Observe(profile_manager);
  }

  // Since we're in ctor postpone any actions till this is fully created.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                       weak_factory_.GetWeakPtr()));
  }

  allow_guest_subscription_ = cros_settings()->AddSettingsObserver(
      kAccountsPrefAllowGuest,
      base::BindRepeating(&UserManager::NotifyUsersSignInConstraintsChanged,
                          weak_factory_.GetWeakPtr()));
  // For user allowlist.
  users_subscription_ = cros_settings()->AddSettingsObserver(
      kAccountsPrefUsers,
      base::BindRepeating(&UserManager::NotifyUsersSignInConstraintsChanged,
                          weak_factory_.GetWeakPtr()));
  family_link_accounts_subscription_ = cros_settings()->AddSettingsObserver(
      kAccountsPrefFamilyLinkAccountsAllowed,
      base::BindRepeating(&UserManager::NotifyUsersSignInConstraintsChanged,
                          weak_factory_.GetWeakPtr()));

  ephemeral_users_enabled_subscription_ = cros_settings()->AddSettingsObserver(
      kAccountsPrefEphemeralUsersEnabled,
      base::BindRepeating(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                          weak_factory_.GetWeakPtr()));
  local_accounts_subscription_ = cros_settings()->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccounts,
      base::BindRepeating(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                          weak_factory_.GetWeakPtr()));

  // |this| is sometimes initialized before owner is ready in CrosSettings for
  // the consoldiated consent screen flow. Listen for changes to owner setting
  // to ensure that owner changes are reflected in |this|.
  // TODO(crbug.com/1307359): Investigate using RetrieveTrustedDevicePolicies
  // instead of UpdateOwnerId.
  owner_subscription_ = cros_settings()->AddSettingsObserver(
      kDeviceOwner, base::BindRepeating(&ChromeUserManagerImpl::UpdateOwnerId,
                                        weak_factory_.GetWeakPtr()));

  if (GetMinimumVersionPolicyHandler()) {
    GetMinimumVersionPolicyHandler()->AddObserver(this);
  }

  // TODO(b/278643115): Move this out from ChromeUserManagerImpl.
  policy::DeviceLocalAccountPolicyService* device_local_account_policy_service =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetDeviceLocalAccountPolicyService();
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings(), device_local_account_policy_service,
          policy::key::kUserAvatarImage, this,
          std::make_unique<policy::UserAvatarImageExternalDataHandler>()));
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings(), device_local_account_policy_service,
          policy::key::kWallpaperImage, this,
          std::make_unique<policy::WallpaperImageExternalDataHandler>()));
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings(), device_local_account_policy_service,
          policy::key::kPrintersBulkConfiguration, this,
          std::make_unique<policy::PrintersExternalDataHandler>()));
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings(), device_local_account_policy_service,
          policy::key::kExternalPrintServers, this,
          std::make_unique<policy::PrintServersExternalDataHandler>()));
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings(), device_local_account_policy_service,
          policy::key::kCrostiniAnsiblePlaybook, this,
          std::make_unique<
              policy::CrostiniAnsiblePlaybookExternalDataHandler>()));
  cloud_external_data_policy_observers_.push_back(
      std::make_unique<policy::CloudExternalDataPolicyObserver>(
          cros_settings(), device_local_account_policy_service,
          policy::key::kPreconfiguredDeskTemplates, this,
          std::make_unique<
              policy::PreconfiguredDeskTemplatesExternalDataHandler>()));
  for (auto& observer : cloud_external_data_policy_observers_) {
    observer->Init();
  }
}

void ChromeUserManagerImpl::UpdateOwnerId() {
  std::string owner_email;
  cros_settings()->GetString(kDeviceOwner, &owner_email);

  user_manager::KnownUser known_user(GetLocalState());
  const AccountId owner_account_id = known_user.GetAccountId(
      owner_email, std::string() /* id */, AccountType::UNKNOWN);
  SetOwnerId(owner_account_id);
}

ChromeUserManagerImpl::~ChromeUserManagerImpl() {
  if (DeviceSettingsService::IsInitialized()) {
    DeviceSettingsService::Get()->RemoveObserver(this);
  }
}

void ChromeUserManagerImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UserManagerBase::Shutdown();

  if (GetMinimumVersionPolicyHandler()) {
    GetMinimumVersionPolicyHandler()->RemoveObserver(this);
  }

  ephemeral_users_enabled_subscription_ = {};
  local_accounts_subscription_ = {};

  if (device_local_account_policy_service_) {
    device_local_account_policy_service_->RemoveObserver(this);
  }

  cloud_external_data_policy_observers_.clear();
}

void ChromeUserManagerImpl::StopPolicyObserverForTesting() {
  cloud_external_data_policy_observers_.clear();
}

void ChromeUserManagerImpl::OwnershipStatusChanged() {
  if (!device_local_account_policy_service_) {
    policy::BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    device_local_account_policy_service_ =
        connector->GetDeviceLocalAccountPolicyService();
    if (device_local_account_policy_service_) {
      device_local_account_policy_service_->AddObserver(this);
    }
  }
  RetrieveTrustedDevicePolicies();
}

void ChromeUserManagerImpl::OnPolicyUpdated(const std::string& user_id) {
  user_manager::KnownUser known_user(GetLocalState());
  const AccountId account_id = known_user.GetAccountId(
      user_id, std::string() /* id */, AccountType::UNKNOWN);
  const user_manager::User* user = FindUser(account_id);
  if (!user || user->GetType() != user_manager::UserType::kPublicAccount) {
    return;
  }
  UpdatePublicAccountDisplayName(user_id);
}

void ChromeUserManagerImpl::OnDeviceLocalAccountsChanged() {
  // No action needed here, changes to the list of device-local accounts get
  // handled via the kAccountsPrefDeviceLocalAccounts device setting observer.
}

void ChromeUserManagerImpl::RetrieveTrustedDevicePolicies() {
  // Local state may not be initialized in unit_tests.
  if (!GetLocalState()) {
    return;
  }

  SetEphemeralModeConfig(EphemeralModeConfig());

  // Schedule a callback if device policy has not yet been verified.
  if (CrosSettingsProvider::TRUSTED !=
      cros_settings()->PrepareTrustedValues(
          base::BindOnce(&ChromeUserManagerImpl::RetrieveTrustedDevicePolicies,
                         weak_factory_.GetWeakPtr()))) {
    return;
  }

  SetEphemeralModeConfig(CreateEphemeralModeConfig(cros_settings()));

  std::string owner_email;
  cros_settings()->GetString(kDeviceOwner, &owner_email);
  user_manager::KnownUser known_user(GetLocalState());
  const AccountId owner_account_id = known_user.GetAccountId(
      owner_email, std::string() /* id */, AccountType::UNKNOWN);
  SetOwnerId(owner_account_id);

  bool changed = UpdateAndCleanUpDeviceLocalAccounts(
      policy::GetDeviceLocalAccounts(cros_settings()));

  // Remove ephemeral regular users (except the owner) when on the login screen.
  if (!IsUserLoggedIn()) {
    ScopedListPrefUpdate prefs_users_update(GetLocalState(),
                                            prefs::kRegularUsersPref);
    // Take snapshot because DeleteUser called in the loop will update it.
    std::vector<raw_ptr<user_manager::User, VectorExperimental>> users = users_;
    for (user_manager::User* user : users) {
      const AccountId account_id = user->GetAccountId();
      if (user->HasGaiaAccount() && account_id != GetOwnerAccountId() &&
          IsEphemeralAccountId(account_id)) {
        user_manager::UserManager::Get()->NotifyUserToBeRemoved(account_id);
        RemoveNonCryptohomeData(account_id);
        DeleteUser(user);
        user_manager::UserManager::Get()->NotifyUserRemoved(
            account_id,
            user_manager::UserRemovalReason::DEVICE_EPHEMERAL_USERS_ENABLED);

        prefs_users_update->EraseValue(base::Value(account_id.GetUserEmail()));
        changed = true;
      }
    }
  }

  if (changed) {
    NotifyLocalStateChanged();
  }
}

void ChromeUserManagerImpl::RemoveNonCryptohomeData(
    const AccountId& account_id) {
  // TODO(tbarzic): Forward data removal request to HammerDeviceHandler,
  // instead of removing the prefs value here.
  if (GetLocalState()->FindPreference(prefs::kDetachableBaseDevices)) {
    ScopedDictPrefUpdate update(GetLocalState(), prefs::kDetachableBaseDevices);
    update->Remove(account_id.HasAccountIdKey() ? account_id.GetAccountIdKey()
                                                : account_id.GetUserEmail());
  }

  UserManagerBase::RemoveNonCryptohomeData(account_id);
}

void ChromeUserManagerImpl::
    CleanUpDeviceLocalAccountNonCryptohomeDataPendingRemoval() {
  PrefService* local_state = GetLocalState();
  const std::string device_local_account_pending_data_removal =
      local_state->GetString(prefs::kDeviceLocalAccountPendingDataRemoval);
  if (device_local_account_pending_data_removal.empty() ||
      (IsUserLoggedIn() &&
       device_local_account_pending_data_removal ==
           GetActiveUser()->GetAccountId().GetUserEmail())) {
    return;
  }

  RemoveNonCryptohomeData(
      AccountId::FromUserEmail(device_local_account_pending_data_removal));
  local_state->ClearPref(prefs::kDeviceLocalAccountPendingDataRemoval);
}

void ChromeUserManagerImpl::CleanUpDeviceLocalAccountNonCryptohomeData(
    const std::vector<std::string>& old_device_local_accounts) {
  std::set<std::string> users;
  for (user_manager::UserList::const_iterator it = users_.begin();
       it != users_.end(); ++it) {
    users.insert((*it)->GetAccountId().GetUserEmail());
  }

  // If the user is logged into a device local account that has been removed
  // from the user list, mark the account's data as pending removal after
  // logout.
  const user_manager::User* const active_user = GetActiveUser();
  if (active_user && active_user->IsDeviceLocalAccount()) {
    const std::string active_user_id =
        active_user->GetAccountId().GetUserEmail();
    if (users.find(active_user_id) == users.end()) {
      GetLocalState()->SetString(prefs::kDeviceLocalAccountPendingDataRemoval,
                                 active_user_id);
      users.insert(active_user_id);
    }
  }

  // Remove the data belonging to any other device local accounts that are no
  // longer found on the user list.
  for (std::vector<std::string>::const_iterator it =
           old_device_local_accounts.begin();
       it != old_device_local_accounts.end(); ++it) {
    if (users.find(*it) == users.end()) {
      RemoveNonCryptohomeData(AccountId::FromUserEmail(*it));
    }
  }
}

bool ChromeUserManagerImpl::UpdateAndCleanUpDeviceLocalAccounts(
    const std::vector<policy::DeviceLocalAccount>& device_local_accounts) {
  // Try to remove any device local account data marked as pending removal.
  CleanUpDeviceLocalAccountNonCryptohomeDataPendingRemoval();

  // Get the current list of device local accounts.
  std::vector<std::string> old_accounts;
  for (user_manager::User* user : users_) {
    if (user->IsDeviceLocalAccount()) {
      old_accounts.push_back(user->GetAccountId().GetUserEmail());
    }
  }

  // Persist the new list of device local accounts in a pref. These accounts
  // will be loaded in LoadDeviceLocalAccounts() on the next reboot regardless
  // of whether they still exist in kAccountsPrefDeviceLocalAccounts, allowing
  // us to clean up associated data if they disappear from policy.
  ScopedListPrefUpdate prefs_device_local_accounts_update(
      GetLocalState(), prefs::kDeviceLocalAccountsWithSavedData);
  prefs_device_local_accounts_update->clear();
  for (const auto& account : device_local_accounts) {
    prefs_device_local_accounts_update->Append(account.user_id);
  }

  // If the list of device local accounts has not changed, return.
  if (device_local_accounts.size() == old_accounts.size()) {
    bool changed = false;
    for (size_t i = 0; i < device_local_accounts.size(); ++i) {
      if (device_local_accounts[i].user_id != old_accounts[i]) {
        changed = true;
        break;
      }
    }
    if (!changed) {
      return false;
    }
  }

  // Remove the old device local accounts from the user list.
  // Take snapshot because DeleteUser will update |user_|.
  std::vector<raw_ptr<user_manager::User, VectorExperimental>> users = users_;
  for (user_manager::User* user : users) {
    if (user->IsDeviceLocalAccount()) {
      if (user != GetActiveUser()) {
        DeleteUser(user);
      } else {
        std::erase(users_, user);
      }
    }
  }

  // Add the new device local accounts to the front of the user list.
  user_manager::User* const active_user = GetActiveUser();
  const bool is_device_local_account_session =
      active_user && active_user->IsDeviceLocalAccount();
  for (const policy::DeviceLocalAccount& account :
       base::Reversed(device_local_accounts)) {
    if (is_device_local_account_session &&
        AccountId::FromUserEmail(account.user_id) ==
            active_user->GetAccountId()) {
      users_.insert(users_.begin(), active_user);
    } else {
      auto user_type =
          chrome_user_manager_util::DeviceLocalAccountTypeToUserType(
              account.type);
      CHECK(user_type.has_value());
      // Using `new` to access a non-public constructor.
      user_storage_.push_back(base::WrapUnique(new user_manager::User(
          AccountId::FromUserEmail(account.user_id), *user_type)));
      users_.insert(users_.begin(), user_storage_.back().get());
    }
    if (account.type == policy::DeviceLocalAccountType::kPublicSession ||
        account.type == policy::DeviceLocalAccountType::kSamlPublicSession) {
      UpdatePublicAccountDisplayName(account.user_id);
    }
  }

  for (auto& observer : observer_list_) {
    observer.OnDeviceLocalUserListUpdated();
  }

  // Remove data belonging to device local accounts that are no longer found on
  // the user list.
  CleanUpDeviceLocalAccountNonCryptohomeData(old_accounts);

  return true;
}

void ChromeUserManagerImpl::UpdatePublicAccountDisplayName(
    const std::string& user_id) {
  std::string display_name;

  if (device_local_account_policy_service_) {
    policy::DeviceLocalAccountPolicyBroker* broker =
        device_local_account_policy_service_->GetBrokerForUser(user_id);
    if (broker) {
      display_name = broker->GetDisplayName();
      // Set or clear the display name.
      SaveUserDisplayName(AccountId::FromUserEmail(user_id),
                          base::UTF8ToUTF16(display_name));
    }
  }
}

void ChromeUserManagerImpl::OnMinimumVersionStateChanged() {
  NotifyUsersSignInConstraintsChanged();
}

void ChromeUserManagerImpl::OnProfileCreationStarted(Profile* profile) {
  // Find a User instance from directory path, and annotate the AccountId.
  // Hereafter, we can use AnnotatedAccountId::Get() to find the User.
  if (ash::IsUserBrowserContext(profile)) {
    auto logged_in_users = GetLoggedInUsers();
    auto it = base::ranges::find(
        logged_in_users,
        ash::BrowserContextHelper::GetUserIdHashFromBrowserContext(profile),
        [](const user_manager::User* user) { return user->username_hash(); });
    if (it == logged_in_users.end()) {
      // User may not be found for now on testing.
      // TODO(crbug.com/40225390): fix tests to annotate AccountId properly.
      CHECK_IS_TEST();
    } else {
      const user_manager::User* user = *it;
      // A |User| instance should always exist for a profile which is not the
      // initial, the sign-in or the lock screen app profile.
      CHECK(session_manager::SessionManager::Get()->HasSessionForAccountId(
          user->GetAccountId()))
          << "Attempting to construct the profile before starting the user "
             "session";
      ash::AnnotatedAccountId::Set(profile, user->GetAccountId(),
                                   /*for_test=*/false);
    }
  }
}

void ChromeUserManagerImpl::OnProfileAdded(Profile* profile) {
  // TODO(crbug.com/40225390): Use ash::AnnotatedAccountId::Get(), when
  // it gets fully ready for tests.
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (user && OnUserProfileCreated(user->GetAccountId(), profile->GetPrefs())) {
    // Add observer for graceful shutdown of User on Profile destruction.
    auto observation =
        std::make_unique<base::ScopedObservation<Profile, ProfileObserver>>(
            this);
    observation->Observe(profile);
    profile_observations_.push_back(std::move(observation));
  }

  ProcessPendingUserSwitchId();
}

void ChromeUserManagerImpl::OnProfileWillBeDestroyed(Profile* profile) {
  CHECK(std::erase_if(profile_observations_, [profile](auto& observation) {
    return observation->IsObservingSource(profile);
  }));
  // TODO(crbug.com/40225390): User ash::AnnotatedAccountId::Get(), when it gets
  // fully ready for tests.
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (user) {
    OnUserProfileWillBeDestroyed(user->GetAccountId());
  }
}

void ChromeUserManagerImpl::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

}  // namespace ash
