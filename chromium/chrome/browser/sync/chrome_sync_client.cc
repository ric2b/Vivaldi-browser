// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/chrome_sync_client.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/product_specifications/product_specifications_service_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_receiver_service_factory.h"
#include "chrome/browser/password_manager/password_sender_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/plus_addresses/plus_address_setting_service_factory.h"
#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/security_events/security_event_recorder.h"
#include "chrome/browser/security_events/security_event_recorder_factory.h"
#include "chrome/browser/sharing/sharing_message_bridge_factory.h"
#include "chrome/browser/sharing/sharing_message_model_type_controller.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/account_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/tab_group_sync/feature_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_trial.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browser_sync/common_controller_builder.h"
#include "components/browser_sync/sync_api_component_factory_impl.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/data_sharing/public/features.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/plus_addresses/settings/plus_address_setting_service.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sharing_message/sharing_message_bridge.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync/service/sync_api_component_factory.h"
#include "components/sync/service/syncable_service_based_model_type_controller.h"
#include "components/sync/service/trusted_vault_synthetic_field_trial.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_user_events/user_event_service.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/sync/glue/extension_model_type_controller.h"
#include "chrome/browser/sync/glue/extension_setting_model_type_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "components/spellcheck/browser/pref_names.h"
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#elif BUILDFLAG(IS_ANDROID)
#include "components/saved_tab_groups/features.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "components/webauthn/core/browser/passkey_model.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_package_sync_model_type_controller.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service_factory.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"
#include "chrome/browser/ash/printing/printers_sync_bridge.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"
#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/webapk/webapk_sync_service.h"
#include "components/browser_sync/sync_client_utils.h"
#endif  // BUILDFLAG(IS_ANDROID)

#include "app/vivaldi_apptools.h"
#include "sync/note_sync_service_factory.h"

using content::BrowserThread;

namespace browser_sync {

namespace {

// A global variable is needed to detect multiprofile scenarios where more than
// one profile try to register a synthetic field trial.
bool trusted_vault_synthetic_field_trial_registered = false;

#if BUILDFLAG(IS_WIN)
constexpr base::FilePath::CharType kLoopbackServerBackendFilename[] =
    FILE_PATH_LITERAL("profile.pb");
#endif  // BUILDFLAG(IS_WIN)

base::WeakPtr<syncer::SyncableService> GetWeakPtrOrNull(
    syncer::SyncableService* service) {
  return service ? service->AsWeakPtr() : nullptr;
}

base::RepeatingClosure GetDumpStackClosure() {
  return base::BindRepeating(&syncer::ReportUnrecoverableError,
                             chrome::GetChannel());
}

bool ShouldSyncBrowserTypes() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::browser_util::IsAshBrowserSyncEnabled();
#else
  return true;
#endif
}

syncer::ModelTypeSet GetDisabledCommonDataTypes() {
  if (!ShouldSyncBrowserTypes()) {
    // If browser-sync is disabled (on ChromeOS Ash), most "common" data types
    // are disabled. These types will be synced in Lacros instead.
    return base::Difference(syncer::UserTypes(),
                            {syncer::DEVICE_INFO, syncer::USER_CONSENTS});
  }

  // Common case: No disabled types.
  return {};
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// App sync is enabled by default, with the exception of Lacros secondary
// profiles.
bool IsAppSyncEnabled(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!profile->IsMainProfile() &&
      !web_app::IsMainProfileCheckSkippedForTesting()) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  return true;
}

bool ShouldSyncAppsTypesInTransportMode() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // When apps sync controlled by Ash Sync settings, allow running apps-related
  // types (WEB_APPS, APPS and APP_SETTINGS) in transport-only mode using the
  // same `delegate`.
  return base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing);
#else
  return false;
#endif
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

syncer::ModelTypeControllerDelegate* GetSavedTabGroupControllerDelegate(
    Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  auto* keyed_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(profile);
  CHECK(keyed_service);
  return keyed_service->GetSavedTabGroupControllerDelegate().get();
#elif BUILDFLAG(IS_ANDROID)
  return tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile)
      ->GetSavedTabGroupControllerDelegate()
      .get();
#else
  NOTREACHED_NORETURN();
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
}

