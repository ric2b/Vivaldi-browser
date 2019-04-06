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
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/base/model_type.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/sync.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/types.pb.h"
#include "sync/vivaldi_invalidation_service.h"
#include "sync/vivaldi_syncmanager.h"
#include "sync/vivaldi_syncmanager_factory.h"

using vivaldi::VivaldiSyncManager;
using vivaldi::VivaldiSyncManagerFactory;

namespace extensions {

namespace {
vivaldi::sync::SyncOperationResult SyncerErrorToOperationResult(
    syncer::SyncerError syncer_error) {
  switch (syncer_error) {
    case syncer::SYNCER_OK:
      return vivaldi::sync::SyncOperationResult::SYNC_OPERATION_RESULT_SUCCESS;
    case syncer::UNSET:
    case syncer::SERVER_MORE_TO_DOWNLOAD:
    case syncer::DATATYPE_TRIGGERED_RETRY:
      return vivaldi::sync::SyncOperationResult::SYNC_OPERATION_RESULT_PENDING;
    default:
      return vivaldi::sync::SyncOperationResult::SYNC_OPERATION_RESULT_FAILURE;
  }
}
}  // anonymous namespace

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

void SyncEventRouter::OnEngineStarted() {
  DispatchEvent(vivaldi::sync::OnEngineStarted::kEventName,
                vivaldi::sync::OnEngineStarted::Create());
}

void SyncEventRouter::OnEngineInitFailed() {
  DispatchEvent(vivaldi::sync::OnEngineInitFailed::kEventName,
                vivaldi::sync::OnEngineInitFailed::Create());
}

void SyncEventRouter::OnBeginSyncing() {
  DispatchEvent(vivaldi::sync::OnBeginSyncing::kEventName,
                vivaldi::sync::OnBeginSyncing::Create());
}

void SyncEventRouter::OnEndSyncing() {
  DispatchEvent(vivaldi::sync::OnEndSyncing::kEventName,
                vivaldi::sync::OnEndSyncing::Create());
}

void SyncEventRouter::OnEngineStopped() {
  DispatchEvent(vivaldi::sync::OnEngineStopped::kEventName,
                vivaldi::sync::OnEngineStopped::Create());
}

void SyncEventRouter::OnDeletingSyncManager() {
  manager_->RemoveVivaldiObserver(this);
  manager_ = nullptr;
}

SyncAPI::SyncAPI(content::BrowserContext* context) : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 vivaldi::sync::OnEngineStarted::kEventName);
  event_router->RegisterObserver(this,
                                 vivaldi::sync::OnEngineStopped::kEventName);
  event_router->RegisterObserver(this,
                                 vivaldi::sync::OnBeginSyncing::kEventName);
  event_router->RegisterObserver(this, vivaldi::sync::OnEndSyncing::kEventName);
  event_router->RegisterObserver(this,
                                 vivaldi::sync::OnEngineInitFailed::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::sync::OnAccessTokenRequested::kEventName);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<SyncAPI>>::DestructorAtExit g_factory_sync =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<SyncAPI>* SyncAPI::GetFactoryInstance() {
  return g_factory_sync.Pointer();
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

bool SyncStartFunction::RunAsync() {
  std::unique_ptr<vivaldi::sync::Start::Params> params(
      vivaldi::sync::Start::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  sync_manager->SetToken(true, params->token.account_id, params->token.token);

  SendResponse(true);
  return true;
}

bool SyncRefreshTokenFunction::RunAsync() {
  std::unique_ptr<vivaldi::sync::RefreshToken::Params> params(
      vivaldi::sync::RefreshToken::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  if (!sync_manager) {
    error_ = "Sync manager is unavailable";
    return false;
  }

  sync_manager->SetToken(false, params->token.account_id, params->token.token);

  SendResponse(true);
  return true;
}

bool SyncIsFirstSetupFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  if (!sync_manager) {
    error_ = "Sync manager is unavailable";
    return false;
  }

  results_ = vivaldi::sync::IsFirstSetup::Results::Create(
      !sync_manager->IsFirstSetupComplete());

  SendResponse(true);
  return true;
}

bool SyncIsEncryptionPasswordSetUpFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  if (!sync_manager) {
    error_ = "Sync manager is unavailable";
    return false;
  }

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
  if (!sync_manager) {
    error_ = "Sync manager is unavailable";
    return false;
  }

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
  if (!sync_manager) {
    error_ = "Sync manager is unavailable";
    return false;
  }

  syncer::ModelTypeSet chosen_types;
  for (const auto& type : params->types) {
    if (type.enabled)
      chosen_types.Put(syncer::ModelTypeFromString(type.name));
  }
  if (chosen_types.Has(syncer::TYPED_URLS))
    chosen_types.Put(syncer::PROXY_TABS);

  sync_manager->ConfigureTypes(params->sync_everything, chosen_types);

  SendResponse(true);
  return true;
}

bool SyncGetTypesFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  if (!sync_manager) {
    error_ = "Sync manager is unavailable";
    return false;
  }

  PrefService* pref_service = GetProfile()->GetPrefs();
  syncer::SyncPrefs sync_prefs(pref_service);

  syncer::ModelTypeSet chosen_types = sync_manager->GetPreferredDataTypes();
  syncer::ModelTypeNameMap model_type_map =
      syncer::GetUserSelectableTypeNameMap();
  std::vector<vivaldi::sync::SyncDataType> data_types;
  for (const auto& model_type : model_type_map) {
    // Skip the model types that don't make sense for us to synchronize.
    if (model_type.first == syncer::THEMES ||
        model_type.first == syncer::APPS ||
        model_type.first == syncer::USER_EVENTS ||
        model_type.first == syncer::PROXY_TABS) {
      continue;
    }
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
  if (!sync_manager || !sync_manager->IsEngineInitialized()) {
    error_ = "Sync manager is unavailable";
    return false;
  }

  syncer::SyncCycleSnapshot cycle_snapshot =
      sync_manager->GetLastCycleSnapshot();

  extensions::vivaldi::sync::SyncStatusInfo status_info;
  status_info.is_encrypting_everything =
      sync_manager->IsEncryptEverythingEnabled();
  status_info.last_sync_time = cycle_snapshot.sync_start_time().ToJsTime();
  status_info.last_commit_status = SyncerErrorToOperationResult(
      cycle_snapshot.model_neutral_state().commit_result);
  status_info.last_download_updates_status = SyncerErrorToOperationResult(
      cycle_snapshot.model_neutral_state().last_download_updates_result);
  status_info.has_synced = cycle_snapshot.is_initialized();

  results_ = vivaldi::sync::GetStatus::Results::Create(status_info);

  SendResponse(true);
  return true;
}

bool SyncClearDataFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  if (!sync_manager) {
    error_ = "Sync manager is unavailable";
    return false;
  }

  sync_manager->ClearSyncData();

  SendResponse(true);
  return true;
}

