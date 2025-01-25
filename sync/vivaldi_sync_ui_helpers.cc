// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_ui_helpers.h"

#include <optional>
#include <tuple>

#include "base/base64.h"
#include "base/logging.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/engine/syncer_error.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_token_status.h"
#include "components/sync/service/sync_user_settings.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {
namespace sync_ui_helpers {

EngineData::EngineData() = default;
EngineData::EngineData(const EngineData& other) = default;
EngineData::~EngineData() = default;

CycleData GetCycleData(syncer::SyncService* sync_service) {
  CHECK(sync_service);
  CycleData cycle_data;

  syncer::SyncCycleSnapshot cycle_snapshot =
      sync_service->GetLastCycleSnapshotForDebugging();
  syncer::SyncStatus status;
  sync_service->QueryDetailedSyncStatusForDebugging(&status);

  cycle_data.cycle_start_time = cycle_snapshot.sync_start_time();
  cycle_data.next_retry_time = status.retry_time;

  if (!cycle_snapshot.is_initialized()) {
    cycle_data.download_updates_status = NOT_SYNCED;
    cycle_data.commit_status = NOT_SYNCED;
    return cycle_data;
  }

  switch (cycle_snapshot.model_neutral_state()
              .last_download_updates_result.type()) {
    case syncer::SyncerError::Type::kSuccess:
      cycle_data.download_updates_status = SUCCESS;
      break;
    case syncer::SyncerError::Type::kHttpError:
      if (cycle_snapshot.model_neutral_state()
              .last_download_updates_result.GetHttpErrorOrDie() ==
          net::HTTP_UNAUTHORIZED) {
        cycle_data.download_updates_status = AUTH_ERROR;
      } else {
        cycle_data.download_updates_status = SERVER_ERROR;
      }
      break;
    case syncer::SyncerError::Type::kNetworkError:
      cycle_data.download_updates_status = NETWORK_ERROR;
      break;
    case syncer::SyncerError::Type::kProtocolError:
      switch (cycle_snapshot.model_neutral_state()
                  .last_download_updates_result.GetProtocolErrorOrDie()) {
        case syncer::SyncProtocolErrorType::THROTTLED:
          cycle_data.download_updates_status = THROTTLED;
          break;
        default:
          cycle_data.download_updates_status = OTHER_ERROR;
      }
      break;
    case syncer::SyncerError::Type::kProtocolViolationError:
      cycle_data.download_updates_status = OTHER_ERROR;
      break;
  }

  switch (cycle_snapshot.model_neutral_state().commit_result.type()) {
    case syncer::SyncerError::Type::kSuccess:
      if (cycle_data.download_updates_status != SUCCESS)
        cycle_data.commit_status = NOT_SYNCED;
      else
        cycle_data.commit_status = SUCCESS;
      break;
    case syncer::SyncerError::Type::kHttpError:
      if (cycle_snapshot.model_neutral_state()
              .commit_result.GetHttpErrorOrDie() == net::HTTP_UNAUTHORIZED) {
        cycle_data.commit_status = AUTH_ERROR;
      } else {
        cycle_data.commit_status = SERVER_ERROR;
      }
      break;
    case syncer::SyncerError::Type::kNetworkError:
      cycle_data.commit_status = NETWORK_ERROR;
      break;
    case syncer::SyncerError::Type::kProtocolError:
      switch (cycle_snapshot.model_neutral_state()
                  .commit_result.GetProtocolErrorOrDie()) {
        case syncer::SyncProtocolErrorType::THROTTLED:
          cycle_data.commit_status = THROTTLED;
          break;
        case syncer::SyncProtocolErrorType::CONFLICT:
          cycle_data.commit_status = CONFLICT;
          break;
        default:
          cycle_data.commit_status = OTHER_ERROR;
      }
      break;
    case syncer::SyncerError::Type::kProtocolViolationError:
      cycle_data.commit_status = OTHER_ERROR;
  }

  return cycle_data;
}

EngineData GetEngineData(syncer::SyncService* sync_service) {
  CHECK(sync_service);
  EngineData engine_data;
  if (sync_service->is_clearing_sync_data()) {
    engine_data.engine_state = EngineState::CLEARING_DATA;
  } else if (!sync_service->HasSyncConsent() ||
             sync_service->GetTransportState() ==
                 syncer::SyncService::TransportState::START_DEFERRED) {
    engine_data.engine_state = EngineState::STOPPED;
  } else if (!sync_service->CanSyncFeatureStart()) {
    engine_data.engine_state = EngineState::FAILED;
  } else if (sync_service->IsEngineInitialized()) {
    if (sync_service->GetTransportState() ==
            syncer::SyncService::TransportState::
                PENDING_DESIRED_CONFIGURATION ||
        !sync_service->GetUserSettings()->IsInitialSyncFeatureSetupComplete()) {
      engine_data.engine_state = EngineState::CONFIGURATION_PENDING;
    } else {
      engine_data.engine_state = EngineState::STARTED;
    }
  } else if (sync_service->GetSyncTokenStatusForDebugging().connection_status ==
             syncer::CONNECTION_SERVER_ERROR) {
    engine_data.engine_state = EngineState::STARTING_SERVER_ERROR;
  } else {
    engine_data.engine_state = EngineState::STARTING;
  }

  engine_data.disable_reasons = sync_service->GetDisableReasons();

  syncer::SyncStatus status;
  sync_service->QueryDetailedSyncStatusForDebugging(&status);

  engine_data.protocol_error_type = status.sync_protocol_error.error_type;
  engine_data.protocol_error_description =
      status.sync_protocol_error.error_description;
  engine_data.protocol_error_client_action = status.sync_protocol_error.action;

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

  engine_data.data_types = sync_service->GetUserSettings()->GetSelectedTypes();

  return engine_data;
}

bool SetEncryptionPassword(syncer::SyncService* sync_service,
                           const std::string& password) {
  CHECK(sync_service);
  if (sync_service->GetUserSettings()->IsPassphraseRequired()) {
    if (password.empty())
      return false;
    return sync_service->GetUserSettings()->SetDecryptionPassphrase(password);
  }

  if (sync_service->GetUserSettings()->IsUsingExplicitPassphrase())
    return false;

  if (!password.empty()) {
    sync_service->GetUserSettings()->SetEncryptionPassphrase(password);
    return true;
  }

  return false;
}

std::string GetBackupEncryptionToken(syncer::SyncService* sync_service) {
  CHECK(sync_service);
  std::string packed_key = sync_service->GetEncryptionBootstrapTokenForBackup();

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
  encoded_key = base::Base64Encode(decrypted_key);
  return encoded_key;
}

bool RestoreEncryptionToken(syncer::SyncService* sync_service,
                            const std::string_view& token) {
  CHECK(sync_service);
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
  encoded_token = base::Base64Encode(encrypted_token);
  sync_service->ResetEncryptionBootstrapTokenFromBackup(encoded_token);

  return true;
}
}  // namespace sync_ui_helpers
}  // namespace vivaldi