syncer::ModelTypeControllerDelegate* GetSharedTabGroupControllerDelegate(
    Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  tab_groups::SavedTabGroupKeyedService* keyed_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(profile);
  CHECK(keyed_service);
  return keyed_service->GetSharedTabGroupControllerDelegate().get();
#elif BUILDFLAG(IS_ANDROID)
  return tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile)
      ->GetSharedTabGroupControllerDelegate()
      .get();
#else
  NOTREACHED_NORETURN();
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
}

}  // namespace

ChromeSyncClient::ChromeSyncClient(Profile* profile)
    : profile_(profile), extensions_activity_monitor_(profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  component_factory_ = std::make_unique<SyncApiComponentFactoryImpl>(
      this,
      DeviceInfoSyncServiceFactory::GetForProfile(profile_)
          ->GetDeviceInfoTracker(),
      GetModelTypeStoreService()->GetSyncDataPath());

#if BUILDFLAG(IS_ANDROID)
  scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store = ProfilePasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::IMPLICIT_ACCESS);
  scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store = AccountPasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::IMPLICIT_ACCESS);

  local_data_query_helper_ =
      std::make_unique<browser_sync::LocalDataQueryHelper>(
          profile_password_store.get(), account_password_store.get(),
          BookmarkModelFactory::GetForBrowserContext(profile_),
          ReadingListModelFactory::GetAsDualReadingListForBrowserContext(
              profile_));

  local_data_migration_helper_ =
      std::make_unique<browser_sync::LocalDataMigrationHelper>(
          profile_password_store.get(), account_password_store.get(),
          BookmarkModelFactory::GetForBrowserContext(profile_),
          ReadingListModelFactory::GetAsDualReadingListForBrowserContext(
              profile_));
#endif  // BUILDFLAG(IS_ANDROID)
}

ChromeSyncClient::~ChromeSyncClient() = default;

PrefService* ChromeSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return profile_->GetPrefs();
}

signin::IdentityManager* ChromeSyncClient::GetIdentityManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return IdentityManagerFactory::GetForProfile(profile_);
}

base::FilePath ChromeSyncClient::GetLocalSyncBackendFolder() {
  base::FilePath local_sync_backend_folder =
      GetPrefService()->GetFilePath(syncer::prefs::kLocalSyncBackendDir);

#if BUILDFLAG(IS_WIN)
  if (local_sync_backend_folder.empty()) {
    if (!base::PathService::Get(chrome::DIR_ROAMING_USER_DATA,
                                &local_sync_backend_folder)) {
      SYSLOG(WARNING) << "Local sync can not get the roaming profile folder.";
      return base::FilePath();
    }
  }

  // This code as it is now will assume the same profile order is present on
  // all machines, which is not a given. It is to be defined if only the
  // Default profile should get this treatment or all profile as is the case
  // now.
  // TODO(pastarmovj): http://crbug.com/674928 Decide if only the Default one
  // should be considered roamed. For now the code assumes all profiles are
  // created in the same order on all machines.
  local_sync_backend_folder =
      local_sync_backend_folder.Append(profile_->GetBaseName());
  local_sync_backend_folder =
      local_sync_backend_folder.Append(kLoopbackServerBackendFilename);
#endif  // BUILDFLAG(IS_WIN)

  return local_sync_backend_folder;
}

