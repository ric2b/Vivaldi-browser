// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_SAVEDPASSWORDS_SAVEDPASSWORDS_API_H_
#define EXTENSIONS_API_SAVEDPASSWORDS_SAVEDPASSWORDS_API_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "content/public/browser/web_ui.h"
#include "extensions/schema/savedpasswords.h"

namespace extensions {

class SavedpasswordsGetListFunction : public ChromeAsyncExtensionFunction,
                                      public PasswordUIView {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.getList", SAVEDPASSWORDS_GETLIST)
  SavedpasswordsGetListFunction();

 private:
  std::string languages_;

  vivaldi::savedpasswords::SavedPasswordItem* GetSavedPasswordItem(
      const std::unique_ptr<autofill::PasswordForm>& form,
      int id);

  ~SavedpasswordsGetListFunction() override;
  virtual void SendAsyncResponse();
  PasswordManagerPresenter password_manager_presenter_;

  // ExtensionFunction:
  bool RunAsync() override;
  virtual void SendResponseToCallback();

  // PasswordUIView implementation.
  Profile* GetProfile() override;
  void ShowPassword(size_t index,
                    const base::string16& password_value) override;
  void SetPasswordList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list)
      override;
  void SetPasswordExceptionList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&
          password_exception_list) override;
#if !defined(OS_ANDROID)
  gfx::NativeWindow GetNativeWindow() const override;
#endif

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsGetListFunction);
};

class SavedpasswordsRemoveFunction : public ChromeAsyncExtensionFunction,
                                     public PasswordUIView {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.remove", SAVEDPASSWORDS_REMOVE)
  SavedpasswordsRemoveFunction();

 private:
  vivaldi::savedpasswords::SavedPasswordItem* GetSavedPasswordItem(
      autofill::PasswordForm* form,
      int id);
  std::string languages_;
  PasswordManagerPresenter password_manager_presenter_;
  int64_t idToRemove;
  void SendResponseToCallback();
  void SendAsyncResponse();

  ~SavedpasswordsRemoveFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  // PasswordUIView implementation.
  Profile* GetProfile() override;
  void ShowPassword(size_t index,
                    const base::string16& password_value) override;
  void SetPasswordList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list)
      override;
  void SetPasswordExceptionList(
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&
          password_exception_list) override;
#if !defined(OS_ANDROID)
  gfx::NativeWindow GetNativeWindow() const override;
#endif

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsRemoveFunction);
};

class SavedpasswordsAddFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.add", SAVEDPASSWORDS_ADD)
  SavedpasswordsAddFunction() = default;

 private:
  ~SavedpasswordsAddFunction() override = default;
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsAddFunction);
};

class SavedpasswordsGetFunction
    : public ChromeAsyncExtensionFunction,
      public password_manager::PasswordStoreConsumer {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.get", SAVEDPASSWORDS_GET)
  SavedpasswordsGetFunction() = default;

 private:
  ~SavedpasswordsGetFunction() override = default;
  bool RunAsync() override;

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  std::string username_;

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsGetFunction);
};

class SavedpasswordsDeleteFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.delete", SAVEDPASSWORDS_DELETE)
  SavedpasswordsDeleteFunction() = default;

 private:
  ~SavedpasswordsDeleteFunction() override = default;
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsDeleteFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SAVEDPASSWORDS_SAVEDPASSWORDS_API_H_
