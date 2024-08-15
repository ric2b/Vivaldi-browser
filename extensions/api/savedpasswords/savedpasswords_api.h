// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_SAVEDPASSWORDS_SAVEDPASSWORDS_API_H_
#define EXTENSIONS_API_SAVEDPASSWORDS_SAVEDPASSWORDS_API_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/savedpasswords.h"

namespace extensions {

class SavedpasswordsGetListFunction
    : public ExtensionFunction,
      public password_manager::PasswordStoreConsumer {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.getList", SAVEDPASSWORDS_GETLIST)
  SavedpasswordsGetListFunction();

 private:
  ~SavedpasswordsGetListFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // PasswordStoreConsumer
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  base::WeakPtrFactory<SavedpasswordsGetListFunction> weak_ptr_factory_{this};
};

class SavedpasswordsRemoveFunction
    : public ExtensionFunction,
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
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  size_t id_to_remove_;
  scoped_refptr<password_manager::PasswordStoreInterface> password_store_;

  base::WeakPtrFactory<SavedpasswordsRemoveFunction> weak_ptr_factory_{this};
};

class SavedpasswordsAddFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.add", SAVEDPASSWORDS_ADD)
  SavedpasswordsAddFunction() = default;

 private:
  ~SavedpasswordsAddFunction() override = default;

  ResponseAction Run() override;
};

class SavedpasswordsGetFunction
    : public ExtensionFunction,
      public password_manager::PasswordStoreConsumer {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.get", SAVEDPASSWORDS_GET)
  SavedpasswordsGetFunction();

 private:
  ~SavedpasswordsGetFunction() override;

  ResponseAction Run() override;

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  std::string username_;

  base::WeakPtrFactory<SavedpasswordsGetFunction> weak_ptr_factory_{this};
};

class SavedpasswordsCreateDelegateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.createDelegate", SAVEDPASSWORDS_CREATEDELEGATE)
  SavedpasswordsCreateDelegateFunction() = default;

 private:
  ~SavedpasswordsCreateDelegateFunction() override = default;

  ResponseAction Run() override;
};

class SavedpasswordsDeleteFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.delete", SAVEDPASSWORDS_DELETE)
  SavedpasswordsDeleteFunction() = default;

 private:
  ~SavedpasswordsDeleteFunction() override = default;

  ResponseAction Run() override;
};

class SavedpasswordsAuthenticateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.authenticate",
                             SAVEDPASSWORDS_AUTHENTICATE)
  SavedpasswordsAuthenticateFunction() = default;

 private:
  ~SavedpasswordsAuthenticateFunction() override = default;

  ResponseAction Run() override;

  void AuthenticationComplete(bool authenticated);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SAVEDPASSWORDS_SAVEDPASSWORDS_API_H_