#if BUILDFLAG(IS_ANDROID)
void ChromeSyncClient::GetLocalDataDescriptions(
    syncer::ModelTypeSet types,
    base::OnceCallback<void(
        std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback) {
  types.RemoveAll(
      local_data_migration_helper_->GetTypesWithOngoingMigrations());
  local_data_query_helper_->Run(types, std::move(callback));
}

void ChromeSyncClient::TriggerLocalDataMigration(syncer::ModelTypeSet types) {
  local_data_migration_helper_->Run(types);
}
#endif  // BUILDFLAG(IS_ANDROID)

syncer::ModelTypeController::TypeVector
ChromeSyncClient::CreateModelTypeControllers(
    syncer::SyncService* sync_service) {
  scoped_refptr<autofill::AutofillWebDataService> profile_web_data_service =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_, ServiceAccessType::IMPLICIT_ACCESS);
  scoped_refptr<autofill::AutofillWebDataService> account_web_data_service =
      WebDataServiceFactory::GetAutofillWebDataForAccount(
          profile_, ServiceAccessType::IMPLICIT_ACCESS);
  scoped_refptr<base::SequencedTaskRunner> web_data_service_thread =
      profile_web_data_service ? profile_web_data_service->GetDBTaskRunner()
                               : nullptr;
  // This class assumes that the database thread is the same across the profile
  // and account storage. This DCHECK makes that assumption explicit.
  DCHECK(!account_web_data_service ||
         web_data_service_thread ==
             account_web_data_service->GetDBTaskRunner());

  browser_sync::CommonControllerBuilder builder;
  builder.SetAutofillWebDataService(
      content::GetUIThreadTaskRunner({}), web_data_service_thread,
      profile_web_data_service, account_web_data_service);
  builder.SetBookmarkModel(
      BookmarkModelFactory::GetForBrowserContext(profile_));
  builder.SetBookmarkSyncService(
      LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(profile_),
      AccountBookmarkSyncServiceFactory::GetForProfile(profile_));
  builder.SetConsentAuditor(ConsentAuditorFactory::GetForProfile(profile_));
  builder.SetDataSharingService(
      data_sharing::DataSharingServiceFactory::GetForProfile(profile_));
  builder.SetDeviceInfoSyncService(
      DeviceInfoSyncServiceFactory::GetForProfile(profile_));
  builder.SetDualReadingListModel(
      ReadingListModelFactory::GetAsDualReadingListForBrowserContext(profile_));
  builder.SetFaviconService(FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetGoogleGroupsManager(
      GoogleGroupsManagerFactory::GetForBrowserContext(profile_));
  builder.SetHistoryService(HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS));
  builder.SetIdentityManager(GetIdentityManager());
  builder.SetModelTypeStoreService(
      ModelTypeStoreServiceFactory::GetForProfile(profile_));
#if !BUILDFLAG(IS_ANDROID)
  builder.SetPasskeyModel(
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)
          ? PasskeyModelFactory::GetForProfile(profile_)
          : nullptr);
