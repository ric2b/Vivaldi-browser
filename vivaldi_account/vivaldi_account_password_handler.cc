// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "vivaldi_account/vivaldi_account_password_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"

namespace {
constexpr char kSyncSignonRealm[] = "vivaldi-sync-login";
constexpr char kSyncOrigin[] = "vivaldi://settings/sync";
}  // namespace

namespace vivaldi {

VivaldiAccountPasswordHandler::VivaldiAccountPasswordHandler(Profile* profile,
                                                             Delegate* delegate)
    : delegate_(delegate),
      password_store_(PasswordStoreFactory::GetForProfile(
          profile,
          ServiceAccessType::IMPLICIT_ACCESS)) {

  UpdatePassword();
  password_store_->AddObserver(this);
}

VivaldiAccountPasswordHandler::~VivaldiAccountPasswordHandler() {}

void VivaldiAccountPasswordHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VivaldiAccountPasswordHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void VivaldiAccountPasswordHandler::SetPassword(const std::string& password) {
  DCHECK(!password.empty());

  autofill::PasswordForm password_form = {};
  password_form.scheme = autofill::PasswordForm::Scheme::kOther;
  password_form.signon_realm = kSyncSignonRealm;
  password_form.origin = GURL(kSyncOrigin);
  password_form.username_value = base::UTF8ToUTF16(delegate_->GetUsername());
  password_form.password_value = base::UTF8ToUTF16(password);
  password_form.date_created = base::Time::Now();

  password_store_->AddLogin(password_form);
}

void VivaldiAccountPasswordHandler::ForgetPassword() {
  autofill::PasswordForm password_form = {};
  password_form.scheme = autofill::PasswordForm::Scheme::kOther;
  password_form.signon_realm = kSyncSignonRealm;
  password_form.origin = GURL(kSyncOrigin);
  password_form.username_value = base::UTF8ToUTF16(delegate_->GetUsername());

  password_store_->RemoveLogin(password_form);
}

void VivaldiAccountPasswordHandler::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  for (const auto& result : results) {
    const std::string username = base::UTF16ToUTF8(result->username_value);
    if (username == delegate_->GetUsername())
      PasswordReceived(base::UTF16ToUTF8(result->password_value));
  }
}

void VivaldiAccountPasswordHandler::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  for (const auto& change : changes) {
    if (change.form().signon_realm == kSyncSignonRealm &&
        change.form().origin == GURL(kSyncOrigin) &&
        base::UTF16ToUTF8(change.form().username_value) ==
            delegate_->GetUsername()) {
      if (change.type() == password_manager::PasswordStoreChange::REMOVE)
        PasswordReceived(std::string());
      else
        UpdatePassword();
    }
  }
}

void VivaldiAccountPasswordHandler::UpdatePassword() {
  password_manager::PasswordStore::FormDigest form_digest(
      autofill::PasswordForm::Scheme::kOther, kSyncSignonRealm,
      GURL(kSyncOrigin));

  password_store_->GetLogins(form_digest, this);
}

void VivaldiAccountPasswordHandler::PasswordReceived(
    const std::string& password) {

  bool should_notfify = password.empty() || password_.empty();

  password_ = password;
  if (should_notfify) {
    for (auto& observer : observers_) {
      observer.OnAccountPasswordStateChanged();
    }
  }
}

}  // namespace vivaldi