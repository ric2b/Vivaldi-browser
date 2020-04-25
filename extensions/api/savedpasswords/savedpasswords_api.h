// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_SAVEDPASSWORDS_SAVEDPASSWORDS_API_H_
#define EXTENSIONS_API_SAVEDPASSWORDS_SAVEDPASSWORDS_API_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view.h"
#include "chrome/browser/ui/passwords/settings/password_manager_presenter.h"
#include "chrome/browser/password_manager/reauth_purpose.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/savedpasswords.h"

namespace extensions {

class SavedpasswordsGetListFunction
    : public UIThreadExtensionFunction,
      public password_manager::PasswordStoreConsumer {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.getList", SAVEDPASSWORDS_GETLIST)
  SavedpasswordsGetListFunction() = default;

 private:
  ~SavedpasswordsGetListFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

  // PasswordStoreConsumer
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsGetListFunction);
};

class SavedpasswordsRemoveFunction
    : public UIThreadExtensionFunction,
      public password_manager::PasswordStoreConsumer {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.remove", SAVEDPASSWORDS_REMOVE)
  SavedpasswordsRemoveFunction();

 private:
  ~SavedpasswordsRemoveFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // PasswordStoreConsumer
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  size_t id_to_remove_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsRemoveFunction);
};

class SavedpasswordsAddFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.add", SAVEDPASSWORDS_ADD)
  SavedpasswordsAddFunction() = default;

 private:
  ~SavedpasswordsAddFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsAddFunction);
};

class SavedpasswordsGetFunction
    : public UIThreadExtensionFunction,
      public password_manager::PasswordStoreConsumer {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.get", SAVEDPASSWORDS_GET)
  SavedpasswordsGetFunction() = default;

 private:
  ~SavedpasswordsGetFunction() override = default;

  ResponseAction Run() override;

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  std::string username_;

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsGetFunction);
};

class SavedpasswordsDeleteFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.delete", SAVEDPASSWORDS_DELETE)
  SavedpasswordsDeleteFunction() = default;

 private:
  ~SavedpasswordsDeleteFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsDeleteFunction);
};

class SavedpasswordsAuthenticateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.authenticate",
                             SAVEDPASSWORDS_AUTHENTICATE)
  SavedpasswordsAuthenticateFunction() = default;

 private:
  ~SavedpasswordsAuthenticateFunction() override = default;

  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsAuthenticateFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SAVEDPASSWORDS_SAVEDPASSWORDS_API_H_
