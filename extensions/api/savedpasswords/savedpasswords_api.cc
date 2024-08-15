// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/savedpasswords/savedpasswords_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_ui.h"

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#include "extensions/schema/savedpasswords.h"
#include "password_list_sorter.h"
#include "ui/vivaldi_browser_window.h"

#if BUILDFLAG(IS_MAC)
#include "extraparts/vivaldi_keychain_util.h"
#endif

namespace extensions {

namespace {

void FilterAndSortPasswords(
    std::vector<std::unique_ptr<password_manager::PasswordForm>>*
        password_list) {
  password_list->erase(
      std::remove_if(password_list->begin(), password_list->end(),
                     [](const auto& form) { return form->blocked_by_user; }),
      password_list->end());

  extensions::SortEntriesAndHideDuplicates(password_list);
}

}  // namespace

SavedpasswordsGetListFunction::SavedpasswordsGetListFunction() {}

SavedpasswordsGetListFunction::~SavedpasswordsGetListFunction() {}

ExtensionFunction::ResponseAction SavedpasswordsGetListFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<password_manager::PasswordStoreInterface> password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile,
                                          ServiceAccessType::EXPLICIT_ACCESS);

  AddRef();  // Balanced in OnGetPasswordStoreResults
  password_store->GetAllLoginsWithAffiliationAndBrandingInformation(
      weak_ptr_factory_.GetWeakPtr());
  return RespondLater();
}

void SavedpasswordsGetListFunction::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>>
        password_list) {
  using vivaldi::savedpasswords::SavedPasswordItem;
  namespace Results = vivaldi::savedpasswords::GetList::Results;

  FilterAndSortPasswords(&password_list);

  std::vector<SavedPasswordItem> svd_pwd_entries;
  for (size_t i = 0; i < password_list.size(); ++i) {
    password_manager::PasswordForm* form = password_list[i].get();
    svd_pwd_entries.emplace_back();
    SavedPasswordItem* notes_tree_node = &svd_pwd_entries.back();
    notes_tree_node->username = base::UTF16ToUTF8(form->username_value);
    notes_tree_node->password = base::UTF16ToUTF8(form->password_value);
    notes_tree_node->origin =
        base::UTF16ToUTF8(url_formatter::FormatUrl(form->url));
    notes_tree_node->index = base::NumberToString(i);
  }

  Respond(ArgumentList(Results::Create(svd_pwd_entries)));
  Release();  // Balanced in Run().
}

SavedpasswordsRemoveFunction::SavedpasswordsRemoveFunction() {}

SavedpasswordsRemoveFunction::~SavedpasswordsRemoveFunction() {}

ExtensionFunction::ResponseAction SavedpasswordsRemoveFunction::Run() {
  using vivaldi::savedpasswords::Remove::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!base::StringToSizeT(params->id, &id_to_remove_)) {
    return RespondNow(Error("id is not a valid index - " + params->id));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  password_store_ = ProfilePasswordStoreFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);

  AddRef();  // Balanced in OnGetPasswordStoreResults
  password_store_->GetAllLoginsWithAffiliationAndBrandingInformation(
      weak_ptr_factory_.GetWeakPtr());
  return RespondLater();
}

void SavedpasswordsRemoveFunction::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>>
        password_list) {
  namespace Results = vivaldi::savedpasswords::Remove::Results;

  FilterAndSortPasswords(&password_list);
  if (id_to_remove_ >= password_list.size()) {
    Respond(Error("id is outside the allowed range"));
  } else {
    password_store_->RemoveLogin(FROM_HERE, *password_list[id_to_remove_]);
    Respond(ArgumentList(Results::Create()));
  }

  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction SavedpasswordsAddFunction::Run() {
  using vivaldi::savedpasswords::Add::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->password_form.password.has_value()) {
    return RespondNow(Error("No password"));
  }

#if BUILDFLAG(IS_MAC)
  if (!::vivaldi::HasKeychainAccess()) {
    return RespondNow(Error("No keychain access, unable to store password."));
  }
#endif

  password_manager::PasswordForm password_form = {};
  password_form.scheme = password_manager::PasswordForm::Scheme::kOther;
  password_form.signon_realm = params->password_form.signon_realm;
  password_form.url = GURL(params->password_form.origin);
  password_form.username_value =
      base::UTF8ToUTF16(params->password_form.username);
  password_form.password_value =
      base::UTF8ToUTF16(params->password_form.password.value());
  password_form.date_created = base::Time::Now();

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<password_manager::PasswordStoreInterface> password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile, params->is_explicit ? ServiceAccessType::EXPLICIT_ACCESS
                                       : ServiceAccessType::IMPLICIT_ACCESS);
  password_store->AddLogin(password_form);

  return RespondNow(NoArguments());
}

