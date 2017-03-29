// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/extensions/api/settings/settings_api.h"

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings.h"
#include "content/public/browser/web_ui.h"
#include "chrome/common/pref_names.h"
#include "net/base/net_util.h"
#include "base/strings/string_number_conversions.h"

namespace extensions {

using api::settings::PreferenceItem;

SettingsTogglePreferenceFunction::~SettingsTogglePreferenceFunction() {
}

SettingsTogglePreferenceFunction::SettingsTogglePreferenceFunction(){
}

bool SettingsTogglePreferenceFunction::RunAsync() {
  scoped_ptr<api::settings::TogglePreference::Params> params(
    api::settings::TogglePreference::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = GetProfile();
  std::string pref = params->value;
  bool currentValue = profile->GetPrefs()->GetBoolean(pref.c_str() );

  profile->GetPrefs()->SetBoolean( pref.c_str(), !currentValue);
  results_ = api::settings::TogglePreference::Results::Create(!currentValue);
  SendResponse(true);
  return true;
}

SettingsGetPreferenceFunction::~SettingsGetPreferenceFunction() {
}

SettingsGetPreferenceFunction::SettingsGetPreferenceFunction(){
}

linked_ptr<api::settings::PreferenceItem>
SettingsGetPreferenceFunction::GetPref(const char* prefName, PrefType type) {
  Profile* profile = GetProfile();
  api::settings::PreferenceItem * prefItem = new api::settings::PreferenceItem;

  switch (type) {
    case booleanPreftype: {
      prefItem->preference_key = prefName;
      prefItem->preference_value.boolean.reset(
          new bool(profile->GetPrefs()->GetBoolean(prefName)));
      prefItem->preference_type = api::settings::PREFERENCE_TYPE_ENUM_BOOLEAN;
    } break;
    case stringPreftype: {
      prefItem->preference_key = prefName;
      prefItem->preference_value.string.reset(
          new std::string(profile->GetPrefs()->GetString(prefName)));
      prefItem->preference_type = api::settings::PREFERENCE_TYPE_ENUM_STRING;
    } break;
    case numberPreftype: {
      prefItem->preference_key = prefName;
      prefItem->preference_value.number.reset(
          new double(profile->GetPrefs()->GetDouble(prefName)));
      prefItem->preference_type = api::settings::PREFERENCE_TYPE_ENUM_NUMBER;
    } break;
    case arrayPreftype: {
      const base::ListValue* url_list = profile->GetPrefs()->GetList(prefName);
      prefItem->preference_key = prefName;
      prefItem->preference_value.array.reset(new std::vector<std::string>());
      for (size_t i = 0; i < url_list->GetSize(); ++i) {
        std::string url_text;
        if (url_list->GetString(i, &url_text)) {
          prefItem->preference_value.array->push_back(url_text);
        }
      }
      prefItem->preference_type = api::settings::PREFERENCE_TYPE_ENUM_ARRAY;
    } break;
  };

  linked_ptr<api::settings::PreferenceItem> new_node(prefItem);
  return new_node;
}

bool SettingsGetPreferenceFunction::RunAsync() {
  std::vector<linked_ptr<api::settings::PreferenceItem> > nodes;
  nodes.push_back(GetPref(prefs::kAlternateErrorPagesEnabled, booleanPreftype));
  nodes.push_back(
      GetPref(prefs::kSafeBrowsingExtendedReportingEnabled, booleanPreftype));
  nodes.push_back(GetPref(prefs::kSafeBrowsingEnabled, booleanPreftype));
  nodes.push_back(GetPref(prefs::kEnableDoNotTrack, booleanPreftype));
  nodes.push_back(GetPref(prefs::kSearchSuggestEnabled, booleanPreftype));

  // Download preferences
  nodes.push_back(GetPref(prefs::kDownloadDefaultDirectory, stringPreftype));

  //vivaldi preferences
  nodes.push_back(GetPref(prefs::kMousegesturesEnabled, booleanPreftype));
  nodes.push_back(GetPref(prefs::kSmoothScrollingEnabled, booleanPreftype));

  // Startup preferences:
  // An integer pref. Holds one of several values:
  // 0: (deprecated) open the homepage on startup.
  // 1: restore the last session.
  // 2: this was used to indicate a specific session should be restored. It is
  //    no longer used, but saved to avoid conflict with old preferences.
  // 3: unused, previously indicated the user wants to restore a saved session.
  // 4: restore the URLs defined in kURLsToRestoreOnStartup.
  // 5: open the New Tab Page on startup.
  //
  // Mostly the same as chrome.importData.getStartupAction().
  nodes.push_back(GetPref(prefs::kRestoreOnStartup, numberPreftype));
  nodes.push_back(GetPref(prefs::kURLsToRestoreOnStartup, arrayPreftype));

  results_ = api::settings::GetPreference::Results::Create(nodes);
  SendResponse(true);
  return true;
}


SettingsSetPreferenceFunction::SettingsSetPreferenceFunction() {}

SettingsSetPreferenceFunction::~SettingsSetPreferenceFunction() {}

bool SettingsSetPreferenceFunction::RunAsync() {
  scoped_ptr<api::settings::SetPreference::Params> params(
      api::settings::SetPreference::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool setResult = SetPref(&(params->preference));
  SendResponse(setResult);
  return setResult;
}

bool SettingsSetPreferenceFunction::SetPref(
    const api::settings::PreferenceItem* item) {
  Profile* profile = GetProfile();
  PrefService* prefs = profile->GetPrefs();
  bool status = false;
  switch (item->preference_type) {
    case api::settings::PREFERENCE_TYPE_ENUM_BOOLEAN: {
      if (item->preference_value.boolean) {
        prefs->SetBoolean(item->preference_key.c_str(),
                          *item->preference_value.boolean);
        status = true;
      } else
        status = false;
    } break;
    case api::settings::PREFERENCE_TYPE_ENUM_STRING: {
      if (item->preference_value.string) {
        prefs->SetString(item->preference_key.c_str(),
                         *item->preference_value.string);
        status = true;
      } else
        status = false;
    } break;
    case api::settings::PREFERENCE_TYPE_ENUM_NUMBER: {
      if (item->preference_value.number) {
        prefs->SetDouble(item->preference_key.c_str(),
                         *item->preference_value.number);
        status = true;
      } else
        status = false;
    } break;
    case api::settings::PREFERENCE_TYPE_ENUM_ARRAY: {
      if (item->preference_value.array) {
        base::ListValue new_url_pref_list;
        new_url_pref_list.AppendStrings(*item->preference_value.array);
        prefs->Set(prefs::kURLsToRestoreOnStartup, new_url_pref_list);
        status = true;
      } else
        status = false;
    } break;
    case api::settings::PREFERENCE_TYPE_ENUM_NONE: {
      status = false;
    } break;
  };

  return status;
}

}  // namespace extensions