bool SyncSetupCompleteFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  if (!sync_manager) {
    error_ = "Sync manager is unavailable";
    return false;
  }

  sync_manager->SetupComplete();

  SendResponse(true);
  return true;
}

bool SyncLogoutFunction::RunAsync() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  if (!sync_manager) {
    error_ = "Sync manager is unavailable";
    return false;
  }

  sync_manager->Logout();

  SendResponse(true);
  return true;
}

bool SyncUpdateNotificationClientStatusFunction::RunAsync() {
  std::unique_ptr<vivaldi::sync::UpdateNotificationClientStatus::Params> params(
      vivaldi::sync::UpdateNotificationClientStatus::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  if (!sync_manager) {
    error_ = "Sync manager is unavailable";
    return false;
  }
  syncer::InvalidatorState state =
      (params->status == vivaldi::sync::SyncNotificationClientStatus::
                             SYNC_NOTIFICATION_CLIENT_STATUS_CONNECTED)
          ? syncer::INVALIDATIONS_ENABLED
          : syncer::DEFAULT_INVALIDATION_ERROR;
  sync_manager->invalidation_service()->UpdateInvalidatorState(state);

  // If notifications just got re-enabled, just invalidate everything to
  // compensate for potentially missed notifications.
  if (state == syncer::INVALIDATIONS_ENABLED) {
    syncer::ObjectIdInvalidationMap invalidation_map =
        syncer::ObjectIdInvalidationMap::InvalidateAll(
            syncer::ModelTypeSetToObjectIdSet(syncer::ProtocolTypes()));
    sync_manager->invalidation_service()->PerformInvalidation(invalidation_map);
  }
  SendResponse(true);
  return true;
}

bool SyncNotificationReceivedFunction::RunAsync() {
  std::unique_ptr<vivaldi::sync::NotificationReceived::Params> params(
      vivaldi::sync::NotificationReceived::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(GetProfile());
  DCHECK(sync_manager);

  // Ignore notifications for changes sent by us.
  if (params->client_id !=
      sync_manager->invalidation_service()->GetInvalidatorClientId()) {
    int64_t version;
    base::StringToInt64(params->version, &version);
    syncer::ObjectIdInvalidationMap invalidations;
    invalidations.Insert(syncer::Invalidation::Init(
        invalidation::ObjectId(ipc::invalidation::ObjectSource::CHROME_SYNC,
                               params->notification_type),
        version, ""));
    sync_manager->invalidation_service()->PerformInvalidation(invalidations);
  }

  SendResponse(true);
  return true;
}

}  // namespace extensions
