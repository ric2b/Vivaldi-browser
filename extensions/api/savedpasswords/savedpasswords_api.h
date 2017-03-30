// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_SAVEDPASSWORDS_API_H_
#define EXTENSIONS_API_SAVEDPASSWORDS_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/web_ui.h"

#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "extensions/schema/savedpasswords.h"

namespace extensions {

class SavedpasswordsGetListFunction :
                                    public ChromeAsyncExtensionFunction,
                                    public PasswordUIView
{
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.getList",
                                                    SAVEDPASSWORDS_GETLIST)
  SavedpasswordsGetListFunction();

 private:
  std::string languages_;

  vivaldi::savedpasswords::SavedPasswordItem*
  GetSavedPasswordItem(const std::unique_ptr<autofill::PasswordForm> &form, int id);

 protected:
   ~SavedpasswordsGetListFunction() override;
   virtual void SendAsyncResponse();
   PasswordManagerPresenter password_manager_presenter_;

  // ExtensionFunction:
  bool RunAsync() override;
  virtual void SendResponseToCallback();

  // PasswordUIView implementation.
  Profile* GetProfile() override;
  void ShowPassword(size_t index,
                    const std::string& origin_url,
                    const std::string& username,
                    const base::string16& password_value)
    override;
  void SetPasswordList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list
    ) override;
  void SetPasswordExceptionList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>&
          password_exception_list)
    override;
#if !defined(OS_ANDROID)
  gfx::NativeWindow GetNativeWindow() const override;
#endif

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsGetListFunction);
};


class SavedpasswordsRemoveFunction :
                          public ChromeAsyncExtensionFunction,
                          public PasswordUIView
{
 public:
  DECLARE_EXTENSION_FUNCTION("savedpasswords.remove",
                              SAVEDPASSWORDS_REMOVE)
  SavedpasswordsRemoveFunction();

 private:
   vivaldi::savedpasswords::SavedPasswordItem* GetSavedPasswordItem(
            autofill::PasswordForm *form, int id);
   std::string languages_;
   PasswordManagerPresenter password_manager_presenter_;
   int64_t idToRemove;
   void SendResponseToCallback();
   void SendAsyncResponse();

 protected:
   ~SavedpasswordsRemoveFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  // PasswordUIView implementation.
  Profile* GetProfile() override;
  void ShowPassword(size_t index,
                    const std::string& origin_url,
                    const std::string& username,
                    const base::string16& password_value)
    override;
  void SetPasswordList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list
    ) override;
  void SetPasswordExceptionList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>&
            password_exception_list)
    override;
#if !defined(OS_ANDROID)
  gfx::NativeWindow GetNativeWindow() const override;
#endif

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsRemoveFunction);
};
}

#endif  // EXTENSIONS_API_SAVEDPASSWORDS_API_H_