#endif  // !BUILDFLAG(IS_ANDROID)
  builder.SetPasswordReceiverService(
      PasswordReceiverServiceFactory::GetForProfile(profile_));
  builder.SetPasswordSenderService(
      PasswordSenderServiceFactory::GetForProfile(profile_));
  builder.SetPasswordStore(ProfilePasswordStoreFactory::GetForProfile(
                               profile_, ServiceAccessType::IMPLICIT_ACCESS),
                           AccountPasswordStoreFactory::GetForProfile(
                               profile_, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetPlusAddressServices(
      PlusAddressSettingServiceFactory::GetForBrowserContext(profile_),
      WebDataServiceFactory::GetPlusAddressWebDataForProfile(
          profile_, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetPowerBookmarkService(
      PowerBookmarkServiceFactory::GetForBrowserContext(profile_));
  builder.SetPrefService(profile_->GetPrefs());
  builder.SetPrefServiceSyncable(PrefServiceSyncableFromProfile(profile_));
  builder.SetProductSpecificationsService(
      commerce::ProductSpecificationsServiceFactory::GetForBrowserContext(
          profile_));
  builder.SetSendTabToSelfSyncService(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile_));
  builder.SetSessionSyncService(
      SessionSyncServiceFactory::GetForProfile(profile_));
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  builder.SetSupervisedUserSettingsService(
      SupervisedUserSettingsServiceFactory::GetForKey(
          profile_->GetProfileKey()));
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
  builder.SetUserEventService(
      browser_sync::UserEventServiceFactory::GetForProfile(profile_));

  // Vivaldi
  builder.SetNoteSyncService(
      vivaldi::NoteSyncServiceFactory::GetForProfile(profile_));
  // End Vivaldi

  syncer::ModelTypeController::TypeVector controllers = builder.Build(
      GetDisabledCommonDataTypes(), sync_service, chrome::GetChannel());

  const base::RepeatingClosure dump_stack = GetDumpStackClosure();

  syncer::RepeatingModelTypeStoreFactory model_type_store_factory =
      GetModelTypeStoreService()->GetStoreFactory();

  if (ShouldSyncBrowserTypes()) {
    syncer::ModelTypeControllerDelegate* security_events_delegate =
        SecurityEventRecorderFactory::GetForProfile(profile_)
            ->GetControllerDelegate()
            .get();
    // Forward both full-sync and transport-only modes to the same delegate,
    // since behavior for SECURITY_EVENTS does not differ.
    controllers.push_back(std::make_unique<syncer::ModelTypeController>(
        syncer::SECURITY_EVENTS,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            security_events_delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            security_events_delegate)));

    // Forward both full-sync and transport-only modes to the same delegate,
    // since behavior for SHARING_MESSAGE does not differ. They both do not
    // store data on persistent storage.
    syncer::ModelTypeControllerDelegate* sharing_message_delegate =
        SharingMessageBridgeFactory::GetForBrowserContext(profile_)
            ->GetControllerDelegate()
            .get();
    controllers.push_back(std::make_unique<SharingMessageModelTypeController>(
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            sharing_message_delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            sharing_message_delegate)));

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Extension sync is enabled by default.
    controllers.push_back(std::make_unique<ExtensionModelTypeController>(
        syncer::EXTENSIONS, model_type_store_factory,
        GetSyncableServiceForType(syncer::EXTENSIONS), dump_stack,
        ExtensionModelTypeController::DelegateMode::kLegacyFullSyncModeOnly,
        profile_));

    // Extension setting sync is enabled by default.
    controllers.push_back(std::make_unique<ExtensionSettingModelTypeController>(
        syncer::EXTENSION_SETTINGS, model_type_store_factory,
        extensions::settings_sync_util::GetSyncableServiceProvider(
            profile_, syncer::EXTENSION_SETTINGS),
        dump_stack,
        ExtensionSettingModelTypeController::DelegateMode::
            kLegacyFullSyncModeOnly,
        profile_));

    if (IsAppSyncEnabled(profile_)) {
      controllers.push_back(CreateAppsModelTypeController());

      controllers.push_back(CreateAppSettingsModelTypeController(sync_service));

      if (web_app::AreWebAppsEnabled(profile_) &&
          web_app::WebAppProvider::GetForWebApps(profile_)) {
        controllers.push_back(CreateWebAppsModelTypeController());
      }
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
      syncer::ModelTypeControllerDelegate* delegate =
          webapk::WebApkSyncService::GetForProfile(profile_)
              ->GetModelTypeControllerDelegate()
              .get();
      controllers.push_back(std::make_unique<syncer::ModelTypeController>(
          syncer::WEB_APKS,
          /*delegate_for_full_sync_mode=*/
          std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
              delegate),
          /*delegate_for_transport_mode=*/
          std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
              delegate)));
    }
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
    // Theme sync is enabled by default.
    controllers.push_back(std::make_unique<ExtensionModelTypeController>(
        syncer::THEMES, model_type_store_factory,
        GetSyncableServiceForType(syncer::THEMES), dump_stack,
        ExtensionModelTypeController::DelegateMode::kLegacyFullSyncModeOnly,
        profile_));
#endif  // !BUILDFLAG(IS_ANDROID)

    // Search Engine sync is enabled by default.
    controllers.push_back(
        std::make_unique<syncer::SyncableServiceBasedModelTypeController>(
            syncer::SEARCH_ENGINES, model_type_store_factory,
            GetSyncableServiceForType(syncer::SEARCH_ENGINES), dump_stack,
            syncer::SyncableServiceBasedModelTypeController::DelegateMode::
                kLegacyFullSyncModeOnly));
// #endif  // !BUILDFLAG(IS_ANDROID)

    // Tab group sync is enabled via separate feature flags on different
    // platforms.
    bool enable_tab_group_sync = false;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    enable_tab_group_sync = true;
