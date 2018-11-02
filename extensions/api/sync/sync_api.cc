// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/api/sync/sync_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync/base/model_type.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/sync.h"
#include "sync/vivaldi_syncmanager.h"
#include "sync/vivaldi_syncmanager_factory.h"

using vivaldi::VivaldiSyncManagerFactory;
using vivaldi::VivaldiSyncManager;

namespace extensions {

SyncEventRouter::SyncEventRouter(Profile* profile)
    : browser_context_(profile),
      manager_(VivaldiSyncManagerFactory::GetForProfileVivaldi(profile)
                   ->AsWeakPtr()) {
  manager_->AddVivaldiObserver(this);
}

SyncEventRouter::~SyncEventRouter() {
  if (manager_) {
    manager_->RemoveVivaldiObserver(this);
  }
}

// Helper to actually dispatch an event to extension listeners.
void SyncEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

void SyncEventRouter::OnAccessTokenRequested() {
  DispatchEvent(vivaldi::sync::OnAccessTokenRequested::kEventName,
                vivaldi::sync::OnAccessTokenRequested::Create());
}

void SyncEventRouter::OnEncryptionPasswordRequested() {
  DispatchEvent(vivaldi::sync::OnEncryptionPasswordRequested::kEventName,
                vivaldi::sync::OnEncryptionPasswordRequested::Create());
}

void SyncEventRouter::OnLoginDone() {
  DispatchEvent(vivaldi::sync::OnLoginDone::kEventName,
                vivaldi::sync::OnLoginDone::Create());
}

void SyncEventRouter::OnSyncEngineInitFailed() {
  DispatchEvent(vivaldi::sync::OnSyncEngineInitFailed::kEventName,
                vivaldi::sync::OnSyncEngineInitFailed::Create());
}

void SyncEventRouter::OnBeginSyncing() {
  DispatchEvent(vivaldi::sync::OnBeginSyncing::kEventName,
                vivaldi::sync::OnBeginSyncing::Create());
}

void SyncEventRouter::OnEndSyncing() {
  DispatchEvent(vivaldi::sync::OnEndSyncing::kEventName,
                vivaldi::sync::OnEndSyncing::Create());
}

void SyncEventRouter::OnLogoutDone() {
  DispatchEvent(vivaldi::sync::OnLogoutDone::kEventName,
                vivaldi::sync::OnLogoutDone::Create());
}

void SyncEventRouter::OnDeletingSyncManager() {
  manager_->RemoveVivaldiObserver(this);
  manager_ = nullptr;
}

SyncAPI::SyncAPI(content::BrowserContext* context) : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, vivaldi::sync::OnLogoutDone::kEventName);
  event_router->RegisterObserver(this, vivaldi::sync::OnLoginDone::kEventName);
  event_router->RegisterObserver(this,
                                 vivaldi::sync::OnBeginSyncing::kEventName);
  event_router->RegisterObserver(this, vivaldi::sync::OnEndSyncing::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::sync::OnSyncEngineInitFailed::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::sync::OnAccessTokenRequested::kEventName);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<SyncAPI> >::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<SyncAPI>* SyncAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

SyncAPI::~SyncAPI() {}