SavedpasswordsGetFunction::SavedpasswordsGetFunction() {}

SavedpasswordsGetFunction::~SavedpasswordsGetFunction() {}

ExtensionFunction::ResponseAction SavedpasswordsGetFunction::Run() {
  using vivaldi::savedpasswords::Get::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // EXPLICIT_ACCESS used as this is a read operation that must work in
  // incognito too.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<password_manager::PasswordStoreInterface> password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);

  username_ = params->password_form.username;

  password_manager::PasswordFormDigest form_digest(
      password_manager::PasswordForm::Scheme::kOther,
      params->password_form.signon_realm, GURL(params->password_form.origin));

  // Adding a ref on the behalf of the password store, which expects us to
  // remain alive
  AddRef();
  password_store->GetLogins(form_digest, weak_ptr_factory_.GetWeakPtr());

  return RespondLater();
}

void SavedpasswordsGetFunction::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> passwords) {
  namespace Results = vivaldi::savedpasswords::Get::Results;

  base::Value::List results;
  for (const auto& result : passwords) {
    if (base::UTF16ToUTF8(result->username_value) == username_) {
      results =
          Results::Create(true, base::UTF16ToUTF8(result->password_value));
    }
  }
  if (results.empty()) {
    results = Results::Create(false, "");
  }

  Respond(ArgumentList(std::move(results)));

  // Balance the AddRef in Run
  Release();
}

ExtensionFunction::ResponseAction SavedpasswordsCreateDelegateFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  //We only need to create the delegate once.
  //There is no process for deleting the delegate,
  // so once it's created it lives until browser shutdown.
  if (!extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(
          profile, false)) {
    scoped_refptr<extensions::PasswordsPrivateDelegate>*
        passwords_private_delegate;
    passwords_private_delegate =
        new scoped_refptr<extensions::PasswordsPrivateDelegate>;
    *passwords_private_delegate =
        extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(
            profile, true);
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SavedpasswordsDeleteFunction::Run() {
  using vivaldi::savedpasswords::Delete::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<password_manager::PasswordStoreInterface> password_store(
      ProfilePasswordStoreFactory::GetForProfile(
          profile, params->is_explicit ? ServiceAccessType::EXPLICIT_ACCESS
                                       : ServiceAccessType::IMPLICIT_ACCESS));
  if (!password_store.get()) {
    return RespondNow(Error("No such passwordstore for profile"));
  }

  password_manager::PasswordForm password_form = {};
  password_form.scheme = password_manager::PasswordForm::Scheme::kOther;
  password_form.signon_realm = params->password_form.signon_realm;
  password_form.url = GURL(params->password_form.origin);
  password_form.username_value =
      base::UTF8ToUTF16(params->password_form.username);

  password_store->RemoveLogin(FROM_HERE, password_form);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SavedpasswordsAuthenticateFunction::Run() {
  using vivaldi::savedpasswords::Authenticate::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params->window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }

  PasswordsPrivateDelegateFactory::GetForBrowserContext(
      window->web_contents()->GetBrowserContext(),
      /*create=*/true)
      ->AuthenticateUser(
          base::BindOnce(
              &SavedpasswordsAuthenticateFunction::AuthenticationComplete,
              this),
          window->web_contents());

  return RespondLater();
}

void SavedpasswordsAuthenticateFunction::AuthenticationComplete(
    bool authenticated) {
  namespace Results = vivaldi::savedpasswords::Authenticate::Results;
  Respond(ArgumentList(Results::Create(authenticated)));
}

}  // namespace extensions
