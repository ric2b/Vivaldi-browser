// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/savedpasswords/savedpasswords_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_ui.h"
#include "extensions/schema/savedpasswords.h"

namespace extensions {
namespace passwords = vivaldi::savedpasswords;
using passwords::SavedPasswordItem;

SavedpasswordsGetListFunction::SavedpasswordsGetListFunction()
    : password_manager_presenter_(this) {}

bool SavedpasswordsGetListFunction::RunAsync() {
  AddRef();
  password_manager_presenter_.Initialize();
  password_manager_presenter_.UpdatePasswordLists();
  return true;
}

SavedpasswordsGetListFunction::~SavedpasswordsGetListFunction() {
  Respond(ArgumentList(std::move(results_)));
}

Profile* SavedpasswordsGetListFunction::GetProfile() {
  return Profile::FromBrowserContext(browser_context());
}

#if !defined(OS_ANDROID)
gfx::NativeWindow SavedpasswordsGetListFunction::GetNativeWindow() const {
  return NULL;
}
#endif

void SavedpasswordsGetListFunction::SetPasswordList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list) {
  std::vector<SavedPasswordItem> svd_pwd_entries;
  base::ListValue entries;
  languages_ = GetProfile()->GetPrefs()->GetString(prefs::kAcceptLanguages);

  for (size_t i = 0; i < password_list.size(); ++i) {
    std::unique_ptr<SavedPasswordItem> new_node(
        GetSavedPasswordItem(password_list[i], i));
    svd_pwd_entries.push_back(std::move(*new_node));
  }

  results_ = vivaldi::savedpasswords::GetList::Results::Create(svd_pwd_entries);
  SendAsyncResponse();
}

SavedPasswordItem* SavedpasswordsGetListFunction::GetSavedPasswordItem(
    const std::unique_ptr<autofill::PasswordForm>& form,
    int id) {
  SavedPasswordItem* notes_tree_node = new SavedPasswordItem();
  notes_tree_node->username = base::UTF16ToUTF8(form->username_value);
  notes_tree_node->origin =
      base::UTF16ToUTF8(url_formatter::FormatUrl(form->origin));
  notes_tree_node->id = base::Int64ToString(id);

  return notes_tree_node;
}

void SavedpasswordsGetListFunction::SendAsyncResponse() {
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SavedpasswordsGetListFunction::SendResponseToCallback, this));
}

void SavedpasswordsGetListFunction::SendResponseToCallback() {
  Release();  // Balanced in RunAsync().
}

void SavedpasswordsGetListFunction::SetPasswordExceptionList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>&
        password_exception_list) {}

void SavedpasswordsGetListFunction::ShowPassword(
    size_t index,
    const base::string16& password_value) {}

SavedpasswordsRemoveFunction::SavedpasswordsRemoveFunction()
    : password_manager_presenter_(this) {}

SavedpasswordsRemoveFunction::~SavedpasswordsRemoveFunction() {}

bool SavedpasswordsRemoveFunction::RunAsync() {
  AddRef();  // Balanced in SendResponseToCallback
  password_manager_presenter_.Initialize();
  password_manager_presenter_.UpdatePasswordLists();

  std::unique_ptr<passwords::Remove::Params> params(
      passwords::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::StringToInt64(params->id, &idToRemove);
  return true;
}

Profile* SavedpasswordsRemoveFunction::GetProfile() {
  return Profile::FromBrowserContext(browser_context());
}
void SavedpasswordsRemoveFunction::ShowPassword(
    size_t index,
    const base::string16& password_value) {}

void SavedpasswordsRemoveFunction::SetPasswordList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list) {
  password_manager_presenter_.RemoveSavedPassword(
      static_cast<size_t>(idToRemove));

  results_ = passwords::Remove::Results::Create();
  SendAsyncResponse();
}
void SavedpasswordsRemoveFunction::SetPasswordExceptionList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>&
        password_exception_list) {}

#if !defined(OS_ANDROID)
gfx::NativeWindow SavedpasswordsRemoveFunction::GetNativeWindow() const {
  return NULL;
}
#endif

void SavedpasswordsRemoveFunction::SendResponseToCallback() {
  SendResponse(true);
  Release();  // Balanced in RunAsync().
}

void SavedpasswordsRemoveFunction::SendAsyncResponse() {
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SavedpasswordsRemoveFunction::SendResponseToCallback, this));
}

bool SavedpasswordsAddFunction::RunAsync() {
  std::unique_ptr<passwords::Add::Params> params(
      passwords::Add::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<password_manager::PasswordStore> password_store(
      PasswordStoreFactory::GetForProfile(
          GetProfile(), params->is_explicit
                            ? ServiceAccessType::EXPLICIT_ACCESS
                            : ServiceAccessType::IMPLICIT_ACCESS));

  if (!params->password_form.password.get()) {
    SendResponse(false);
    return true;
  }

  autofill::PasswordForm password_form = {};
  password_form.scheme = autofill::PasswordForm::SCHEME_OTHER;
  password_form.signon_realm = params->password_form.signon_realm;
  password_form.origin = GURL(params->password_form.origin);
  password_form.username_value =
      base::UTF8ToUTF16(params->password_form.username);
  password_form.password_value =
      base::UTF8ToUTF16(*params->password_form.password.get());
  password_form.date_created = base::Time::Now();

  password_store->AddLogin(password_form);

  SendResponse(true);
  return true;
}

bool SavedpasswordsGetFunction::RunAsync() {
  std::unique_ptr<passwords::Get::Params> params(
      passwords::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<password_manager::PasswordStore> password_store(
      PasswordStoreFactory::GetForProfile(
          GetProfile(), params->is_explicit
                            ? ServiceAccessType::EXPLICIT_ACCESS
                            : ServiceAccessType::IMPLICIT_ACCESS));

  username_ = params->password_form.username;

  password_manager::PasswordStore::FormDigest form_digest(
      autofill::PasswordForm::SCHEME_OTHER, params->password_form.signon_realm,
      GURL(params->password_form.origin));

  // Adding a ref on the behalf of the password store, which expects us to
  // remain alive
  AddRef();
  password_store->GetLogins(form_digest, this);

  return true;
}

void SavedpasswordsGetFunction::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  results_ = passwords::Get::Results::Create(false, "");
  for (const auto& result : results) {
    if (base::UTF16ToUTF8(result->username_value) == username_) {
      results_ = passwords::Get::Results::Create(
          true, base::UTF16ToASCII(result->password_value));
    }
  }

  SendResponse(true);

  // Balance the AddRef in RunAsync
  Release();
}

bool SavedpasswordsDeleteFunction::RunAsync() {
  std::unique_ptr<passwords::Delete::Params> params(
      passwords::Delete::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<password_manager::PasswordStore> password_store(
      PasswordStoreFactory::GetForProfile(
          GetProfile(), params->is_explicit
                            ? ServiceAccessType::EXPLICIT_ACCESS
                            : ServiceAccessType::IMPLICIT_ACCESS));

  autofill::PasswordForm password_form = {};
  password_form.scheme = autofill::PasswordForm::SCHEME_OTHER;
  password_form.signon_realm = params->password_form.signon_realm;
  password_form.origin = GURL(params->password_form.origin);
  password_form.username_value =
      base::UTF8ToUTF16(params->password_form.username);

  password_store->RemoveLogin(password_form);

  SendResponse(true);
  return true;
}

}  // namespace extensions