#elif BUILDFLAG(IS_ANDROID)
    enable_tab_group_sync =
        tab_groups::IsTabGroupSyncEnabled(GetPrefService()) &&
        !base::FeatureList::IsEnabled(
            tab_groups::kTabGroupSyncDisableNetworkLayer);
    tab_groups::TabGroupTrial::OnTabgroupSyncEnabled(enable_tab_group_sync);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

    if (enable_tab_group_sync) {
      syncer::ModelTypeControllerDelegate* delegate =
          GetSavedTabGroupControllerDelegate(profile_);

      controllers.push_back(std::make_unique<syncer::ModelTypeController>(
          syncer::SAVED_TAB_GROUP,
          /*delegate_for_full_sync_mode=*/
          std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
              delegate),
          /*delegate_for_transport_mode=*/
          std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
              delegate)));
    }

    if (base::FeatureList::IsEnabled(
            data_sharing::features::kDataSharingFeature)) {
      syncer::ModelTypeControllerDelegate* delegate =
          GetSharedTabGroupControllerDelegate(profile_);
      controllers.push_back(std::make_unique<syncer::ModelTypeController>(
          syncer::SHARED_TAB_GROUP_DATA,
          /*delegate_for_full_sync_mode=*/
          std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
              delegate),
          /*delegate_for_transport_mode=*/
          std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
              delegate)));
    }

// Chrome prefers OS provided spell checkers where they exist. So only sync the
// custom dictionary on platforms that typically don't provide one.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    // Dictionary sync is enabled by default.
    if (GetPrefService()->GetBoolean(spellcheck::prefs::kSpellCheckEnable)) {
      controllers.push_back(
          std::make_unique<syncer::SyncableServiceBasedModelTypeController>(
              syncer::DICTIONARY, model_type_store_factory,
              GetSyncableServiceForType(syncer::DICTIONARY), dump_stack,
              syncer::SyncableServiceBasedModelTypeController::DelegateMode::
                  kLegacyFullSyncModeOnly));
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Some profile types (e.g. sign-in screen) don't support app list.
  // Temporarily Disable AppListSyncableService for tablet form factor devices.
  // See crbug/1013732 for details.
  if (app_list::AppListSyncableServiceFactory::GetForProfile(profile_) &&
      !ash::switches::IsTabletFormFactor()) {
    // Runs in sync transport-mode and full-sync mode.
    controllers.push_back(
        std::make_unique<syncer::SyncableServiceBasedModelTypeController>(
            syncer::APP_LIST, model_type_store_factory,
            GetSyncableServiceForType(syncer::APP_LIST), dump_stack,
            syncer::SyncableServiceBasedModelTypeController::DelegateMode::
                kTransportModeWithSingleModel));
  }

  if (arc::IsArcAllowedForProfile(profile_) &&
      !arc::IsArcAppSyncFlowDisabled()) {
    controllers.push_back(std::make_unique<ArcPackageSyncModelTypeController>(
        model_type_store_factory,
        GetSyncableServiceForType(syncer::ARC_PACKAGE), dump_stack,
        sync_service, profile_));
  }
  controllers.push_back(
      std::make_unique<syncer::SyncableServiceBasedModelTypeController>(
          syncer::OS_PREFERENCES, model_type_store_factory,
          GetSyncableServiceForType(syncer::OS_PREFERENCES), dump_stack,
          syncer::SyncableServiceBasedModelTypeController::DelegateMode::
              kTransportModeWithSingleModel));
  controllers.push_back(
      std::make_unique<syncer::SyncableServiceBasedModelTypeController>(
          syncer::OS_PRIORITY_PREFERENCES, model_type_store_factory,
          GetSyncableServiceForType(syncer::OS_PRIORITY_PREFERENCES),
          dump_stack,
          syncer::SyncableServiceBasedModelTypeController::DelegateMode::
              kTransportModeWithSingleModel));

  syncer::ModelTypeControllerDelegate* printers_delegate =
      ash::SyncedPrintersManagerFactory::GetForBrowserContext(profile_)
          ->GetSyncBridge()
          ->change_processor()
          ->GetControllerDelegate()
          .get();
  controllers.push_back(std::make_unique<syncer::ModelTypeController>(
      syncer::PRINTERS,
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
          printers_delegate),
      /*delegate_for_transport_mode=*/nullptr));

  if (WifiConfigurationSyncServiceFactory::ShouldRunInProfile(profile_)) {
    syncer::ModelTypeControllerDelegate* wifi_configurations_delegate =
        WifiConfigurationSyncServiceFactory::GetForProfile(profile_,
                                                           /*create=*/true)
            ->GetControllerDelegate()
            .get();
    controllers.push_back(std::make_unique<syncer::ModelTypeController>(
        syncer::WIFI_CONFIGURATIONS,
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            wifi_configurations_delegate),
        /*delegate_for_transport_mode=*/nullptr));
  }

  syncer::ModelTypeControllerDelegate* workspace_desk_delegate =
      DeskSyncServiceFactory::GetForProfile(profile_)
          ->GetControllerDelegate()
          .get();
  controllers.push_back(std::make_unique<syncer::ModelTypeController>(
      syncer::WORKSPACE_DESK,
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
          workspace_desk_delegate),
      /*delegate_for_transport_mode=*/nullptr));

  if (ash::features::IsOAuthIppEnabled()) {
    syncer::ModelTypeControllerDelegate*
        printers_authorization_servers_delegate =
            ash::printing::oauth2::AuthorizationZonesManagerFactory::
                GetForBrowserContext(profile_)
                    ->GetModelTypeSyncBridge()
                    ->change_processor()
                    ->GetControllerDelegate()
                    .get();
    controllers.push_back(std::make_unique<syncer::ModelTypeController>(
        syncer::PRINTERS_AUTHORIZATION_SERVERS,
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            printers_authorization_servers_delegate),
        /*delegate_for_transport_mode=*/nullptr));
  }

  if (ash::features::IsFloatingSsoAllowed()) {
    syncer::ModelTypeControllerDelegate* cookies_delegate =
        ash::floating_sso::FloatingSsoServiceFactory::GetForProfile(profile_)
            ->GetControllerDelegate()
            .get();
    controllers.push_back(std::make_unique<syncer::ModelTypeController>(
        syncer::COOKIES,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            cookies_delegate),
        /*delegate_for_transport_mode=*/nullptr));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return controllers;
}