void SyncAPI::OnListenerAdded(const EventListenerInfo& details) {
  sync_event_router_.reset(
      new SyncEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

// KeyedService implementation.
void SyncAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

bool SyncLoginCompleteFunction::RunAsync() {
  std::unique_ptr<vivaldi::sync::LoginComplete::Params> params(
      vivaldi::sync::LoginComplete::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  sync_manager->SetToken(true, params->login_details.username,
                         params->login_details.password, params->token.token,
                         params->token.expire, params->token.account_id);

  SendResponse(true);
  return true;
}

bool SyncRefreshTokenFunction::RunAsync() {
  std::unique_ptr<vivaldi::sync::RefreshToken::Params> params(
      vivaldi::sync::RefreshToken::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  sync_manager->SetToken(false, std::string(), std::string(),
                         params->token.token, params->token.expire,
                         params->token.account_id);

  SendResponse(true);
  return true;
}

bool SyncIsFirstSetupFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  results_ = vivaldi::sync::IsFirstSetup::Results::Create(
      !sync_manager->IsFirstSetupComplete());

  SendResponse(true);
  return true;
}

bool SyncIsEncryptionPasswordSetUpFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  results_ = vivaldi::sync::IsFirstSetup::Results::Create(
      sync_manager->IsUsingSecondaryPassphrase());

  SendResponse(true);
  return true;
}

bool SyncSetEncryptionPasswordFunction::RunAsync() {
  std::unique_ptr<vivaldi::sync::SetEncryptionPassword::Params> params(
      vivaldi::sync::SetEncryptionPassword::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  bool success = sync_manager->SetEncryptionPassword(params->password);

  if (!success)
    results_ = vivaldi::sync::SetEncryptionPassword::Results::Create(
        success, sync_manager->IsPassphraseRequired());
  else
    results_ =
        vivaldi::sync::SetEncryptionPassword::Results::Create(success, false);

  SendResponse(success);
  return true;
}

bool SyncSetTypesFunction::RunAsync() {
  std::unique_ptr<vivaldi::sync::SetTypes::Params> params(
      vivaldi::sync::SetTypes::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  syncer::ModelTypeSet chosen_types;
  for (const auto& type : params->types) {
    if (type.enabled)
      chosen_types.Put(syncer::ModelTypeFromString(type.name));
  }

  sync_manager->ConfigureTypes(params->sync_everything, chosen_types);

  SendResponse(true);
  return true;
}

bool SyncGetTypesFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  PrefService* pref_service = GetProfile()->GetPrefs();
  syncer::SyncPrefs sync_prefs(pref_service);

  syncer::ModelTypeSet chosen_types = sync_manager->GetPreferredDataTypes();
  syncer::ModelTypeNameMap model_type_map =
      syncer::GetUserSelectableTypeNameMap();
  std::vector<vivaldi::sync::SyncDataType> data_types;
  for (const auto& model_type : model_type_map) {
    // Skip the model types that don't make sense for us to synchronize.
    if (model_type.first == syncer::THEMES)
      continue;
    vivaldi::sync::SyncDataType data_type;
    data_type.name.assign(syncer::ModelTypeToString(model_type.first));
    data_type.enabled = chosen_types.Has(model_type.first);
    data_types.push_back(std::move(data_type));
  }

  results_ = vivaldi::sync::GetTypes::Results::Create(
      sync_prefs.HasKeepEverythingSynced(), std::move(data_types));

  SendResponse(true);
  return true;
}

bool SyncGetStatusFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  syncer::SyncCycleSnapshot cycle_snapshot =
      sync_manager->GetLastCycleSnapshot();

  extensions::vivaldi::sync::SyncStatusInfo status_info;
  status_info.is_encrypting_everything =
      sync_manager->IsEncryptEverythingEnabled();
  status_info.last_sync_time = base::UTF16ToUTF8(
      base::TimeFormatShortDateAndTime(cycle_snapshot.sync_start_time()));
  status_info.last_commit_success =
      cycle_snapshot.model_neutral_state().commit_result == syncer::SYNCER_OK;
  status_info.last_download_updates_success =
      cycle_snapshot.model_neutral_state().last_download_updates_result ==
      syncer::SYNCER_OK;
  status_info.has_synced = cycle_snapshot.is_initialized();

  results_ = vivaldi::sync::GetStatus::Results::Create(status_info);

  SendResponse(true);
  return true;
}

bool SyncClearDataFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  sync_manager->ClearSyncData();

  SendResponse(true);
  return true;
}

bool SyncSetupCompleteFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  sync_manager->SetupComplete();

  SendResponse(true);
  return true;
}

bool SyncLogoutFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  sync_manager->Logout();

  SendResponse(true);
  return true;
}

bool SyncPollServerFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  sync_manager->PollServer();

  SendResponse(true);
  return true;
}

}  // namespace extensions
