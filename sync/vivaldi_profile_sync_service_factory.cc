// Copyright (c) 2015-2020 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_profile_sync_service_factory.h"

#include <memory>
#include <string>

#include "app/vivaldi_apptools.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"
#include "prefs/vivaldi_pref_names.h"
#include "sync/note_sync_service_factory.h"
#include "sync/vivaldi_profile_sync_service.h"
#include "sync/vivaldi_sync_client.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

namespace vivaldi {

namespace {

void UpdateNetworkTimeOnUIThread(base::Time network_time,
                                 base::TimeDelta resolution,
                                 base::TimeDelta latency,
                                 base::TimeTicks post_time) {
  g_browser_process->network_time_tracker()->UpdateNetworkTime(
      network_time, resolution, latency, post_time);
}

void UpdateNetworkTime(const base::Time& network_time,
                       const base::TimeDelta& resolution,
                       const base::TimeDelta& latency) {
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::Bind(&UpdateNetworkTimeOnUIThread, network_time,
                            resolution, latency, base::TimeTicks::Now()));
}

}  // anonymous namespace

// static
VivaldiProfileSyncServiceFactory*
VivaldiProfileSyncServiceFactory::GetInstance() {
  return base::Singleton<VivaldiProfileSyncServiceFactory>::get();
}

// static
ProfileSyncService* VivaldiProfileSyncServiceFactory::GetForProfile(
    Profile* profile) {
  return GetForProfileVivaldi(profile);
}

// static
VivaldiProfileSyncService*
VivaldiProfileSyncServiceFactory::GetForProfileVivaldi(Profile* profile) {
  if (!switches::IsSyncAllowedByFlag()) {
    return nullptr;
  }

  return static_cast<VivaldiProfileSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
bool VivaldiProfileSyncServiceFactory::HasProfileSyncService(Profile* profile) {
  return GetInstance()->GetServiceForBrowserContext(profile, false) != NULL;
}

VivaldiProfileSyncServiceFactory::VivaldiProfileSyncServiceFactory()
    : ProfileSyncServiceFactory() {
  // The VivaldiProfileSyncService depends on various SyncableServices being
  // around when it is shut down.  Specify those dependencies here to build the
  // proper destruction order.
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(BookmarkModelFactory::GetInstance());
#if !defined(OS_ANDROID)
  DependsOn(GlobalErrorServiceFactory::GetInstance());
#endif
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
#if !defined(OS_ANDROID)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#endif
  DependsOn(NoteSyncServiceFactory::GetInstance());
  DependsOn(VivaldiAccountManagerFactory::GetInstance());
}

VivaldiProfileSyncServiceFactory::~VivaldiProfileSyncServiceFactory() {}

KeyedService* VivaldiProfileSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  ProfileSyncService::InitParams init_params;
  init_params.sync_client = std::make_unique<VivaldiSyncClient>(profile);
  init_params.network_time_update_callback = base::Bind(&UpdateNetworkTime);
  init_params.url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();
  init_params.network_connection_tracker =
      content::GetNetworkConnectionTracker();
  init_params.channel = chrome::GetChannel();
  init_params.debug_identifier = profile->GetDebugName();
  init_params.policy_service =
      profile->GetProfilePolicyConnector()->policy_service();
  init_params.identity_manager = IdentityManagerFactory::GetForProfile(profile);
  init_params.start_behavior = ProfileSyncService::MANUAL_START;

  PrefService* local_state = g_browser_process->local_state();
  if (local_state)
    init_params.sync_server_url =
        GURL(local_state->GetString(vivaldiprefs::kVivaldiSyncServerUrl));

  auto vpss = std::make_unique<VivaldiProfileSyncService>(
      &init_params, profile,
      VivaldiAccountManagerFactory::GetForProfile(profile));

  vpss->Initialize();

  // Hook PSS into PersonalDataManager (a circular dependency).
  autofill::PersonalDataManager* pdm =
      autofill::PersonalDataManagerFactory::GetForProfile(profile);
  pdm->OnSyncServiceInitialized(vpss.get());

  return vpss.release();
}

}  // namespace vivaldi
