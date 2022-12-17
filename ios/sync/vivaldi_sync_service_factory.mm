// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/sync/vivaldi_sync_service_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/policy/core/common/policy_map.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_impl.h"
#include "ios/chrome/browser/application_context/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_sync_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/policy/browser_state_policy_connector.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#include "ios/chrome/browser/sync/model_type_store_service_factory.h"
#include "ios/chrome/browser/sync/session_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_invalidations_service_factory.h"
#include "ios/chrome/browser/undo/bookmark_undo_service_factory.h"
#include "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/notes/notes_factory.h"
#include "ios/vivaldi_account/vivaldi_account_manager_factory.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/sync/vivaldi_sync_client.h"
#include "prefs/vivaldi_pref_names.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "sync/vivaldi_sync_service_impl.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(ConsentAuditorFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(ios::BookmarkSyncServiceFactory::GetInstance());
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

std::unique_ptr<KeyedService>
VivaldiSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  syncer::SyncServiceImpl::InitParams init_params;
  init_params.identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  init_params.start_behavior = syncer::SyncServiceImpl::MANUAL_START;
  init_params.sync_client =
      std::make_unique<VivaldiSyncClient>(browser_state);
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

  // Hook |sync_service| into PersonalDataManager (a circular dependency).
  autofill::PersonalDataManager* pdm =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browser_state);
  pdm->OnSyncServiceInitialized(sync_service.get());

  return sync_service;
}

}  // namespace vivaldi