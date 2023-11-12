// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/api/sync/sync_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/sync_token_status.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/sync.h"
#include "extensions/tools/vivaldi_tools.h"
#include "sync/vivaldi_sync_service_factory.h"
#include "sync/vivaldi_sync_service_impl.h"
#include "sync/vivaldi_sync_ui_helper.h"

using syncer::SyncService;
using vivaldi::VivaldiSyncServiceFactory;
using vivaldi::VivaldiSyncServiceImpl;

namespace extensions {

namespace {
bool WriteFileWrapper(const base::FilePath& filename,
                      std::unique_ptr<std::string> data) {
  return base::WriteFile(filename, *data);
}

vivaldi::sync::CycleStatus ToVivaldiCycleStatus(
    ::vivaldi::VivaldiSyncUIHelper::CycleStatus cycle_status) {
  switch (cycle_status) {
    case ::vivaldi::VivaldiSyncUIHelper::NOT_SYNCED:
      return vivaldi::sync::CycleStatus::CYCLE_STATUS_NOT_SYNCED;
    case ::vivaldi::VivaldiSyncUIHelper::SUCCESS:
      return vivaldi::sync::CycleStatus::CYCLE_STATUS_SUCCESS;
    case ::vivaldi::VivaldiSyncUIHelper::AUTH_ERROR:
      return vivaldi::sync::CycleStatus::CYCLE_STATUS_AUTH_ERROR;
    case ::vivaldi::VivaldiSyncUIHelper::SERVER_ERROR:
      return vivaldi::sync::CycleStatus::CYCLE_STATUS_SERVER_ERROR;
    case ::vivaldi::VivaldiSyncUIHelper::NETWORK_ERROR:
      return vivaldi::sync::CycleStatus::CYCLE_STATUS_NETWORK_ERROR;
    case ::vivaldi::VivaldiSyncUIHelper::CONFLICT:
      return vivaldi::sync::CycleStatus::CYCLE_STATUS_CONFLICT;
    case ::vivaldi::VivaldiSyncUIHelper::THROTTLED:
      return vivaldi::sync::CycleStatus::CYCLE_STATUS_THROTTLED;
    case ::vivaldi::VivaldiSyncUIHelper::OTHER_ERROR:
      return vivaldi::sync::CycleStatus::CYCLE_STATUS_OTHER_ERROR;
  }
}

std::vector<extensions::vivaldi::sync::DisableReason>
ToVivaldiSyncDisableReasons(syncer::SyncService::DisableReasonSet reasons) {
  std::vector<extensions::vivaldi::sync::DisableReason> disable_reasons;

  for (auto reason : reasons) {
    switch (reason) {
      case syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY:
        disable_reasons.push_back(
            vivaldi::sync::DisableReason::DISABLE_REASON_ENTERPRISE_POLICY);
        break;
      case syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN:
        disable_reasons.push_back(
            vivaldi::sync::DisableReason::DISABLE_REASON_NOT_SIGNED_IN);
        break;
      case syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR:
        disable_reasons.push_back(
            vivaldi::sync::DisableReason::DISABLE_REASON_UNRECOVERABLE_ERROR);
        break;
      default:
        break;
    }
  }
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
    case syncer::DISABLED_BY_ADMIN:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_DISABLED_BY_ADMIN;
    case syncer::PARTIAL_FAILURE:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_PARTIAL_FAILURE;
    case syncer::CLIENT_DATA_OBSOLETE:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_CLIENT_DATA_OBSOLETE;
    case syncer::ENCRYPTION_OBSOLETE:
      return vivaldi::sync::ProtocolErrorType::
          PROTOCOL_ERROR_TYPE_ENCRYPTION_OBSOLETE;
    case syncer::UNKNOWN_ERROR:
      return vivaldi::sync::ProtocolErrorType::PROTOCOL_ERROR_TYPE_UNKNOWN;
  }
}

vivaldi::sync::ClientAction ToVivaldiSyncClientAction(
    syncer::ClientAction client_action) {
  switch (client_action) {
    case syncer::UPGRADE_CLIENT:
      return vivaldi::sync::ClientAction::CLIENT_ACTION_UPGRADE_CLIENT;
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

vivaldi::sync::DataType ToVivaldiSyncDataType(
    syncer::UserSelectableType data_type) {
  switch (data_type) {
    case syncer::UserSelectableType::kBookmarks:
      return vivaldi::sync::DataType::DATA_TYPE_BOOKMARKS;
    case syncer::UserSelectableType::kPreferences:
      return vivaldi::sync::DataType::DATA_TYPE_PREFERENCES;
    case syncer::UserSelectableType::kPasswords:
      return vivaldi::sync::DataType::DATA_TYPE_PASSWORDS;
    case syncer::UserSelectableType::kAutofill:
      return vivaldi::sync::DataType::DATA_TYPE_AUTOFILL;
    case syncer::UserSelectableType::kHistory:
      return vivaldi::sync::DataType::DATA_TYPE_HISTORY;
    case syncer::UserSelectableType::kExtensions:
      return vivaldi::sync::DataType::DATA_TYPE_EXTENSIONS;
    case syncer::UserSelectableType::kApps:
      return vivaldi::sync::DataType::DATA_TYPE_APPS;
    case syncer::UserSelectableType::kReadingList:
      return vivaldi::sync::DataType::DATA_TYPE_READING_LIST;
    case syncer::UserSelectableType::kTabs:
      return vivaldi::sync::DataType::DATA_TYPE_TABS;
    case syncer::UserSelectableType::kNotes:
      return vivaldi::sync::DataType::DATA_TYPE_NOTES;
    default:
      NOTREACHED();
      return vivaldi::sync::DataType::DATA_TYPE_NONE;
  }
}

absl::optional<syncer::UserSelectableType> FromVivaldiSyncDataType(
    vivaldi::sync::DataType data_type) {
  switch (data_type) {
    case vivaldi::sync::DataType::DATA_TYPE_BOOKMARKS:
      return syncer::UserSelectableType::kBookmarks;
    case vivaldi::sync::DataType::DATA_TYPE_PREFERENCES:
      return syncer::UserSelectableType::kPreferences;
    case vivaldi::sync::DataType::DATA_TYPE_PASSWORDS:
      return syncer::UserSelectableType::kPasswords;
    case vivaldi::sync::DataType::DATA_TYPE_AUTOFILL:
      return syncer::UserSelectableType::kAutofill;
    case vivaldi::sync::DataType::DATA_TYPE_HISTORY:
      return syncer::UserSelectableType::kHistory;
    case vivaldi::sync::DataType::DATA_TYPE_EXTENSIONS:
      return syncer::UserSelectableType::kExtensions;
    case vivaldi::sync::DataType::DATA_TYPE_APPS:
      return syncer::UserSelectableType::kApps;
    case vivaldi::sync::DataType::DATA_TYPE_READING_LIST:
      return syncer::UserSelectableType::kReadingList;
    case vivaldi::sync::DataType::DATA_TYPE_TABS:
      return syncer::UserSelectableType::kTabs;
    case vivaldi::sync::DataType::DATA_TYPE_NOTES:
      return syncer::UserSelectableType::kNotes;
    default:
      NOTREACHED();
      return absl::nullopt;
  }
}

vivaldi::sync::CycleData GetLastCycleData(Profile* profile) {
  VivaldiSyncServiceImpl* sync_service =
      VivaldiSyncServiceFactory::GetForProfileVivaldi(profile);
  vivaldi::sync::CycleData cycle_data;
  cycle_data.is_ready = true;

  if (!sync_service) {
    cycle_data.cycle_start_time = 0;
    cycle_data.download_updates_status =
        vivaldi::sync::CycleStatus::CYCLE_STATUS_NOT_SYNCED;
    cycle_data.commit_status =
        vivaldi::sync::CycleStatus::CYCLE_STATUS_NOT_SYNCED;
    return cycle_data;
  }

  ::vivaldi::VivaldiSyncUIHelper::CycleData helper_cycle_data =
      sync_service->ui_helper()->GetCycleData();

  cycle_data.cycle_start_time = helper_cycle_data.cycle_start_time.ToJsTime();
  cycle_data.download_updates_status =
      ToVivaldiCycleStatus(helper_cycle_data.download_updates_status);
  cycle_data.commit_status =
      ToVivaldiCycleStatus(helper_cycle_data.commit_status);
  cycle_data.next_retry_time = helper_cycle_data.next_retry_time.ToJsTime();

  return cycle_data;
}

vivaldi::sync::EngineData GetEngineData(Profile* profile) {
  VivaldiSyncServiceImpl* sync_service =
      VivaldiSyncServiceFactory::GetForProfileVivaldi(profile);

  vivaldi::sync::EngineData engine_data;
  engine_data.is_ready = true;

  if (!sync_service) {
    engine_data.engine_state = vivaldi::sync::EngineState::ENGINE_STATE_FAILED;
    if (!syncer::IsSyncAllowedByFlag())
      engine_data.disable_reasons = {
          vivaldi::sync::DisableReason::DISABLE_REASON_FLAG};
    engine_data.protocol_error_type =
        vivaldi::sync::ProtocolErrorType::PROTOCOL_ERROR_TYPE_UNKNOWN;
    engine_data.protocol_error_client_action =
        vivaldi::sync::ClientAction::CLIENT_ACTION_UNKNOWN;

    return engine_data;
  }

  if (sync_service->is_clearing_sync_data()) {
    engine_data.engine_state =
        vivaldi::sync::EngineState::ENGINE_STATE_CLEARING_DATA;
  } else if (!sync_service->HasSyncConsent() ||
             sync_service->GetTransportState() ==
                 syncer::SyncService::TransportState::START_DEFERRED) {
    engine_data.engine_state = vivaldi::sync::EngineState::ENGINE_STATE_STOPPED;
  } else if (!sync_service->CanSyncFeatureStart()) {
    engine_data.engine_state = vivaldi::sync::EngineState::ENGINE_STATE_FAILED;
  } else if (sync_service->IsEngineInitialized()) {
    if (sync_service->GetTransportState() ==
            syncer::SyncService::TransportState::
                PENDING_DESIRED_CONFIGURATION ||
        !sync_service->GetUserSettings()->IsInitialSyncFeatureSetupComplete()) {
      engine_data.engine_state =
          vivaldi::sync::EngineState::ENGINE_STATE_CONFIGURATION_PENDING;
    } else {
      engine_data.engine_state =
          vivaldi::sync::EngineState::ENGINE_STATE_STARTED;
    }
  } else if (sync_service->GetSyncTokenStatusForDebugging().connection_status ==
             syncer::CONNECTION_SERVER_ERROR) {
    engine_data.engine_state =
        vivaldi::sync::EngineState::ENGINE_STATE_STARTING_SERVER_ERROR;
  } else {
    engine_data.engine_state =
        vivaldi::sync::EngineState::ENGINE_STATE_STARTING;
  }

  engine_data.disable_reasons =
      ToVivaldiSyncDisableReasons(sync_service->GetDisableReasons());

  syncer::SyncStatus status;
  sync_service->QueryDetailedSyncStatusForDebugging(&status);

  engine_data.protocol_error_type =
      ToVivaldiSyncProtocolErrorType(status.sync_protocol_error.error_type);
  engine_data.protocol_error_description =
      status.sync_protocol_error.error_description;
  engine_data.protocol_error_client_action =
      ToVivaldiSyncClientAction(status.sync_protocol_error.action);

  engine_data.is_encrypting_everything =
      sync_service->IsEngineInitialized()
          ? sync_service->GetUserSettings()->IsEncryptEverythingEnabled()
          : false;
  engine_data.uses_encryption_password =
      sync_service->GetUserSettings()->IsUsingExplicitPassphrase();
  engine_data.needs_decryption_password =
      sync_service->GetUserSettings()
          ->IsPassphraseRequiredForPreferredDataTypes();
  engine_data.is_setup_in_progress = sync_service->IsSetupInProgress();
  engine_data.is_first_setup_complete =
      sync_service->GetUserSettings()->IsInitialSyncFeatureSetupComplete();

  engine_data.sync_everything =
      sync_service->GetUserSettings()->IsSyncEverythingEnabled();
  syncer::UserSelectableTypeSet chosen_types =
      sync_service->GetUserSettings()->GetSelectedTypes();
  std::vector<vivaldi::sync::DataType> data_types;
  for (const syncer::UserSelectableType data_type :
       syncer::UserSelectableTypeSet::All()) {
    // We do not use chrome themes and the saved tab groups feature is currently
    // disabled
    if (data_type == syncer::UserSelectableType::kThemes ||
        data_type == syncer::UserSelectableType::kSavedTabGroups ||
        data_type == syncer::UserSelectableType::kPayments) {
      continue;
    }

    vivaldi::sync::DataTypeSelection data_type_selection;
    data_type_selection.data_type = ToVivaldiSyncDataType(data_type);
    data_type_selection.enabled = chosen_types.Has(data_type);
    engine_data.data_types.push_back(std::move(data_type_selection));
  }

  return engine_data;
}

}  // anonymous namespace

SyncEventRouter::SyncEventRouter(Profile* profile) : profile_(profile) {
  SyncService* sync_service = VivaldiSyncServiceFactory::GetForProfile(profile);
  if (sync_service)
    sync_service->AddObserver(this);
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
      vivaldi::sync::OnCycleCompleted::Create(GetLastCycleData(profile_)),
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
  SyncService* sync_service = VivaldiSyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));