trusted_vault::TrustedVaultClient* ChromeSyncClient::GetTrustedVaultClient() {
  return TrustedVaultServiceFactory::GetForProfile(profile_)
      ->GetTrustedVaultClient(trusted_vault::SecurityDomainId::kChromeSync);
}

syncer::SyncInvalidationsService*
ChromeSyncClient::GetSyncInvalidationsService() {
  return SyncInvalidationsServiceFactory::GetForProfile(profile_);
}

scoped_refptr<syncer::ExtensionsActivity>
ChromeSyncClient::GetExtensionsActivity() {
  return extensions_activity_monitor_.GetExtensionsActivity();
}

syncer::ModelTypeStoreService* ChromeSyncClient::GetModelTypeStoreService() {
  return ModelTypeStoreServiceFactory::GetForProfile(profile_);
}

base::WeakPtr<syncer::SyncableService>
ChromeSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  switch (type) {
    case syncer::SEARCH_ENGINES:
      return GetWeakPtrOrNull(
          TemplateURLServiceFactory::GetForProfile(profile_));
#if BUILDFLAG(ENABLE_EXTENSIONS)
    case syncer::APPS:
    case syncer::EXTENSIONS:
      return GetWeakPtrOrNull(ExtensionSyncService::Get(profile_));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case syncer::APP_LIST:
      return GetWeakPtrOrNull(
          app_list::AppListSyncableServiceFactory::GetForProfile(profile_));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if !BUILDFLAG(IS_ANDROID)
    case syncer::THEMES:
      return ThemeServiceFactory::GetForProfile(profile_)
          ->GetThemeSyncableService()
          ->AsWeakPtr();
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_SPELLCHECK)
    case syncer::DICTIONARY: {
      SpellcheckService* spellcheck_service =
          SpellcheckServiceFactory::GetForContext(profile_);
      return spellcheck_service
                 ? spellcheck_service->GetCustomDictionary()->AsWeakPtr()
                 : nullptr;
    }
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case syncer::ARC_PACKAGE:
      return arc::ArcPackageSyncableService::Get(profile_)->AsWeakPtr();
    case syncer::OS_PREFERENCES:
    case syncer::OS_PRIORITY_PREFERENCES:
      return PrefServiceSyncableFromProfile(profile_)
          ->GetSyncableService(type)
          ->AsWeakPtr();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

syncer::SyncApiComponentFactory*
ChromeSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
}

bool ChromeSyncClient::IsCustomPassphraseAllowed() {
  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              profile_->GetProfileKey());
  if (supervised_user_settings_service) {
    return supervised_user_settings_service->IsCustomPassphraseAllowed();
  }
  return true;
}

