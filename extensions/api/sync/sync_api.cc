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
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/sync/base/get_session_name.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/base/model_type.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/sync.h"
#include "extensions/tools/vivaldi_tools.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/types.pb.h"
#include "sync/vivaldi_invalidation_service.h"
#include "sync/vivaldi_syncmanager.h"
#include "sync/vivaldi_syncmanager_factory.h"

using vivaldi::VivaldiSyncManager;
using vivaldi::VivaldiSyncManagerFactory;

namespace extensions {

namespace {

vivaldi::sync::SyncerError ToVivaldiSyncerError(
    syncer::SyncerError syncer_error) {
  switch (syncer_error.value()) {
    case syncer::SyncerError::UNSET:
      return vivaldi::sync::SyncerError::SYNCER_ERROR_UNSET;
    case syncer::SyncerError::CANNOT_DO_WORK:
      return vivaldi::sync::SyncerError::SYNCER_ERROR_CANNOT_DO_WORK;
    case syncer::SyncerError::NETWORK_CONNECTION_UNAVAILABLE:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_NETWORK_CONNECTION_UNAVAILABLE;
    case syncer::SyncerError::NETWORK_IO_ERROR:
      return vivaldi::sync::SyncerError::SYNCER_ERROR_NETWORK_IO_ERROR;
    case syncer::SyncerError::SYNC_SERVER_ERROR:
      return vivaldi::sync::SyncerError::SYNCER_ERROR_SYNC_SERVER_ERROR;
    case syncer::SyncerError::SYNC_AUTH_ERROR:
      return vivaldi::sync::SyncerError::SYNCER_ERROR_SYNC_AUTH_ERROR;
    case syncer::SyncerError::SERVER_RETURN_INVALID_CREDENTIAL:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_SERVER_RETURN_INVALID_CREDENTIAL;
    case syncer::SyncerError::SERVER_RETURN_UNKNOWN_ERROR:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_SERVER_RETURN_UNKNOWN_ERROR;
    case syncer::SyncerError::SERVER_RETURN_THROTTLED:
      return vivaldi::sync::SyncerError::SYNCER_ERROR_SERVER_RETURN_THROTTLED;
    case syncer::SyncerError::SERVER_RETURN_TRANSIENT_ERROR:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_SERVER_RETURN_TRANSIENT_ERROR;
    case syncer::SyncerError::SERVER_RETURN_MIGRATION_DONE:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_SERVER_RETURN_MIGRATION_DONE;
    case syncer::SyncerError::SERVER_RETURN_CLEAR_PENDING:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_SERVER_RETURN_CLEAR_PENDING;
    case syncer::SyncerError::SERVER_RETURN_NOT_MY_BIRTHDAY:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_SERVER_RETURN_NOT_MY_BIRTHDAY;
    case syncer::SyncerError::SERVER_RETURN_CONFLICT:
      return vivaldi::sync::SyncerError::SYNCER_ERROR_SERVER_RETURN_CONFLICT;
    case syncer::SyncerError::SERVER_RESPONSE_VALIDATION_FAILED:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_SERVER_RESPONSE_VALIDATION_FAILED;
    case syncer::SyncerError::SERVER_RETURN_DISABLED_BY_ADMIN:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_SERVER_RETURN_DISABLED_BY_ADMIN;
    case syncer::SyncerError::SERVER_RETURN_USER_ROLLBACK:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_SERVER_RETURN_USER_ROLLBACK;
    case syncer::SyncerError::SERVER_RETURN_PARTIAL_FAILURE:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_SERVER_RETURN_PARTIAL_FAILURE;
    case syncer::SyncerError::SERVER_RETURN_CLIENT_DATA_OBSOLETE:
      return vivaldi::sync::SyncerError::
          SYNCER_ERROR_SERVER_RETURN_CLIENT_DATA_OBSOLETE;
    case syncer::SyncerError::DATATYPE_TRIGGERED_RETRY:
      return vivaldi::sync::SyncerError::SYNCER_ERROR_DATATYPE_TRIGGERED_RETRY;
    case syncer::SyncerError::SERVER_MORE_TO_DOWNLOAD:
      return vivaldi::sync::SyncerError::SYNCER_ERROR_SERVER_MORE_TO_DOWNLOAD;
    case syncer::SyncerError::SYNCER_OK:
      return vivaldi::sync::SyncerError::SYNCER_ERROR_SYNCER_OK;
  }
}

std::vector<extensions::vivaldi::sync::DisableReason>
ToVivaldiSyncDisableReasons(int reasons) {
  std::vector<extensions::vivaldi::sync::DisableReason> disable_reasons;

  if (reasons & syncer::SyncService::DISABLE_REASON_PLATFORM_OVERRIDE)
    disable_reasons.push_back(
        vivaldi::sync::DisableReason::DISABLE_REASON_PLATFORM_OVERRIDE);
  if (reasons & syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)
    disable_reasons.push_back(
        vivaldi::sync::DisableReason::DISABLE_REASON_ENTERPRISE_POLICY);
  if (reasons & syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN)
    disable_reasons.push_back(
        vivaldi::sync::DisableReason::DISABLE_REASON_NOT_SIGNED_IN);
  if (reasons & syncer::SyncService::DISABLE_REASON_USER_CHOICE)
    disable_reasons.push_back(
        vivaldi::sync::DisableReason::DISABLE_REASON_USER_CHOICE);
  if (reasons & syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR)
    disable_reasons.push_back(
        vivaldi::sync::DisableReason::DISABLE_REASON_UNRECOVERABLE_ERROR);

  return disable_reasons;
}

vivaldi::sync::ProtocolErrorType ToVivaldiSyncProtocolErrorType(
    syncer::SyncProtocolErrorType error_type) {
  switch (error_type) {
    case syncer::SYNC_SUCCESS:
      return vivaldi::sync::ProtocolErrorType::PROTOCOL_ERROR_TYPE_SUCCESS;
    case syncer::NOT_MY_BIRTHDAY:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_NOT_MY_BIRTHDAY;
    case syncer::THROTTLED:
      return vivaldi::sync::ProtocolErrorType::PROTOCOL_ERROR_TYPE_THROTTLED;
    case syncer::CLEAR_PENDING:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_CLEAR_PENDING;
    case syncer::TRANSIENT_ERROR:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_TRANSIENT_ERROR;
    case syncer::MIGRATION_DONE:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_MIGRATION_DONE;
    case syncer::INVALID_CREDENTIAL:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_INVALID_CREDENTIAL;
    case syncer::DISABLED_BY_ADMIN:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_DISABLED_BY_ADMIN;
    case syncer::PARTIAL_FAILURE:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_PARTIAL_FAILURE;
    case syncer::CLIENT_DATA_OBSOLETE:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_CLIENT_DATA_OBSOLETE;
    case syncer::UNKNOWN_ERROR:
      return vivaldi::sync::ProtocolErrorType::PROTOCOL_ERROR_TYPE_UNKNOWN;
  }
}

vivaldi::sync::ClientAction ToVivaldiSyncClientAction(
    syncer::ClientAction client_action) {
  switch (client_action) {
    case syncer::UPGRADE_CLIENT:
      return vivaldi::sync::ClientAction::CLIENT_ACTION_UPGRADE_CLIENT;
    case syncer::CLEAR_USER_DATA_AND_RESYNC:
      return vivaldi::sync::ClientAction::
          CLIENT_ACTION_CLEAR_USER_DATA_AND_RESYNC;
    case syncer::ENABLE_SYNC_ON_ACCOUNT:
      return vivaldi::sync::ClientAction::CLIENT_ACTION_ENABLE_SYNC_ON_ACCOUNT;
    case syncer::STOP_AND_RESTART_SYNC:
      return vivaldi::sync::ClientAction::CLIENT_ACTION_STOP_AND_RESTART_SYNC;
    case syncer::DISABLE_SYNC_ON_CLIENT:
      return vivaldi::sync::ClientAction::CLIENT_ACTION_DISABLE_SYNC_ON_CLIENT;
    case syncer::STOP_SYNC_FOR_DISABLED_ACCOUNT:
      return vivaldi::sync::ClientAction::
          CLIENT_ACTION_STOP_SYNC_FOR_DISABLED_ACCOUNT;
    case syncer::RESET_LOCAL_SYNC_DATA:
      return vivaldi::sync::ClientAction::CLIENT_ACTION_RESET_LOCAL_SYNC_DATA;
    case syncer::UNKNOWN_ACTION:
      return vivaldi::sync::ClientAction::CLIENT_ACTION_UNKNOWN;
  }
}

vivaldi::sync::CycleData GetLastCycleData(syncer::SyncService* sync_service) {
  vivaldi::sync::CycleData cycle_data;
  cycle_data.is_ready = true;

  if (!sync_service) {
    cycle_data.has_synced = false;
    cycle_data.last_sync_time = 0;
    cycle_data.last_commit_result =
        vivaldi::sync::SyncerError::SYNCER_ERROR_UNSET;
    cycle_data.last_download_updates_result =
        vivaldi::sync::SyncerError::SYNCER_ERROR_UNSET;
    return cycle_data;
  }

  syncer::SyncCycleSnapshot cycle_snapshot =
      sync_service->GetLastCycleSnapshot();

  cycle_data.last_sync_time = cycle_snapshot.sync_start_time().ToJsTime();
  cycle_data.last_commit_result =
      ToVivaldiSyncerError(cycle_snapshot.model_neutral_state().commit_result);
  cycle_data.last_download_updates_result = ToVivaldiSyncerError(
      cycle_snapshot.model_neutral_state().last_download_updates_result);
  cycle_data.has_remaining_local_changes =
      cycle_snapshot.has_remaining_local_changes();
  cycle_data.has_synced = cycle_snapshot.is_initialized();

  syncer::SyncStatus status;
  sync_service->QueryDetailedSyncStatus(&status);

  cycle_data.next_retry_time = status.retry_time.ToJsTime();

  return cycle_data;
}

vivaldi::sync::EngineData GetEngineData(Profile* profile) {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(profile);

  vivaldi::sync::EngineData engine_data;
  engine_data.is_ready = true;

  if (!sync_manager) {
    engine_data.engine_state = vivaldi::sync::EngineState::ENGINE_STATE_FAILED;
    if (!switches::IsSyncAllowedByFlag())
      engine_data.disable_reasons = {
          vivaldi::sync::DisableReason::DISABLE_REASON_FLAG};
    engine_data.protocol_error_type =
        vivaldi::sync::ProtocolErrorType::PROTOCOL_ERROR_TYPE_UNKNOWN;
    engine_data.protocol_error_client_action =
        vivaldi::sync::ClientAction::CLIENT_ACTION_UNKNOWN;

    return engine_data;
  }

  if (sync_manager->is_clearing_sync_data()) {
    engine_data.engine_state =
        vivaldi::sync::EngineState::ENGINE_STATE_CLEARING_DATA;
  } else if (!sync_manager->GetUserSettings()->IsSyncRequested() ||
             sync_manager->GetTransportState() ==
                 syncer::SyncService::TransportState::START_DEFERRED) {
    engine_data.engine_state = vivaldi::sync::EngineState::ENGINE_STATE_STOPPED;

  } else if (!sync_manager->CanSyncFeatureStart()) {
    engine_data.engine_state = vivaldi::sync::EngineState::ENGINE_STATE_FAILED;

  } else if (sync_manager->IsEngineInitialized()) {
    if (sync_manager->GetTransportState() ==
            syncer::SyncService::TransportState::
                PENDING_DESIRED_CONFIGURATION ||
        !sync_manager->GetUserSettings()->IsFirstSetupComplete()) {
      engine_data.engine_state =
          vivaldi::sync::EngineState::ENGINE_STATE_CONFIGURATION_PENDING;
    } else {
      engine_data.engine_state =
          vivaldi::sync::EngineState::ENGINE_STATE_STARTED;
    }

  } else {
    engine_data.engine_state =
        vivaldi::sync::EngineState::ENGINE_STATE_STARTING;
  }

  engine_data.disable_reasons =
      ToVivaldiSyncDisableReasons(sync_manager->GetDisableReasons());

  syncer::SyncStatus status;
  sync_manager->QueryDetailedSyncStatus(&status);

  engine_data.protocol_error_type =
      ToVivaldiSyncProtocolErrorType(status.sync_protocol_error.error_type);
  engine_data.protocol_error_description =
      status.sync_protocol_error.error_description;
  engine_data.protocol_error_client_action =
      ToVivaldiSyncClientAction(status.sync_protocol_error.action);

  engine_data.is_encrypting_everything =
      sync_manager->IsEngineInitialized()
          ? sync_manager->GetUserSettings()->IsEncryptEverythingEnabled()
          : false;
  engine_data.uses_encryption_password =
      sync_manager->GetUserSettings()->IsUsingSecondaryPassphrase();
  engine_data.needs_decryption_password =
      sync_manager->GetUserSettings()->IsPassphraseRequiredForDecryption();
  engine_data.is_setup_in_progress = sync_manager->IsSetupInProgress();
  engine_data.is_first_setup_complete =
      sync_manager->GetUserSettings()->IsFirstSetupComplete();

  engine_data.sync_everything =
      sync_manager->GetUserSettings()->IsSyncEverythingEnabled();
  syncer::ModelTypeSet chosen_types =
      sync_manager->GetUserSettings()->GetChosenDataTypes();
  syncer::ModelTypeNameMap model_type_map =
      syncer::GetUserSelectableTypeNameMap();
  std::vector<vivaldi::sync::DataType> data_types;
  for (const auto& model_type : model_type_map) {
    // Skip the model types that don't make sense for us to synchronize.
    if (model_type.first == syncer::THEMES ||
        model_type.first == syncer::APPS ||
        model_type.first == syncer::USER_EVENTS ||
        model_type.first == syncer::PROXY_TABS) {
      continue;
    }
    vivaldi::sync::DataType data_type;
    data_type.name.assign(syncer::ModelTypeToString(model_type.first));
    data_type.enabled = chosen_types.Has(model_type.first);
    engine_data.data_types.push_back(std::move(data_type));
  }

  return engine_data;
}

}  // anonymous namespace

SyncEventRouter::SyncEventRouter(Profile* profile) : profile_(profile) {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(profile);
  if (sync_manager)
    sync_manager->AddObserver(this);
}

SyncEventRouter::~SyncEventRouter() {}

void SyncEventRouter::OnStateChanged(syncer::SyncService* sync) {
  ::vivaldi::BroadcastEvent(
      vivaldi::sync::OnEngineStateChanged::kEventName,
      vivaldi::sync::OnEngineStateChanged::Create(GetEngineData(profile_)),
      profile_);
}

void SyncEventRouter::OnSyncCycleCompleted(syncer::SyncService* sync) {
  ::vivaldi::BroadcastEvent(
      vivaldi::sync::OnCycleCompleted::kEventName,
      vivaldi::sync::OnCycleCompleted::Create(GetLastCycleData(sync)),
      profile_);
}

void SyncEventRouter::OnSyncShutdown(syncer::SyncService* sync) {
  sync->RemoveObserver(this);
}

SyncAPI::SyncAPI(content::BrowserContext* context) : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(
      this, vivaldi::sync::OnEngineStateChanged::kEventName);
  event_router->RegisterObserver(this,
                                 vivaldi::sync::OnCycleCompleted::kEventName);
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

void SyncAPI::StartSyncSetup(syncer::SyncService* sync) {
  if (!sync_setup_handle_.get())
    sync_setup_handle_ = sync->GetSetupInProgressHandle();
}

// KeyedService implementation.
void SyncAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void SyncAPI::SyncSetupComplete() {
  sync_setup_handle_.reset();
}

ExtensionFunction::ResponseAction SyncStartFunction::Run() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(context_));

  if (!sync_manager)
    return RespondNow(NoArguments());

  sync_manager->GetUserSettings()->SetSyncRequested(true);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SyncStopFunction::Run() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(context_));

  if (!sync_manager)
    return RespondNow(NoArguments());

  sync_manager->StopAndClear();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SyncSetEncryptionPasswordFunction::Run() {
  std::unique_ptr<vivaldi::sync::SetEncryptionPassword::Params> params(
      vivaldi::sync::SetEncryptionPassword::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(context_));
  if (!sync_manager)
    return RespondNow(Error("Sync manager is unavailable"));

  if (!sync_manager->IsEngineInitialized())
    return RespondNow(Error("Sync engine is not ready"));

  bool success = false;

  if (sync_manager->IsPassphraseRequired()) {
    success = sync_manager->GetUserSettings()->SetDecryptionPassphrase(
        params->password);
  } else if (!sync_manager->GetUserSettings()->IsUsingSecondaryPassphrase()) {
    sync_manager->GetUserSettings()->SetEncryptionPassphrase(params->password);
    success = true;
  }

  return RespondNow(ArgumentList(
      vivaldi::sync::SetEncryptionPassword::Results::Create(success)));
}