  if (!sync_service)
    return RespondNow(NoArguments());

  sync_service->SetSyncFeatureRequested();

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SyncStopFunction::Run() {
  SyncService* sync_service = VivaldiSyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));

  if (!sync_service)
    return RespondNow(NoArguments());

  sync_service->StopAndClear();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SyncSetEncryptionPasswordFunction::Run() {
  absl::optional<vivaldi::sync::SetEncryptionPassword::Params> params(
      vivaldi::sync::SetEncryptionPassword::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiSyncServiceImpl* sync_service =
      VivaldiSyncServiceFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(browser_context()));
  if (!sync_service)
    return RespondNow(Error("Sync manager is unavailable"));

  if (!sync_service->IsEngineInitialized())
    return RespondNow(Error("Sync engine is not ready"));

  bool success = sync_service->ui_helper()->SetEncryptionPassword(
      params->password ? *params->password : std::string());

  return RespondNow(ArgumentList(
      vivaldi::sync::SetEncryptionPassword::Results::Create(success)));
}

ExtensionFunction::ResponseAction SyncBackupEncryptionTokenFunction::Run() {
  absl::optional<vivaldi::sync::BackupEncryptionToken::Params> params(
      vivaldi::sync::BackupEncryptionToken::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiSyncServiceImpl* sync_service =
      VivaldiSyncServiceFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(browser_context()));
  if (!sync_service)
    return RespondNow(Error("Sync manager is unavailable"));

  if (!sync_service->GetUserSettings()->IsEncryptEverythingEnabled())
    return RespondNow(Error("Encryption not enabled"));

  auto key = std::make_unique<std::string>(
      sync_service->ui_helper()->GetBackupEncryptionToken());

  if (key->empty()) {
    return RespondNow(ArgumentList(
        vivaldi::sync::BackupEncryptionToken::Results::Create(false)));
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteFileWrapper,
                     base::FilePath::FromUTF8Unsafe(params->target_file),
                     std::move(key)),
      base::BindOnce(&SyncBackupEncryptionTokenFunction::OnBackupDone, this));

  AddRef();
  return RespondLater();
}