bool ChromeSyncClient::IsPasswordSyncAllowed() {
#if BUILDFLAG(IS_ANDROID)
  return profile_->GetPrefs()->GetInteger(
             password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores) !=
         static_cast<int>(
             password_manager::prefs::UseUpmLocalAndSeparateStoresState::
                 kOffAndMigrationPending);
#else
  return true;
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeSyncClient::SetPasswordSyncAllowedChangeCb(
    const base::RepeatingClosure& cb) {
#if BUILDFLAG(IS_ANDROID)
  CHECK(!upm_pref_change_registrar_.prefs())
      << "SetPasswordSyncAllowedChangeCb() must be called at most once";
  upm_pref_change_registrar_.Init(profile_->GetPrefs());
  // This overfires: the kPasswordsUseUPMLocalAndSeparateStores pref might have
  // changed value, but not IsPasswordSyncAllowed(). That's fine, `cb` should
  // handle this case.
  upm_pref_change_registrar_.Add(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores, cb);
#else
  // IsPasswordSyncAllowed() doesn't change outside of Android.
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeSyncClient::RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
    const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group) {
  CHECK(group.is_valid());

  if (!base::FeatureList::IsEnabled(
          syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrial)) {
    // Disabled via variations, as additional safeguard.
    return;
  }

  // If `trusted_vault_synthetic_field_trial_registered` is true, and given that
  // each SyncService invokes this function at most once, it means that multiple
  // profiles are trying to register a synthetic field trial. In that case,
  // register a special "conflict" group.
  const std::string group_name =
      trusted_vault_synthetic_field_trial_registered
          ? syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::
                GetMultiProfileConflictGroupName()
          : group.name();

  trusted_vault_synthetic_field_trial_registered = true;

  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
std::unique_ptr<syncer::ModelTypeController>
ChromeSyncClient::CreateAppsModelTypeController() {
  auto delegate_mode =
      ExtensionModelTypeController::DelegateMode::kLegacyFullSyncModeOnly;
  if (ShouldSyncAppsTypesInTransportMode()) {
    delegate_mode = ExtensionModelTypeController::DelegateMode::
        kTransportModeWithSingleModel;
  }
  return std::make_unique<ExtensionModelTypeController>(
      syncer::APPS, GetModelTypeStoreService()->GetStoreFactory(),
      GetSyncableServiceForType(syncer::APPS), GetDumpStackClosure(),
      delegate_mode, profile_);
}

std::unique_ptr<syncer::ModelTypeController>
ChromeSyncClient::CreateAppSettingsModelTypeController(
    syncer::SyncService* sync_service) {
  auto delegate_mode = ExtensionSettingModelTypeController::DelegateMode::
      kLegacyFullSyncModeOnly;
  if (ShouldSyncAppsTypesInTransportMode()) {
    delegate_mode = ExtensionSettingModelTypeController::DelegateMode::
        kTransportModeWithSingleModel;
  }
  return std::make_unique<ExtensionSettingModelTypeController>(
      syncer::APP_SETTINGS, GetModelTypeStoreService()->GetStoreFactory(),
      extensions::settings_sync_util::GetSyncableServiceProvider(
          profile_, syncer::APP_SETTINGS),
      GetDumpStackClosure(), delegate_mode, profile_);
}

std::unique_ptr<syncer::ModelTypeController>
ChromeSyncClient::CreateWebAppsModelTypeController() {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);

  // This function should never be called when GetForWebApps() returns nullptr.
  DCHECK(provider);
  DCHECK(web_app::AreWebAppsEnabled(profile_));

  syncer::ModelTypeControllerDelegate* delegate = provider->sync_bridge_unsafe()
                                                      .change_processor()
                                                      ->GetControllerDelegate()
                                                      .get();

  std::unique_ptr<syncer::ModelTypeControllerDelegate>
      delegate_for_transport_mode = nullptr;
  if (ShouldSyncAppsTypesInTransportMode()) {
    delegate_for_transport_mode =
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate);
  }
  return std::make_unique<syncer::ModelTypeController>(
      syncer::WEB_APPS,
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      std::move(delegate_for_transport_mode));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace browser_sync
