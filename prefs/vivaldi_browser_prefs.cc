// Copyright 2017 Vivaldi Technologies

#include "prefs/vivaldi_browser_prefs.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "apps/switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

void RegisterOldPlatformPrefs(user_prefs::PrefRegistrySyncable* registry);
void MigrateOldPlatformPrefs(PrefService* prefs);
std::string GetPlatformDefaultKey();
std::unique_ptr<base::Value> GetPlatformComputedDefault(
    const std::string& path);

const base::FilePath::CharType kVivaldiResourcesFolder[] =
    FILE_PATH_LITERAL("vivaldi");
const base::FilePath::CharType kPrefsDefinitionFileName[] =
    FILE_PATH_LITERAL("prefs_definitions.json");

const char kVivaldiKeyName[] = "vivaldi";
const char kChromiumKeyName[] = "chromium";
const char kChromiumLocalKeyName[] = "chromium_local";

const char kTypeKeyName[] = "type";
const char kDefaultKeyName[] = "default";
const char kSyncableKeyName[] = "syncable";
const char kEnumValuesKey[] = "enum_values";

const char kTypeEnumName[] = "enum";
const char kTypeStringName[] = "string";
const char kTypeFilePathName[] = "file_path";
const char kTypeBooleanName[] = "boolean";
const char kTypeIntegerName[] = "integer";
const char kTypeDoubleName[] = "double";
const char kTypeListName[] = "list";
const char kTypeDictionaryName[] = "dictionary";

PrefProperties::PrefProperties(
    bool local_pref,
    std::unordered_map<std::string, int> string_to_value,
    std::unordered_map<int, std::string> value_to_string)
    : local_pref(local_pref) {
  enum_properties.reset(new EnumProperties);
  enum_properties->value_to_string = std::move(value_to_string);
  enum_properties->string_to_value = std::move(string_to_value);
}

PrefProperties::PrefProperties(bool local_pref) : local_pref(local_pref) {}
PrefProperties::PrefProperties(PrefProperties&&) = default;
PrefProperties::~PrefProperties() = default;

PrefProperties::EnumProperties::EnumProperties() = default;
PrefProperties::EnumProperties::EnumProperties(EnumProperties&&) = default;
PrefProperties::EnumProperties::~EnumProperties() = default;

void RegisterLocalState(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(vivaldiprefs::kVivaldiUniqueUserId, "");
}

std::unique_ptr<base::Value> GetComputedDefault(const std::string& path) {
  if (path == vivaldiprefs::kWebpagesCaptureDirectory) {
    base::FilePath path;
    base::PathService::Get(chrome::DIR_USER_PICTURES, &path);
    path = path.AppendASCII("Vivaldi Captures");
    return std::make_unique<base::Value>(path.value());
  }
  return GetPlatformComputedDefault(path);
}

