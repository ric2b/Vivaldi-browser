// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/savedpasswords/savedpasswords_api.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/url_formatter/url_formatter.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "extensions/schema/savedpasswords.h"

namespace extensions {
namespace passwords = vivaldi::savedpasswords;
using passwords::SavedPasswordItem;

SavedpasswordsGetListFunction::SavedpasswordsGetListFunction()
  : password_manager_presenter_(this)
{
}

bool SavedpasswordsGetListFunction::RunAsync() {
  AddRef();
  password_manager_presenter_.Initialize();
  password_manager_presenter_.UpdatePasswordLists();
  return true;
}

SavedpasswordsGetListFunction::~SavedpasswordsGetListFunction() {
}

Profile* SavedpasswordsGetListFunction::GetProfile() {
 return ChromeUIThreadExtensionFunction::GetProfile();
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
    scoped_ptr<SavedPasswordItem> new_node(
          GetSavedPasswordItem(password_list[i], i));
    svd_pwd_entries.push_back(std::move(*new_node));
  }

  results_ = vivaldi::savedpasswords::GetList::Results::Create(svd_pwd_entries);
  SendAsyncResponse();
}

SavedPasswordItem* SavedpasswordsGetListFunction::GetSavedPasswordItem(
        const scoped_ptr<autofill::PasswordForm> &form, int id
      ){
  SavedPasswordItem* notes_tree_node = new SavedPasswordItem();
  notes_tree_node->username =  base::UTF16ToUTF8(form->username_value);
  notes_tree_node->origin = base::UTF16ToUTF8(
        url_formatter::FormatUrl(form->origin));
  notes_tree_node->id = base::Int64ToString(id);

  return notes_tree_node;
}

void SavedpasswordsGetListFunction::SendAsyncResponse() {
  base::MessageLoop::current()->PostTask(
    FROM_HERE,
    base::Bind(&SavedpasswordsGetListFunction::SendResponseToCallback, this));
}

void SavedpasswordsGetListFunction::SendResponseToCallback() {
  SendResponse(true);
  Release();  // Balanced in RunAsync().
}


void SavedpasswordsGetListFunction::SetPasswordExceptionList(
  const std::vector<scoped_ptr<autofill::PasswordForm>>&
          password_exception_list) {
}

void SavedpasswordsGetListFunction::ShowPassword(
  size_t index,
  const std::string& origin_url,
  const std::string& username,
  const base::string16& password_value) {
}


SavedpasswordsRemoveFunction::SavedpasswordsRemoveFunction():
password_manager_presenter_(this)
{
}


SavedpasswordsRemoveFunction::~SavedpasswordsRemoveFunction(){
}

bool SavedpasswordsRemoveFunction::RunAsync(){
  AddRef(); //Balanced in SendResponseToCallback
  password_manager_presenter_.Initialize();
  password_manager_presenter_.UpdatePasswordLists();

  scoped_ptr<passwords::Remove::Params> params(
    passwords::Remove::Params::Create(*args_));
    EXTENSION_FUNCTION_VALIDATE(params.get());
    base::StringToInt64(params->id, &idToRemove);
  return true;
}

Profile* SavedpasswordsRemoveFunction::GetProfile(){
  return ChromeUIThreadExtensionFunction::GetProfile();
}
void SavedpasswordsRemoveFunction::ShowPassword(size_t index,
      const std::string& origin_url,
      const std::string& username,
      const base::string16& password_value){
}

void SavedpasswordsRemoveFunction::SetPasswordList(
  const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list){

  password_manager_presenter_.RemoveSavedPassword(
          static_cast<size_t>(idToRemove));

  results_ = passwords::Remove::Results::Create();
  SendAsyncResponse();
}
void SavedpasswordsRemoveFunction::SetPasswordExceptionList(
  const std::vector<std::unique_ptr<autofill::PasswordForm>>&
            password_exception_list){}

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
  base::MessageLoop::current()->PostTask(
    FROM_HERE,
    base::Bind(&SavedpasswordsRemoveFunction::SendResponseToCallback, this));
}

}  // namespace extensions