void SyncBackupEncryptionTokenFunction::OnBackupDone(bool result) {
  Respond(ArgumentList(
      vivaldi::sync::BackupEncryptionToken::Results::Create(result)));
  Release();  // Balances AddRef in Run()
}

ExtensionFunction::ResponseAction SyncRestoreEncryptionTokenFunction::Run() {
  absl::optional<vivaldi::sync::RestoreEncryptionToken::Params> params(
      vivaldi::sync::RestoreEncryptionToken::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  SyncService* sync_service = VivaldiSyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!sync_service)
    return RespondNow(Error("Sync manager is unavailable"));

  if (!sync_service->GetUserSettings()->IsPassphraseRequired())
    return RespondNow(
        Error("Sync currently isn't requiring an encryption password"));

  auto token = std::make_unique<std::string>();
  // token_ptr is expected to be valid as long as token is valid,
  // which should be at least until OnRestoreDone is called. So, it should be
  // safe to use during base::ReadFileToString
  std::string* token_ptr = token.get();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::ReadFileToString,
                     base::FilePath::FromUTF8Unsafe(params->source_file),
                     token_ptr),
      base::BindOnce(&SyncRestoreEncryptionTokenFunction::OnRestoreDone, this,
                     std::move(token)));

  return RespondLater();
}

