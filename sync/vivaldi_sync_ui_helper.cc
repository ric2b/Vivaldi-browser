// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_ui_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "sync/vivaldi_syncmanager.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "vivaldi_account/vivaldi_account_manager.h"

namespace vivaldi {

VivaldiSyncUIHelper::VivaldiSyncUIHelper(Profile* profile,
                                         VivaldiSyncManager* sync_manager)
    : profile_(profile), sync_manager_(sync_manager) {
  sync_manager_->AddObserver(this);
}

void VivaldiSyncUIHelper::OnStateChanged(syncer::SyncService* sync) {
  if (!sync->IsEngineInitialized()) {
    tried_decrypt_ = false;
    return;
  }

  if (!sync->GetUserSettings()->IsPassphraseRequiredForDecryption() || tried_decrypt_)
    return;

  tried_decrypt_ = true;

  scoped_refptr<password_manager::PasswordStore> password_store(
      PasswordStoreFactory::GetForProfile(profile_,
                                          ServiceAccessType::IMPLICIT_ACCESS));

  password_manager::PasswordStore::FormDigest form_digest(
      autofill::PasswordForm::SCHEME_OTHER,
      VivaldiAccountManager::kSyncSignonRealm,
      GURL(VivaldiAccountManager::kSyncOrigin));

  password_store->GetLogins(form_digest, this);
}

void VivaldiSyncUIHelper::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  if (!sync_manager_->IsEngineInitialized() ||
      !sync_manager_->GetUserSettings()->IsPassphraseRequiredForDecryption())
    return;

  for (const auto& result : results) {
    if (base::UTF16ToUTF8(result->username_value) ==
            sync_manager_->GetAuthenticatedAccountInfo().email &&
        sync_manager_->GetUserSettings()->SetDecryptionPassphrase(
            base::UTF16ToUTF8(result->password_value))) {
      profile_->GetPrefs()->SetInteger(
          vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword,
          static_cast<int>(
              vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::NAY));
      return;
    }
  }

  profile_->GetPrefs()->SetInteger(
      vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword,
      static_cast<int>(
          vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::AYE));
}

void VivaldiSyncUIHelper::OnSyncShutdown(syncer::SyncService* sync) {
  sync->RemoveObserver(this);
}

VivaldiSyncUIHelper::~VivaldiSyncUIHelper() {}

}  // namespace vivaldi