// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/savedpasswords/savedpasswords_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/language/core/browser/pref_names.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_ui.h"
#include "extensions/schema/savedpasswords.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

namespace extensions {

namespace {

void FilterAndSortPasswords(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* password_list) {
  password_list->erase(
      std::remove_if(
          password_list->begin(), password_list->end(),
          [](const auto& form) { return form->blacklisted_by_user; }),
      password_list->end());

  password_manager::DuplicatesMap ignored_duplicates;
  password_manager::SortEntriesAndHideDuplicates(password_list,
                                                 &ignored_duplicates);
}

}  // namespace

ExtensionFunction::ResponseAction SavedpasswordsGetListFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS);

  AddRef();  // Balanced in OnGetPasswordStoreResults
  password_store->GetAllLoginsWithAffiliationAndBrandingInformation(this);
  return RespondLater();
}

void SavedpasswordsGetListFunction::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> password_list) {
  using vivaldi::savedpasswords::SavedPasswordItem;
  namespace Results = vivaldi::savedpasswords::GetList::Results;

  FilterAndSortPasswords(&password_list);

  std::vector<SavedPasswordItem> svd_pwd_entries;
  for (size_t i = 0; i < password_list.size(); ++i) {
    autofill::PasswordForm* form = password_list[i].get();
    svd_pwd_entries.emplace_back();
    SavedPasswordItem* notes_tree_node = &svd_pwd_entries.back();
    notes_tree_node->username = base::UTF16ToUTF8(form->username_value);
    notes_tree_node->password = base::UTF16ToUTF8(form->password_value);
    notes_tree_node->origin =
        base::UTF16ToUTF8(url_formatter::FormatUrl(form->origin));
    notes_tree_node->id = base::NumberToString(i);
  }

  Respond(ArgumentList(Results::Create(svd_pwd_entries)));
  Release();  // Balanced in Run().
}

SavedpasswordsRemoveFunction::SavedpasswordsRemoveFunction() {}

SavedpasswordsRemoveFunction::~SavedpasswordsRemoveFunction() {}

ExtensionFunction::ResponseAction SavedpasswordsRemoveFunction::Run() {
  using vivaldi::savedpasswords::Remove::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!base::StringToSizeT(params->id, &id_to_remove_)) {
    return RespondNow(Error("id is not a valid index - " + params->id));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  password_store_ = PasswordStoreFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);

  AddRef();  // Balanced in OnGetPasswordStoreResults
  password_store_->GetAllLoginsWithAffiliationAndBrandingInformation(this);
  return RespondLater();
}

void SavedpasswordsRemoveFunction::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> password_list) {
  namespace Results = vivaldi::savedpasswords::Remove::Results;

  FilterAndSortPasswords(&password_list);
  if (id_to_remove_ >= password_list.size()) {
    Respond(Error("id is outside the allowed range"));
  } else {
    password_store_->RemoveLogin(*password_list[id_to_remove_]);
    Respond(ArgumentList(Results::Create()));
  }

  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction SavedpasswordsAddFunction::Run() {
  using vivaldi::savedpasswords::Add::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->password_form.password.get()) {
    return RespondNow(Error("No password"));
  }

  autofill::PasswordForm password_form = {};
  password_form.scheme = autofill::PasswordForm::Scheme::kOther;
  password_form.signon_realm = params->password_form.signon_realm;
  password_form.origin = GURL(params->password_form.origin);
  password_form.username_value =
      base::UTF8ToUTF16(params->password_form.username);
  password_form.password_value =
      base::UTF8ToUTF16(*params->password_form.password.get());
  password_form.date_created = base::Time::Now();

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(
          profile, params->is_explicit ? ServiceAccessType::EXPLICIT_ACCESS
                                       : ServiceAccessType::IMPLICIT_ACCESS);
  password_store->AddLogin(password_form);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SavedpasswordsGetFunction::Run() {
  using vivaldi::savedpasswords::Get::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(
          profile, params->is_explicit ? ServiceAccessType::EXPLICIT_ACCESS
                                       : ServiceAccessType::IMPLICIT_ACCESS);

  username_ = params->password_form.username;

  password_manager::PasswordStore::FormDigest form_digest(
      autofill::PasswordForm::Scheme::kOther,
      params->password_form.signon_realm, GURL(params->password_form.origin));

  // Adding a ref on the behalf of the password store, which expects us to
  // remain alive
  AddRef();
  password_store->GetLogins(form_digest, this);

  return RespondLater();
}

void SavedpasswordsGetFunction::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> passwords) {
  namespace Results = vivaldi::savedpasswords::Get::Results;

  std::unique_ptr<base::ListValue> results;
  for (const auto& result : passwords) {
    if (base::UTF16ToUTF8(result->username_value) == username_) {
      results =
          Results::Create(true, base::UTF16ToASCII(result->password_value));
    }
  }
  if (!results) {
    results = Results::Create(false, "");
  }

  Respond(ArgumentList(std::move(results)));

  // Balance the AddRef in Run
  Release();
}

ExtensionFunction::ResponseAction SavedpasswordsDeleteFunction::Run() {
  using vivaldi::savedpasswords::Delete::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<password_manager::PasswordStore> password_store(
      PasswordStoreFactory::GetForProfile(
          profile, params->is_explicit ? ServiceAccessType::EXPLICIT_ACCESS
                                       : ServiceAccessType::IMPLICIT_ACCESS));

  autofill::PasswordForm password_form = {};
  password_form.scheme = autofill::PasswordForm::Scheme::kOther;
  password_form.signon_realm = params->password_form.signon_realm;
  password_form.origin = GURL(params->password_form.origin);
  password_form.username_value =
      base::UTF8ToUTF16(params->password_form.username);

  password_store->RemoveLogin(password_form);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SavedpasswordsAuthenticateFunction::Run() {
  namespace Results = vivaldi::savedpasswords::Authenticate::Results;

  bool success = VivaldiUtilitiesAPI::AuthenticateUser(
      browser_context(), dispatcher()->GetAssociatedWebContents());

  return RespondNow(ArgumentList(Results::Create(success)));
}

}  // namespace extensions