void RegisterPrefsFromDefinition(const base::DictionaryValue* definition,
                                 const std::string& current_path,
                                 user_prefs::PrefRegistrySyncable* registry,
                                 PrefsProperties* prefs_properties) {
  if (!definition->HasKey(kTypeKeyName)) {
    for (const auto& child : *definition) {
      base::DictionaryValue* child_dict;
      if (!child.second->GetAsDictionary(&child_dict)) {
        LOG(DFATAL) << "Expected a dictionary at '" << current_path << "'";
        break;
      }
      RegisterPrefsFromDefinition(child_dict, current_path + "." + child.first,
                                  registry, prefs_properties);
    }

    return;
  }

  const base::Value* syncable =
      definition->FindKeyOfType(kSyncableKeyName, base::Value::Type::BOOLEAN);
  if (!syncable) {
    LOG(DFATAL) << "Expected a boolean at '" << current_path << "."
                << kSyncableKeyName << "'";
  }

  int flags =
      syncable->GetBool() ? user_prefs::PrefRegistrySyncable::SYNCABLE_PREF : 0;

  const base::Value* type_value =
      definition->FindKeyOfType(kTypeKeyName, base::Value::Type::STRING);
  if (!type_value) {
    LOG(DFATAL) << "Expected a string or a list at '" << current_path << "."
                << kTypeKeyName << "'";
  }
  std::string type = type_value->GetString();

  const base::Value* default_value;
  std::unique_ptr<base::Value> computed_default;
  default_value = definition->FindKey(GetPlatformDefaultKey());
  if (!default_value)
    default_value = definition->FindKey(kDefaultKeyName);
  if (!default_value || default_value->is_none()) {
    computed_default = GetComputedDefault(current_path);
    if (computed_default->is_none())
      return;
    default_value = const_cast<const base::Value*>(computed_default.get());
  }

  if (type.compare(kTypeEnumName) == 0) {
    const base::DictionaryValue* enum_values;
    if (!definition->GetDictionary(kEnumValuesKey, &enum_values))
      LOG(DFATAL) << "Expected a dictionary at '" << current_path << "."
                  << kEnumValuesKey << "'";

    if (!default_value->is_string())
      LOG(DFATAL) << "Expected a string at '" << current_path << "."
                  << kDefaultKeyName << "'";
    std::string default_string = default_value->GetString();

    std::unordered_map<std::string, int> string_to_value;
    std::unordered_map<int, std::string> value_to_string;
    bool found_default = false;
    int default_index = 0;
    for (const auto& enum_value : *enum_values) {
      if (!enum_value.second->is_int())
        LOG(DFATAL) << "Expected an integer at '" << current_path << "."
                    << kEnumValuesKey << "." << enum_value.first << "'";
      string_to_value.insert(
          std::make_pair(enum_value.first, enum_value.second->GetInt()));
      value_to_string.insert(
          std::make_pair(enum_value.second->GetInt(), enum_value.first));

      if (default_string == enum_value.first) {
        default_index = enum_value.second->GetInt();
        found_default = true;
      }
    }

    if (!found_default)
      LOG(DFATAL) << "Default value for enum isn't part of possible values at '"
                  << current_path << "'";

    prefs_properties->insert(std::make_pair(
        current_path, PrefProperties(false, std::move(string_to_value),
                                     std::move(value_to_string))));
    registry->RegisterIntegerPref(current_path, default_index, flags);
    return;
  }

  if (type.compare(kTypeStringName) == 0) {
    if (!default_value->is_string())
      LOG(DFATAL) << "Expected a string at '" << current_path << "."
                  << kDefaultKeyName << "'";

    prefs_properties->insert(
        std::make_pair(current_path, PrefProperties(false)));
    registry->RegisterStringPref(current_path, default_value->GetString(),
                                 flags);

  } else if (type.compare(kTypeFilePathName) == 0) {
    if (!default_value->is_string())
      LOG(DFATAL) << "Expected a string at '" << current_path << "."
                  << kDefaultKeyName << "'";

    prefs_properties->insert(
        std::make_pair(current_path, PrefProperties(false)));
    registry->RegisterFilePathPref(
        current_path,
        base::FilePath::FromUTF8Unsafe(default_value->GetString()), flags);

  } else if (type.compare(kTypeBooleanName) == 0) {
    if (!default_value->is_bool())
      LOG(DFATAL) << "Expected a boolean at '" << current_path << "."
                  << kDefaultKeyName << "'";

    prefs_properties->insert(
        std::make_pair(current_path, PrefProperties(false)));
    registry->RegisterBooleanPref(current_path, default_value->GetBool(),
                                  flags);

  } else if (type.compare(kTypeIntegerName) == 0) {
    if (!default_value->is_int())
      LOG(DFATAL) << "Expected an integer at '" << current_path << "."
                  << kDefaultKeyName << "'";

    prefs_properties->insert(
        std::make_pair(current_path, PrefProperties(false)));
    registry->RegisterIntegerPref(current_path, default_value->GetInt(), flags);

  } else if (type.compare(kTypeDoubleName) == 0) {
    if (!default_value->is_double())
      LOG(DFATAL) << "Expected a double at '" << current_path << "."
                  << kDefaultKeyName << "'";

    prefs_properties->insert(
        std::make_pair(current_path, PrefProperties(false)));
    registry->RegisterDoublePref(current_path, default_value->GetDouble(),
                                 flags);

  } else if (type.compare(kTypeListName) == 0) {
    if (!default_value->is_list())
      LOG(DFATAL) << "Expected a list at '" << current_path << "."
                  << kDefaultKeyName << "'";

    prefs_properties->insert(
        std::make_pair(current_path, PrefProperties(false)));
    registry->RegisterListPref(
        current_path,
        base::ListValue::From(
            std::make_unique<base::Value>(default_value->Clone())),
        flags);

  } else if (type.compare(kTypeDictionaryName) == 0) {
    if (!default_value->is_dict())
      LOG(DFATAL) << "Expected a dictionary at '" << current_path << "."
                  << kDefaultKeyName << "'";

    prefs_properties->insert(
        std::make_pair(current_path, PrefProperties(false)));
    registry->RegisterDictionaryPref(
        current_path,
        base::DictionaryValue::From(
            std::make_unique<base::Value>(default_value->Clone())),
        flags);

  } else {
    LOG(DFATAL) << "Invalid type value at '" << current_path << "'";
  }
}