void SyncRestoreEncryptionTokenFunction::OnRestoreDone(
    std::unique_ptr<std::string> token,
    bool result) {
  if (result) {
    VivaldiSyncServiceImpl* sync_service =
        VivaldiSyncServiceFactory::GetForProfileVivaldi(
            Profile::FromBrowserContext(browser_context()));
    if (!sync_service)
      return Respond(Error("Sync manager is unavailable"));
    result = sync_service->ui_helper()->RestoreEncryptionToken(*token);
  }

  Respond(ArgumentList(
      vivaldi::sync::RestoreEncryptionToken::Results::Create(result)));
}

SyncGetDefaultSessionNameFunction::SyncGetDefaultSessionNameFunction() =
    default;
SyncGetDefaultSessionNameFunction::~SyncGetDefaultSessionNameFunction() =
    default;

ExtensionFunction::ResponseAction SyncGetDefaultSessionNameFunction::Run() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&syncer::GetPersonalizableDeviceNameBlocking),
      base::BindOnce(
          &SyncGetDefaultSessionNameFunction::OnGetDefaultSessionName, this));
  return RespondLater();
}

void SyncGetDefaultSessionNameFunction::OnGetDefaultSessionName(
    const std::string& session_name) {
  Respond(ArgumentList(
      vivaldi::sync::GetDefaultSessionName::Results::Create(session_name)));
}

