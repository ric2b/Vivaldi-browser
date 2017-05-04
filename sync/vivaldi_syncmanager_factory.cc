// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_syncmanager.h"
#include "sync/vivaldi_syncmanager_factory.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/channel_info.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_time/network_time_tracker.h"

#include "content/public/browser/browser_thread.h"

#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

#include "notes/notes_factory.h"
#include "sync/vivaldi_profile_oauth2_token_service.h"
#include "sync/vivaldi_profile_oauth2_token_service_factory.h"
#include "sync/vivaldi_signin_manager.h"
#include "sync/vivaldi_signin_manager_factory.h"
#include "sync/vivaldi_sync_client.h"

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
  DependsOn(GlobalErrorServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(VivaldiSigninManagerFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(NotesModelFactory::GetInstance());
}

VivaldiSyncManagerFactory::~VivaldiSyncManagerFactory() {}

KeyedService* VivaldiSyncManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  VivaldiSigninManager* signin =
      VivaldiSigninManagerFactory::GetForProfile(profile);

  ProfileSyncService::InitParams init_params;

  init_params.signin_wrapper =
      base::WrapUnique(new SupervisedUserSigninManagerWrapper(profile, signin));
  init_params.oauth2_token_service =
      VivaldiProfileOAuth2TokenServiceFactory::GetForProfile(profile);

  init_params.start_behavior = ProfileSyncService::MANUAL_START;

  VivaldiSyncClient* sync_client = new VivaldiSyncClient(profile);
  init_params.sync_client = base::WrapUnique(sync_client);

  init_params.network_time_update_callback = base::Bind(&UpdateNetworkTime);
  init_params.base_directory = profile->GetPath();
  init_params.url_request_context = profile->GetRequestContext();
  init_params.debug_identifier = profile->GetDebugName();
  init_params.channel = chrome::GetChannel();

  base::SequencedWorkerPool* blocking_pool =
      content::BrowserThread::GetBlockingPool();
  init_params.blocking_task_runner =
      blocking_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          blocking_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);

  auto vss = base::WrapUnique(new VivaldiSyncManager(
      init_params, sync_client->GetVivaldiInvalidationService()));

  vss->Initialize();

  return vss.release();
}

}  // namespace vivaldi