std::unordered_map<std::string, PrefProperties> RegisterBrowserPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  base::FilePath prefs_definition_file;
#if !defined(OS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(apps::kLoadAndLaunchApp) &&
      !version_info::IsOfficialBuild()) {
    base::FilePath prefs_definition_file_from_switch =
        command_line->GetSwitchValuePath(apps::kLoadAndLaunchApp);
    if (!prefs_definition_file_from_switch.IsAbsolute()) {
      base::PathService::Get(base::DIR_EXE, &prefs_definition_file);
      prefs_definition_file =
          prefs_definition_file
              .Append(
                  prefs_definition_file_from_switch.NormalizePathSeparators())
              .Append(kPrefsDefinitionFileName);

      // ReadFileToString called below will refuse any path containing a
      // reference to parent (..) componwnt, for security reasons. Since the
      // path to vivapp used in development commonly contains those, we have to
      // strip them. It's probably a bad idea to circumvent that check in this
      // way, but:
      // 1. This code path is forbidden in official builds anyway.
      // 2. The consequences of being able to refer to any file here are
      // unlikely to be very severe.
      if (prefs_definition_file.ReferencesParent()) {
        std::vector<base::FilePath::StringType> components;
        prefs_definition_file.GetComponents(&components);
        prefs_definition_file.clear();

        for (const auto& component : components) {
          if (component.compare(FILE_PATH_LITERAL("..")) == 0)
            prefs_definition_file = prefs_definition_file.DirName();
          else
            prefs_definition_file = prefs_definition_file.Append(component);
        }
      }
    } else {
      prefs_definition_file =
          prefs_definition_file_from_switch.Append(kPrefsDefinitionFileName);
    }
  }
#endif  // !OS_ANDROID
  if (prefs_definition_file.empty() ||
      !base::PathExists(prefs_definition_file)) {
    base::PathService::Get(chrome::DIR_RESOURCES, &prefs_definition_file);
    prefs_definition_file =
        prefs_definition_file.Append(kVivaldiResourcesFolder)
            .Append(kPrefsDefinitionFileName);
  }

  LOG_IF(DFATAL, !base::PathExists(prefs_definition_file))
      << "Could not find the preference definition file";

  std::string prefs_definitions_content;
  base::ReadFileToString(prefs_definition_file, &prefs_definitions_content);

  auto prefs_definitions_json =
      base::JSONReader::Read(prefs_definitions_content);
  base::DictionaryValue* prefs_definitions = nullptr;
  if (!prefs_definitions_json.get() ||
      !prefs_definitions_json->GetAsDictionary(&prefs_definitions)) {
    LOG(DFATAL) << "Expected a dictionary at the root of the pref definitions";
  }

  base::DictionaryValue* vivaldi_pref_definition = nullptr;
  if (!prefs_definitions->GetDictionary(kVivaldiKeyName,
                                        &vivaldi_pref_definition)) {
    LOG(DFATAL) << "Expected a dictionary at '" << kVivaldiKeyName << "'";
  }

  base::DictionaryValue* chromium_prefs_list = nullptr;
  if (!prefs_definitions->GetDictionary(kChromiumKeyName,
                                        &chromium_prefs_list)) {
    LOG(DFATAL) << "Expected a dictionary at '" << kChromiumKeyName << "'";
  }

  base::DictionaryValue* chromium_local_prefs_list = nullptr;
  if (!prefs_definitions->GetDictionary(kChromiumLocalKeyName,
                                        &chromium_local_prefs_list)) {
    LOG(DFATAL) << "Expected a dictionary at '" << kChromiumLocalKeyName << "'";
  }

  PrefsProperties prefs_properties;

  RegisterPrefsFromDefinition(vivaldi_pref_definition, kVivaldiKeyName,
                              registry, &prefs_properties);

  for (const auto& pref : *chromium_prefs_list) {
    std::string pref_path;
    if (!pref.second->GetAsString(&pref_path)) {
      LOG(DFATAL) << "Expected a string at 'chromium." << pref.first;
    }
    prefs_properties.insert(std::make_pair(pref_path, PrefProperties(false)));
  }

  for (const auto& pref : *chromium_local_prefs_list) {
    std::string pref_path;
    if (!pref.second->GetAsString(&pref_path)) {
      LOG(DFATAL) << "Expected a string at 'chromium." << pref.first;
    }
    prefs_properties.insert(std::make_pair(pref_path, PrefProperties(true)));
  }

  return prefs_properties;
}

void RegisterOldPrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Note: We don't really care about default values for the following prefs
  // because these prefs are only registered in order to migrate them and then
  // we only migrate those prefs for which the value was explicitly set by the
  // user. So these defaults are never used in practice.

  registry->RegisterBooleanPref(
      vivaldiprefs::kOldMousegesturesEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(
      vivaldiprefs::kOldRockerGesturesEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(
      vivaldiprefs::kOldSmoothScrollingEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterStringPref(vivaldiprefs::kOldVivaldiCaptureDirectory, "",
                               0);

  registry->RegisterBooleanPref(
      vivaldiprefs::kOldVivaldiTabZoom, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterIntegerPref(
      vivaldiprefs::kOldVivaldiNumberOfDaysToKeepVisits, 90,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(vivaldiprefs::kOldPluginsWidevideEnabled, true);

  registry->RegisterBooleanPref(
      vivaldiprefs::kOldVivaldiTabsToLinks, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(
      vivaldiprefs::kOldDeferredTabLoadingAfterRestore, true);

  registry->RegisterBooleanPref(
      vivaldiprefs::kOldAlwaysLoadPinnedTabAfterRestore, true);

  registry->RegisterBooleanPref(
      vivaldiprefs::kOldVivaldiUseNativeWindowDecoration, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterStringPref(vivaldiprefs::kOldVivaldiHomepage, "",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  RegisterOldPlatformPrefs(registry);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // This pref is obsolete.
  registry->RegisterBooleanPref(vivaldiprefs::kAutoUpdateEnabled, true);

  registry->RegisterListPref(vivaldiprefs::kVivaldiExperiments);
  registry->RegisterInt64Pref(vivaldiprefs::kVivaldiLastTopSitesVacuumDate, 0);
}

void MigrateOldPrefs(PrefService* prefs) {
  {
    const base::Value* old_mouse_gestures_pref =
        prefs->GetUserPrefValue(vivaldiprefs::kOldMousegesturesEnabled);
    if (old_mouse_gestures_pref)
      prefs->Set(vivaldiprefs::kMouseGesturesEnabled, *old_mouse_gestures_pref);
    prefs->ClearPref(vivaldiprefs::kOldMousegesturesEnabled);
  }

  {
    const base::Value* old_rocker_gestures_pref =
        prefs->GetUserPrefValue(vivaldiprefs::kOldRockerGesturesEnabled);
    if (old_rocker_gestures_pref)
      prefs->Set(vivaldiprefs::kMouseGesturesRockerGesturesEnabled,
                 *old_rocker_gestures_pref);
    prefs->ClearPref(vivaldiprefs::kOldRockerGesturesEnabled);
  }

  {
    const base::Value* old_smooth_scrolling_pref =
        prefs->GetUserPrefValue(vivaldiprefs::kOldSmoothScrollingEnabled);
    if (old_smooth_scrolling_pref)
      prefs->Set(vivaldiprefs::kWebpagesSmoothScrollingEnabled,
                 *old_smooth_scrolling_pref);
    prefs->ClearPref(vivaldiprefs::kOldSmoothScrollingEnabled);
  }

  {
    const base::Value* old_capture_directory_pref =
        prefs->GetUserPrefValue(vivaldiprefs::kOldVivaldiCaptureDirectory);
    if (old_capture_directory_pref)
      prefs->Set(vivaldiprefs::kWebpagesCaptureDirectory,
                 *old_capture_directory_pref);
    prefs->ClearPref(vivaldiprefs::kOldVivaldiCaptureDirectory);
  }

  {
    const base::Value* old_tab_zoom_pref =
        prefs->GetUserPrefValue(vivaldiprefs::kOldVivaldiTabZoom);
    if (old_tab_zoom_pref)
      prefs->Set(vivaldiprefs::kWebpagesTabZoomEnabled, *old_tab_zoom_pref);
    prefs->ClearPref(vivaldiprefs::kOldVivaldiTabZoom);
  }

#if !defined(OS_ANDROID)
  {
    const base::Value* old_number_of_days_to_keep_visits_pref =
        prefs->GetUserPrefValue(
            vivaldiprefs::kOldVivaldiNumberOfDaysToKeepVisits);
    if (old_number_of_days_to_keep_visits_pref) {
      int days = old_number_of_days_to_keep_visits_pref->GetInt();
      // The UI used to set incorrect number of days for all options
      // from 3 months and up. Taking the opportunity of this migration
      // to fix that as well.
      switch (days) {
        case 90:
          days = 91;
          break;
        case 178:
          days = 182;
          break;
        case 356:
          days = 365;
          break;
        case 3560:
          days = 3650;
          break;
        default:
          // Do nothing for other values.
          break;
      }
      prefs->SetInteger(vivaldiprefs::kHistoryDaysToKeepVisits, days);
    }
    prefs->ClearPref(vivaldiprefs::kOldVivaldiNumberOfDaysToKeepVisits);
  }
#endif

  {
    const base::Value* old_widevine_enabled_pref =
        prefs->GetUserPrefValue(vivaldiprefs::kOldPluginsWidevideEnabled);
    if (old_widevine_enabled_pref)
      prefs->Set(vivaldiprefs::kPluginsWidevineEnabled,
                 *old_widevine_enabled_pref);
    prefs->ClearPref(vivaldiprefs::kOldPluginsWidevideEnabled);
  }

  {
    const base::Value* old_tabs_to_link_pref =
        prefs->GetUserPrefValue(vivaldiprefs::kOldVivaldiTabsToLinks);
    if (old_tabs_to_link_pref)
      prefs->Set(vivaldiprefs::kWebpagesTabFocusesLinks,
                 *old_tabs_to_link_pref);
    prefs->ClearPref(vivaldiprefs::kOldVivaldiTabsToLinks);
  }

  {
    const base::Value* old_deferred_tab_loading_pref = prefs->GetUserPrefValue(
        vivaldiprefs::kOldDeferredTabLoadingAfterRestore);
    if (old_deferred_tab_loading_pref)
      prefs->Set(vivaldiprefs::kTabsDeferLoadingAfterRestore,
                 *old_deferred_tab_loading_pref);
    prefs->ClearPref(vivaldiprefs::kOldDeferredTabLoadingAfterRestore);
  }

  {
    const base::Value* old_always_load_pinned_tab = prefs->GetUserPrefValue(
        vivaldiprefs::kOldAlwaysLoadPinnedTabAfterRestore);
    if (old_always_load_pinned_tab)
      prefs->Set(vivaldiprefs::kTabsAlwaysLoadPinnedAfterRestore,
                 *old_always_load_pinned_tab);
    prefs->ClearPref(vivaldiprefs::kOldAlwaysLoadPinnedTabAfterRestore);
  }

  {
    const base::Value* old_use_native_decoration = prefs->GetUserPrefValue(
        vivaldiprefs::kOldVivaldiUseNativeWindowDecoration);
    if (old_use_native_decoration)
      prefs->Set(vivaldiprefs::kWindowsUseNativeDecoration,
                 *old_use_native_decoration);
    prefs->ClearPref(vivaldiprefs::kOldVivaldiUseNativeWindowDecoration);
  }

  {
    const base::Value* old_home_page_pref =
        prefs->GetUserPrefValue(vivaldiprefs::kOldVivaldiHomepage);
    if (old_home_page_pref)
      prefs->Set(vivaldiprefs::kHomepage, *old_home_page_pref);
    prefs->ClearPref(vivaldiprefs::kOldVivaldiHomepage);
  }

  MigrateOldPlatformPrefs(prefs);
}

}  // namespace vivaldi
