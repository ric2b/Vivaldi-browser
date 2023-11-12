// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_ui_helper.h"

#include <tuple>

#include "base/base64.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/syncer_error.h"
#include "sync/vivaldi_sync_service_impl.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "vivaldi_account/vivaldi_account_manager.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

namespace vivaldi {

EngineData::EngineData() = default;
EngineData::EngineData(const EngineData& other) = default;
EngineData::~EngineData() = default;

VivaldiSyncUIHelper::VivaldiSyncUIHelper(VivaldiSyncServiceImpl* sync_service,
                                         VivaldiAccountManager* account_manager)
    : sync_service_(sync_service), account_manager_(account_manager) {}

void VivaldiSyncUIHelper::RegisterObserver() {
  sync_service_->AddObserver(this);
}

VivaldiSyncUIHelper::CycleData VivaldiSyncUIHelper::GetCycleData() {
  CycleData cycle_data;

  syncer::SyncCycleSnapshot cycle_snapshot =
      sync_service_->GetLastCycleSnapshotForDebugging();
  syncer::SyncStatus status;
  sync_service_->QueryDetailedSyncStatusForDebugging(&status);

  cycle_data.cycle_start_time = cycle_snapshot.sync_start_time();
  cycle_data.next_retry_time = status.retry_time;

  if (!cycle_snapshot.is_initialized()) {
    cycle_data.download_updates_status = NOT_SYNCED;
    cycle_data.commit_status = NOT_SYNCED;
    return cycle_data;
  }

  switch (cycle_snapshot.model_neutral_state()
              .last_download_updates_result.value()) {
    case syncer::SyncerError::UNSET:
    case syncer::SyncerError::SYNCER_OK:
      cycle_data.download_updates_status = SUCCESS;
      break;
    case syncer::SyncerError::SYNC_AUTH_ERROR:
      cycle_data.download_updates_status = AUTH_ERROR;
      break;
    case syncer::SyncerError::SYNC_SERVER_ERROR:
    case syncer::SyncerError::SERVER_RESPONSE_VALIDATION_FAILED:
    case syncer::SyncerError::SERVER_RETURN_TRANSIENT_ERROR:
      cycle_data.download_updates_status = SERVER_ERROR;
      break;
    case syncer::SyncerError::NETWORK_CONNECTION_UNAVAILABLE:
    case syncer::SyncerError::NETWORK_IO_ERROR:
      cycle_data.download_updates_status = NETWORK_ERROR;
      break;
    case syncer::SyncerError::SERVER_RETURN_THROTTLED:
      cycle_data.download_updates_status = THROTTLED;
      break;
    default:
      // These errors should not occur either because they do not make sense or
      // are already covered elsewhere
      cycle_data.download_updates_status = OTHER_ERROR;
  }

  switch (cycle_snapshot.model_neutral_state().commit_result.value()) {
    case syncer::SyncerError::UNSET:
    case syncer::SyncerError::SYNCER_OK:
      if (cycle_data.download_updates_status != SUCCESS)
        cycle_data.commit_status = NOT_SYNCED;
      else
        cycle_data.commit_status = SUCCESS;
      break;
    case syncer::SyncerError::SYNC_AUTH_ERROR:
      cycle_data.commit_status = AUTH_ERROR;
      break;
    case syncer::SyncerError::SYNC_SERVER_ERROR:
    case syncer::SyncerError::SERVER_RESPONSE_VALIDATION_FAILED:
    case syncer::SyncerError::SERVER_RETURN_TRANSIENT_ERROR:
      cycle_data.commit_status = SERVER_ERROR;
      break;
    case syncer::SyncerError::NETWORK_CONNECTION_UNAVAILABLE:
    case syncer::SyncerError::NETWORK_IO_ERROR:
      cycle_data.commit_status = NETWORK_ERROR;
      break;
    case syncer::SyncerError::SERVER_RETURN_CONFLICT:
      cycle_data.commit_status = CONFLICT;
      break;
    case syncer::SyncerError::SERVER_RETURN_THROTTLED:
      cycle_data.commit_status = THROTTLED;
      break;
    default:
      // These errors should not occur either because they do not make sense or
      // are already covered elsewhere
      cycle_data.commit_status = OTHER_ERROR;
  }

  return cycle_data;
}

vivaldi::EngineData VivaldiSyncUIHelper::GetEngineData() {
  vivaldi::EngineData engine_data;

  if (sync_service_->is_clearing_sync_data()) {
    engine_data.engine_state = EngineState::CLEARING_DATA;
  } else if (!sync_service_->HasSyncConsent() ||
             sync_service_->GetTransportState() ==
                 syncer::SyncService::TransportState::START_DEFERRED) {
    engine_data.engine_state = EngineState::STOPPED;
  } else if (!sync_service_->CanSyncFeatureStart()) {
    engine_data.engine_state = EngineState::FAILED;
  } else if (sync_service_->IsEngineInitialized()) {
    if (sync_service_->GetTransportState() ==
            syncer::SyncService::TransportState::
                PENDING_DESIRED_CONFIGURATION ||
        !sync_service_->GetUserSettings()
             ->IsInitialSyncFeatureSetupComplete()) {
      engine_data.engine_state = EngineState::CONFIGURATION_PENDING;
    } else {
      engine_data.engine_state = EngineState::STARTED;
    }
  } else if (sync_service_->GetSyncTokenStatusForDebugging()
                 .connection_status == syncer::CONNECTION_SERVER_ERROR) {
    engine_data.engine_state = EngineState::STARTING_SERVER_ERROR;
  } else {
    engine_data.engine_state = EngineState::STARTING;
  }

  engine_data.disable_reasons = sync_service_->GetDisableReasons();

  syncer::SyncStatus status;
  sync_service_->QueryDetailedSyncStatusForDebugging(&status);

  engine_data.protocol_error_type = status.sync_protocol_error.error_type;
  engine_data.protocol_error_description =
      status.sync_protocol_error.error_description;
  engine_data.protocol_error_client_action = status.sync_protocol_error.action;

  engine_data.is_encrypting_everything =
      sync_service_->IsEngineInitialized()
          ? sync_service_->GetUserSettings()->IsEncryptEverythingEnabled()
          : false;
  engine_data.uses_encryption_password =
      sync_service_->GetUserSettings()->IsUsingExplicitPassphrase();
  engine_data.needs_decryption_password =
      sync_service_->GetUserSettings()
          ->IsPassphraseRequiredForPreferredDataTypes();
  engine_data.is_setup_in_progress = sync_service_->IsSetupInProgress();
  engine_data.is_first_setup_complete =
      sync_service_->GetUserSettings()->IsInitialSyncFeatureSetupComplete();

  engine_data.sync_everything =
      sync_service_->GetUserSettings()->IsSyncEverythingEnabled();

  engine_data.data_types = sync_service_->GetUserSettings()->GetSelectedTypes();

  return engine_data;
}

bool VivaldiSyncUIHelper::SetEncryptionPassword(const std::string& password) {
  if (sync_service_->GetUserSettings()->IsPassphraseRequired()) {
    if (password.empty())
      return false;
    return sync_service_->GetUserSettings()->SetDecryptionPassphrase(password);
  }

  if (sync_service_->GetUserSettings()->IsUsingExplicitPassphrase())
    return false;

  if (!password.empty()) {
    sync_service_->GetUserSettings()->SetEncryptionPassphrase(password);
    return true;
  }

  return false;
}

void VivaldiSyncUIHelper::OnStateChanged(syncer::SyncService* sync) {
  if (!sync->IsEngineInitialized()) {
    tried_decrypt_ = false;
    return;
  }

  if (!sync->GetUserSettings()->IsPassphraseRequiredForPreferredDataTypes() ||
      tried_decrypt_)
    return;

  tried_decrypt_ = true;

  std::string password = account_manager_->password_handler()->password();

  if (!password.empty()) {
    // See if the user is using the same encryption and login password. If yes,
    // this will cause the engine to proceed to the next step, and cause the
    // encryption password prompt UI to be skipped.
    // Otherwise, the UI will just stick to showing the password prompt, so we
    // can silently drop informing the UI about it.
    std::ignore =
        sync_service_->GetUserSettings()->SetDecryptionPassphrase(password);
  }
}

std::string VivaldiSyncUIHelper::GetBackupEncryptionToken() {
  std::string packed_key = sync_service_->GetEncryptionBootstrapToken();

  if (packed_key.empty())
    return std::string();

  std::string decoded_key;
  if (!base::Base64Decode(packed_key, &decoded_key)) {
    DLOG(ERROR) << "Failed to decode explicit passphrase key.";
    return std::string();
  }

  std::string decrypted_key;
  if (!OSCrypt::DecryptString(decoded_key, &decrypted_key)) {
    DLOG(ERROR) << "Failed to decrypt explicit passphrase key.";
    return std::string();
  }

  std::string encoded_key;
  base::Base64Encode(decrypted_key, &encoded_key);
  return encoded_key;
}

bool VivaldiSyncUIHelper::RestoreEncryptionToken(
    const base::StringPiece& token) {
  if (token.empty())
    return false;

  std::string decoded_token;
  if (!base::Base64Decode(token, &decoded_token)) {
    DLOG(ERROR) << "Failed to decode token.";
    return false;
  }

  // The sync engine expects to receive an encrypted token.
  std::string encrypted_token;
  if (!OSCrypt::EncryptString(decoded_token, &encrypted_token)) {
    DLOG(ERROR) << "Failed to encrypt token.";
    return false;
  }

  std::string encoded_token;
  base::Base64Encode(encrypted_token, &encoded_token);
  sync_service_->ResetEncryptionBootstrapToken(encoded_token);

  return true;
}

void VivaldiSyncUIHelper::OnSyncShutdown(syncer::SyncService* sync) {
  sync->RemoveObserver(this);
}

VivaldiSyncUIHelper::~VivaldiSyncUIHelper() {}

}  // namespace vivaldi