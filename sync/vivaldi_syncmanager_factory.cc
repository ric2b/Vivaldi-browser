// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_syncmanager_factory.h"

#include <string>
#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/driver/signin_manager_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "notes/notes_factory.h"
#include "sync/vivaldi_sync_client.h"
#include "sync/vivaldi_syncmanager.h"

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
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&UpdateNetworkTimeOnUIThread, network_time, resolution,
                 latency, base::TimeTicks::Now()));
}

}  // anonymous namespace

// static
VivaldiSyncManagerFactory* VivaldiSyncManagerFactory::GetInstance() {
  return base::Singleton<VivaldiSyncManagerFactory>::get();
}

// static
ProfileSyncService* VivaldiSyncManagerFactory::GetForProfile(Profile* profile) {
  return GetForProfileVivaldi(profile);
}

// static
VivaldiSyncManager* VivaldiSyncManagerFactory::GetForProfileVivaldi(
    Profile* profile) {
  if (!VivaldiSyncManager::IsSyncEnabled())
    return NULL;

  return static_cast<VivaldiSyncManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
bool VivaldiSyncManagerFactory::HasProfileSyncService(Profile* profile) {
  return GetInstance()->GetServiceForBrowserContext(profile, false) != NULL;
}

VivaldiSyncManagerFactory::VivaldiSyncManagerFactory()
    : ProfileSyncServiceFactory() {
  // The VivaldiSyncManager depends on various SyncableServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order.
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(BookmarkModelFactory::GetInstance());
#if !defined(OS_ANDROID)
  DependsOn(GlobalErrorServiceFactory::GetInstance());
#endif
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
#if !defined(OS_ANDROID)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#endif
  DependsOn(NotesModelFactory::GetInstance());
}

VivaldiSyncManagerFactory::~VivaldiSyncManagerFactory() {}

KeyedService* VivaldiSyncManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  SigninManager* signin =
      SigninManagerFactory::GetForProfile(profile);

  ProfileSyncService::InitParams init_params;

  init_params.signin_wrapper =
      std::make_unique<SigninManagerWrapper>(
          IdentityManagerFactory::GetForProfile(profile), signin);
  init_params.url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();
#if defined(OS_WIN)
  init_params.signin_scoped_device_id_callback =
    base::BindRepeating([]() { return std::string("local_device"); });
#else
  // Note: base::Unretained(signin_client) is safe because the SigninClient is
  // guaranteed to outlive the PSS, per a DependsOn() above (and because PSS
  // clears the callback in its Shutdown()).
  init_params.signin_scoped_device_id_callback = base::BindRepeating(
    &SigninClient::GetSigninScopedDeviceId,
    base::Unretained(ChromeSigninClientFactory::GetForProfile(profile)));
#endif

  init_params.start_behavior = ProfileSyncService::MANUAL_START;

  VivaldiSyncClient* sync_client = new VivaldiSyncClient(profile);
  init_params.sync_client = base::WrapUnique(sync_client);

  init_params.network_time_update_callback = base::Bind(&UpdateNetworkTime);
  init_params.url_request_context = profile->GetRequestContext();
  init_params.debug_identifier = profile->GetDebugName();
  init_params.channel = chrome::GetChannel();

  auto vss = base::WrapUnique(new VivaldiSyncManager(
      &init_params, sync_client->GetVivaldiInvalidationService()));

  vss->Initialize();

  return vss.release();
}

}  // namespace vivaldi
