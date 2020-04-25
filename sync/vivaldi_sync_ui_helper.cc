// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_ui_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "sync/vivaldi_profile_sync_service.h"

#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "vivaldi_account/vivaldi_account_manager.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"

namespace vivaldi {

VivaldiSyncUIHelper::VivaldiSyncUIHelper(Profile* profile,
                                         VivaldiProfileSyncService* sync_manager)
    : profile_(profile), sync_manager_(sync_manager) {
  sync_manager_->AddObserver(this);
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
            vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::AYE));
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
          vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::NAY));
  sync_manager_->GetUserSettings()->SetEncryptionPassphrase(login_password);
  return true;
}

void VivaldiSyncUIHelper::OnStateChanged(syncer::SyncService* sync) {
  if (!sync->IsEngineInitialized()) {
    tried_decrypt_ = false;
    return;
  }

  if (!sync->GetUserSettings()->IsPassphraseRequiredForDecryption() ||
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
            vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::NAY));
  } else {
    profile_->GetPrefs()->SetInteger(
        vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword,
        static_cast<int>(
            vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::AYE));
  }
}

void VivaldiSyncUIHelper::OnSyncShutdown(syncer::SyncService* sync) {
  sync->RemoveObserver(this);
}

VivaldiSyncUIHelper::~VivaldiSyncUIHelper() {}

}  // namespace vivaldi