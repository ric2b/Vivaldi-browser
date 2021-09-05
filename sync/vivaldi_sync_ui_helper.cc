// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_ui_helper.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/syncer_error.h"
#include "sync/vivaldi_profile_sync_service.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "vivaldi_account/vivaldi_account_manager.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

namespace vivaldi {
namespace {
bool DoBackupEncryptionToken(const base::FilePath& target,
                             const std::string& packed_key) {
  bool result = !packed_key.empty();

  std::string decoded_key;
  if (result && !base::Base64Decode(packed_key, &decoded_key)) {
    DLOG(ERROR) << "Failed to decode explicit passphrase key.";
    result = false;
  }

  std::string decrypted_key;
  if (result && !OSCrypt::DecryptString(decoded_key, &decrypted_key)) {
    DLOG(ERROR) << "Failed to decrypt explicit passphrase key.";
    result = false;
  }

  std::string encoded_key;
  base::Base64Encode(decrypted_key, &encoded_key);

  return result && base::WriteFile(target, encoded_key);
}

void ReadEncryptionToken(
    const base::FilePath& source,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    VivaldiSyncUIHelper::ResultCallback result_callback,
    base::OnceCallback<void(const std::string&)> token_callback) {
  DCHECK(callback_task_runner);
  std::string token;
  bool result = base::ReadFileToString(source, &token);

  std::string decoded_token;
  if (result && !base::Base64Decode(token, &decoded_token)) {
    DLOG(ERROR) << "Failed to decode token.";
    result = false;
  }

  // The sync engine expects to receive an encrypted token.
  std::string encrypted_token;
  if (result && !OSCrypt::EncryptString(decoded_token, &encrypted_token)) {
    DLOG(ERROR) << "Failed to encrypt token.";
    result = false;
  }

  std::string encoded_token;
  base::Base64Encode(encrypted_token, &encoded_token);

  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback), result));
  if (result)
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(token_callback), encoded_token));
}

}  // namespace

VivaldiSyncUIHelper::VivaldiSyncUIHelper(
    Profile* profile,
    VivaldiProfileSyncService* sync_manager)
    : profile_(profile), sync_manager_(sync_manager) {
  sync_manager_->AddObserver(this);
}

VivaldiSyncUIHelper::CycleData VivaldiSyncUIHelper::GetCycleData() {
  CycleData cycle_data;

  syncer::SyncCycleSnapshot cycle_snapshot =
      sync_manager_->GetLastCycleSnapshotForDebugging();
  syncer::SyncStatus status;
  sync_manager_->QueryDetailedSyncStatusForDebugging(&status);

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
  if (sync_manager_->GetUserSettings()->IsPassphraseRequired()) {
    if (password.empty())
      return false;
    return sync_manager_->GetUserSettings()->SetDecryptionPassphrase(password);
  }

  if (sync_manager_->GetUserSettings()->IsUsingSecondaryPassphrase())
    return false;

  if (!password.empty()) {
    sync_manager_->GetUserSettings()->SetEncryptionPassphrase(password);
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
  sync_manager_->GetUserSettings()->SetEncryptionPassphrase(login_password);
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
      sync_manager_->GetUserSettings()->SetDecryptionPassphrase(password)) {
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

void VivaldiSyncUIHelper::BackupEncryptionToken(const base::FilePath& target,
                                                ResultCallback callback) {
  std::string packed_key = sync_manager_->GetEncryptionBootstrapToken();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&DoBackupEncryptionToken, target, packed_key),
      std::move(callback));
}

void VivaldiSyncUIHelper::RestoreEncryptionToken(const base::FilePath& source,
                                                 ResultCallback callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &ReadEncryptionToken, source, base::SequencedTaskRunnerHandle::Get(),
          std::move(callback),
          base::BindOnce(
              &VivaldiProfileSyncService::SetEncryptionBootstrapToken,
              sync_manager_->AsWeakPtr())));
}

void OnEncryptionTokenRead(const std::string& token);

void VivaldiSyncUIHelper::OnSyncShutdown(syncer::SyncService* sync) {
  sync->RemoveObserver(this);
}

VivaldiSyncUIHelper::~VivaldiSyncUIHelper() {}

}  // namespace vivaldi