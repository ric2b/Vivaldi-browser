// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_ui_helper.h"

#include "base/base64.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/syncer_error.h"
#include "sync/vivaldi_sync_service_impl.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "vivaldi_account/vivaldi_account_manager.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

namespace vivaldi {
VivaldiSyncUIHelper::VivaldiSyncUIHelper(Profile* profile,
                                         VivaldiSyncServiceImpl* sync_service)
    : profile_(profile), sync_service_(sync_service) {}

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
    case syncer::SyncerError::DATATYPE_TRIGGERED_RETRY:
    case syncer::SyncerError::SERVER_MORE_TO_DOWNLOAD:
      // We don't ever get notified of this case in practice, but we support it
      // anyway in case it becomes relevant in the future.
      cycle_data.download_updates_status = IN_PROGRESS;
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
    case syncer::SyncerError::CANNOT_DO_WORK:
      cycle_data.download_updates_status = CLIENT_ERROR;
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
    profile_->GetPrefs()->SetInteger(
        vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword,
        static_cast<int>(
            vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::kYes));
    return true;
  }

  std::string login_password =
      VivaldiAccountManagerFactory::GetForProfile(profile_)
          ->password_handler()
          ->password();

  if (login_password.empty())
    return false;

  profile_->GetPrefs()->SetInteger(
      vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword,
      static_cast<int>(
          vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::kNo));
  sync_service_->GetUserSettings()->SetEncryptionPassphrase(login_password);
  return true;
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

  std::string password = VivaldiAccountManagerFactory::GetForProfile(profile_)
                             ->password_handler()
                             ->password();

  if (!password.empty() &&
      sync_service_->GetUserSettings()->SetDecryptionPassphrase(password)) {
    profile_->GetPrefs()->SetInteger(
        vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword,
        static_cast<int>(
            vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::kNo));
  } else {
    profile_->GetPrefs()->SetInteger(
        vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword,
        static_cast<int>(
            vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::kYes));
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