SyncGetDefaultSessionNameFunction::SyncGetDefaultSessionNameFunction() =
    default;
SyncGetDefaultSessionNameFunction::~SyncGetDefaultSessionNameFunction() =
    default;

ExtensionFunction::ResponseAction SyncGetDefaultSessionNameFunction::Run() {
  base::PostTaskAndReplyWithResult(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
          .get(),
      FROM_HERE, base::Bind(&syncer::GetSessionNameBlocking),
      base::Bind(&SyncGetDefaultSessionNameFunction::OnGetDefaultSessionName,
                 this));
  return RespondLater();
}

void SyncGetDefaultSessionNameFunction::OnGetDefaultSessionName(
    const std::string& session_name) {
  Respond(ArgumentList(
      vivaldi::sync::GetDefaultSessionName::Results::Create(session_name)));
}

ExtensionFunction::ResponseAction SyncSetTypesFunction::Run() {
  std::unique_ptr<vivaldi::sync::SetTypes::Params> params(
      vivaldi::sync::SetTypes::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(context_));
  if (!sync_manager)
    return RespondNow(Error("Sync manager is unavailable"));

  SyncAPI::GetFactoryInstance()
      ->Get(Profile::FromBrowserContext(context_))
      ->StartSyncSetup(sync_manager);

  syncer::ModelTypeSet chosen_types;
  for (const auto& type : params->types) {
    if (type.enabled)
      chosen_types.Put(syncer::ModelTypeFromString(type.name));
  }
  if (chosen_types.Has(syncer::TYPED_URLS))
    chosen_types.Put(syncer::PROXY_TABS);

  sync_manager->GetUserSettings()->SetChosenDataTypes(params->sync_everything,
                                                      chosen_types);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SyncGetEngineStateFunction::Run() {
  return RespondNow(ArgumentList(vivaldi::sync::GetEngineState::Results::Create(
      GetEngineData(Profile::FromBrowserContext(context_)))));
}

ExtensionFunction::ResponseAction SyncGetLastCycleStateFunction::Run() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(context_));

  return RespondNow(
      ArgumentList(vivaldi::sync::GetLastCycleState::Results::Create(
          GetLastCycleData(sync_manager))));
}