ExtensionFunction::ResponseAction SyncSetTypesFunction::Run() {
  absl::optional<vivaldi::sync::SetTypes::Params> params(
      vivaldi::sync::SetTypes::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  SyncService* sync_service = VivaldiSyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!sync_service)
    return RespondNow(Error("Sync manager is unavailable"));

  SyncAPI::GetFactoryInstance()
      ->Get(Profile::FromBrowserContext(browser_context()))
      ->StartSyncSetup(sync_service);

  syncer::UserSelectableTypeSet chosen_types;
  for (const auto& type_selection : params->types) {
    if (type_selection.enabled) {
      chosen_types.Put(
          FromVivaldiSyncDataType(type_selection.data_type).value());
    }
  }

  sync_service->GetUserSettings()->SetSelectedTypes(params->sync_everything,
                                                    chosen_types);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SyncGetEngineStateFunction::Run() {
  return RespondNow(ArgumentList(vivaldi::sync::GetEngineState::Results::Create(
      GetEngineData(Profile::FromBrowserContext(browser_context())))));
}

ExtensionFunction::ResponseAction SyncGetLastCycleStateFunction::Run() {
  return RespondNow(
      ArgumentList(vivaldi::sync::GetLastCycleState::Results::Create(
          GetLastCycleData(Profile::FromBrowserContext(browser_context())))));
}

ExtensionFunction::ResponseAction SyncClearDataFunction::Run() {
  VivaldiSyncServiceImpl* sync_service =
      VivaldiSyncServiceFactory::GetForProfileVivaldi(
          Profile::FromBrowserContext(browser_context()));
  if (!sync_service)
    return RespondNow(Error("Sync manager is unavailable"));

  sync_service->ClearSyncData();

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SyncSetupCompleteFunction::Run() {
  SyncService* sync_service = VivaldiSyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!sync_service)
    return RespondNow(Error("Sync manager is unavailable"));

  sync_service->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  SyncAPI::GetFactoryInstance()
      ->Get(Profile::FromBrowserContext(browser_context()))
      ->SyncSetupComplete();

  return RespondNow(NoArguments());
}

}  // namespace extensions
