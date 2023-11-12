// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/sync/vivaldi_sync_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/network_time/network_time_tracker.h"
#import "components/policy/core/common/policy_map.h"
#import "components/prefs/pref_service.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_impl.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/consent_auditor/consent_auditor_factory.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/ios_chrome_sync_client.h"
#import "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#import "ios/chrome/browser/sync/model_type_store_service_factory.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_invalidations_service_factory.h"
#import "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/notes/notes_factory.h"
#import "ios/vivaldi_account/vivaldi_account_manager_factory.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "prefs/vivaldi_pref_names.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "sync/vivaldi_sync_service_impl.h"
#import "url/gurl.h"

namespace vivaldi {
// static
VivaldiSyncServiceFactory* VivaldiSyncServiceFactory::GetInstance() {
  static base::NoDestructor<VivaldiSyncServiceFactory> instance;
  return instance.get();
}

VivaldiSyncServiceFactory::VivaldiSyncServiceFactory() {
  // The SyncService depends on various SyncableServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order.
  DependsOn(ConsentAuditorFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(ios::LocalOrSyncableBookmarkModelFactory::GetInstance());
  DependsOn(ios::LocalOrSyncableBookmarkSyncServiceFactory::GetInstance());
  DependsOn(ios::BookmarkUndoServiceFactory::GetInstance());
  DependsOn(ios::FaviconServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(ios::WebDataServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(IOSChromePasswordStoreFactory::GetInstance());
  DependsOn(IOSUserEventServiceFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(SyncInvalidationsServiceFactory::GetInstance());
  DependsOn(NotesModelFactory::GetInstance());
  DependsOn(VivaldiAccountManagerFactory::GetInstance());
}

VivaldiSyncServiceFactory::~VivaldiSyncServiceFactory() {}

// static
VivaldiSyncServiceImpl* VivaldiSyncServiceFactory::GetForBrowserStateVivaldi(
    ChromeBrowserState* browser_state) {
  if (!syncer::IsSyncAllowedByFlag()) {
    return nullptr;
  }

  return static_cast<VivaldiSyncServiceImpl*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

std::unique_ptr<KeyedService>
VivaldiSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  syncer::SyncServiceImpl::InitParams init_params;
  init_params.identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  init_params.sync_client =
      std::make_unique<IOSChromeSyncClient>(browser_state);
  init_params.url_loader_factory = browser_state->GetSharedURLLoaderFactory();
  init_params.network_connection_tracker =
      GetApplicationContext()->GetNetworkConnectionTracker();
  init_params.channel = ::GetChannel();
  init_params.debug_identifier = browser_state->GetDebugName();

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  if (local_state)
    init_params.sync_server_url =
        GURL(local_state->GetString(vivaldiprefs::kVivaldiSyncServerUrl));

  auto* vivaldi_account_manager =
      VivaldiAccountManagerFactory::GetForBrowserState(browser_state);

  auto sync_service = std::make_unique<VivaldiSyncServiceImpl>(
      std::move(init_params), browser_state->GetPrefs(),
      vivaldi_account_manager);
  sync_service->Initialize();

  return sync_service;
}

}  // namespace vivaldi