ExtensionFunction::ResponseAction SyncClearDataFunction::Run() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(context_));
  if (!sync_manager)
    return RespondNow(Error("Sync manager is unavailable"));

  sync_manager->ClearSyncData();

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SyncSetupCompleteFunction::Run() {
  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(context_));
  if (!sync_manager)
    return RespondNow(Error("Sync manager is unavailable"));

  sync_manager->GetUserSettings()->SetFirstSetupComplete();
  SyncAPI::GetFactoryInstance()
      ->Get(Profile::FromBrowserContext(context_))
      ->SyncSetupComplete();

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SyncUpdateNotificationClientStatusFunction::Run() {
  std::unique_ptr<vivaldi::sync::UpdateNotificationClientStatus::Params> params(
      vivaldi::sync::UpdateNotificationClientStatus::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(context_));
  if (!sync_manager)
    return RespondNow(Error("Sync manager is unavailable"));

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

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SyncNotificationReceivedFunction::Run() {
  std::unique_ptr<vivaldi::sync::NotificationReceived::Params> params(
      vivaldi::sync::NotificationReceived::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiSyncManager* sync_manager =
      VivaldiSyncManagerFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(context_));
  if (!sync_manager)
    return RespondNow(Error("Sync manager is unavailable"));

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

  return RespondNow(NoArguments());
}

}  // namespace extensions
