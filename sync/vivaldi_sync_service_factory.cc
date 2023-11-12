// Copyright (c) 2015-2020 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_service_factory.h"

#include <memory>
#include <string>

#include "app/vivaldi_apptools.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/security_events/security_event_recorder_factory.h"
#include "chrome/browser/sharing/sharing_message_bridge_factory.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/sync/bookmark_sync_service_factory.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync/base/command_line_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"
#include "prefs/vivaldi_pref_names.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "sync/note_sync_service_factory.h"
#include "sync/vivaldi_sync_service_impl.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/passkey_model_factory.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace vivaldi {

// static
VivaldiSyncServiceFactory* VivaldiSyncServiceFactory::GetInstance() {
  static base::NoDestructor<VivaldiSyncServiceFactory> instance;
  return instance.get();
}

// static
SyncService* VivaldiSyncServiceFactory::GetForProfile(Profile* profile) {
  return GetForProfileVivaldi(profile);
}

// static
VivaldiSyncServiceImpl* VivaldiSyncServiceFactory::GetForProfileVivaldi(
    Profile* profile) {
  if (!syncer::IsSyncAllowedByFlag()) {
    return nullptr;
  }

  return static_cast<VivaldiSyncServiceImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
bool VivaldiSyncServiceFactory::HasSyncService(Profile* profile) {
  return GetInstance()->GetServiceForBrowserContext(profile, false) != NULL;
}

VivaldiSyncServiceFactory::VivaldiSyncServiceFactory() : SyncServiceFactory() {
  // The VivaldiSyncService depends on various SyncableServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order.
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(BookmarkSyncServiceFactory::GetInstance());
  DependsOn(BookmarkUndoServiceFactory::GetInstance());
  DependsOn(browser_sync::UserEventServiceFactory::GetInstance());
  DependsOn(ConsentAuditorFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(FaviconServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(PasskeyModelFactory::GetInstance());
#endif  // !BUILDFLAG(IS_ANDROID)
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(PowerBookmarkServiceFactory::GetInstance());
  DependsOn(SecurityEventRecorderFactory::GetInstance());
  DependsOn(SendTabToSelfSyncServiceFactory::GetInstance());
  DependsOn(SharingMessageBridgeFactory::GetInstance());
  DependsOn(SpellcheckServiceFactory::GetInstance());
  DependsOn(SyncInvalidationsServiceFactory::GetInstance());
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(ThemeServiceFactory::GetInstance());
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  DependsOn(SavedTabGroupServiceFactory::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
  DependsOn(WebDataServiceFactory::GetInstance());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(extensions::StorageFrontend::GetFactoryInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(NoteSyncServiceFactory::GetInstance());
  DependsOn(VivaldiAccountManagerFactory::GetInstance());
}

VivaldiSyncServiceFactory::~VivaldiSyncServiceFactory() {}

KeyedService* VivaldiSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  syncer::SyncServiceImpl::InitParams init_params;

  Profile* profile = Profile::FromBrowserContext(context);

  DCHECK(!profile->IsOffTheRecord());

  init_params.is_regular_profile_for_uma = profile->IsRegularProfile();
  init_params.sync_client =
      std::make_unique<browser_sync::ChromeSyncClient>(profile);
  init_params.url_loader_factory = profile->GetDefaultStoragePartition()
                                       ->GetURLLoaderFactoryForBrowserProcess();
  init_params.network_connection_tracker =
      content::GetNetworkConnectionTracker();
  init_params.channel = chrome::GetChannel();
  init_params.debug_identifier = profile->GetDebugName();

  bool local_sync_backend_enabled = false;
// Only check the local sync backend pref on the supported platforms of
// Windows, Mac and Linux.
// TODO(crbug.com/1052397): Reassess whether the following block needs to be
// included in lacros-chrome once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  syncer::SyncPrefs prefs(profile->GetPrefs());
  local_sync_backend_enabled = prefs.IsLocalSyncEnabled();
  if (local_sync_backend_enabled) {
    base::FilePath local_sync_backend_folder =
        init_params.sync_client->GetLocalSyncBackendFolder();

    // If the user has not specified a folder and we can't get the default
    // roaming profile location the sync service will not be created.
    if (local_sync_backend_folder.empty()) {
      return nullptr;
    }
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))

  if (!local_sync_backend_enabled) {
    init_params.identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
  }

  PrefService* local_state = g_browser_process->local_state();
  if (local_state)
    init_params.sync_server_url =
        GURL(local_state->GetString(vivaldiprefs::kVivaldiSyncServerUrl));

  auto sync_service = std::make_unique<VivaldiSyncServiceImpl>(
      std::move(init_params), profile->GetPrefs(),
      VivaldiAccountManagerFactory::GetForProfile(profile));

  sync_service->Initialize();

  auto password_store = PasswordStoreFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  // PasswordStoreInterface may be null in tests.
  if (password_store) {
    password_store->OnSyncServiceInitialized(sync_service.get());
  }

  return sync_service.release();
}

}  // namespace vivaldi
