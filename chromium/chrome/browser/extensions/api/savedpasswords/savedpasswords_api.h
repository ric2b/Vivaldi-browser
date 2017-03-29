// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// See http://www.chromium.org/developers/design-documents/extensions/proposed-changes/creating-new-apis

#ifndef CHROME_BROWSER_EXTENSIONS_API_SAVEDPASSWORDS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SAVEDPASSWORDS_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/web_ui.h"

#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/common/extensions/api/savedpasswords.h"

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

  api::savedpasswords::SavedPasswordItem* GetSavedPasswordItem(autofill::PasswordForm *form, int id);

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
    const ScopedVector<autofill::PasswordForm>& password_list,
    bool show_passwords) override;
  void SetPasswordExceptionList(
    const ScopedVector<autofill::PasswordForm>& password_exception_list)
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
   api::savedpasswords::SavedPasswordItem* GetSavedPasswordItem(autofill::PasswordForm *form, int id);
   std::string languages_;
   PasswordManagerPresenter password_manager_presenter_;
   int64 idToRemove;
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
    const ScopedVector<autofill::PasswordForm>& password_list,
    bool show_passwords) override;
  void SetPasswordExceptionList(
    const ScopedVector<autofill::PasswordForm>& password_exception_list)
    override;
#if !defined(OS_ANDROID)
  gfx::NativeWindow GetNativeWindow() const override;
#endif

  DISALLOW_COPY_AND_ASSIGN(SavedpasswordsRemoveFunction);
};
}

#endif  // CHROME_BROWSER_EXTENSIONS_API_SAVEDPASSWORDS_API_H_
