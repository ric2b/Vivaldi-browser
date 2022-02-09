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
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "prefs/vivaldi_browser_prefs_util.h"
#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#endif

namespace vivaldi {

void RegisterOldPlatformPrefs(user_prefs::PrefRegistrySyncable* registry);
void MigrateOldPlatformPrefs(PrefService* prefs);
base::Value GetPlatformComputedDefault(const std::string& path);

namespace {

#if !defined(OS_ANDROID)
const base::FilePath::CharType kVivaldiResourcesFolder[] =
    FILE_PATH_LITERAL("vivaldi");
#endif
const base::FilePath::CharType kPrefsDefinitionFileName[] =
    FILE_PATH_LITERAL("prefs_definitions.json");

const char kVivaldiKeyName[] = "vivaldi";
#if !defined(OS_ANDROID)
const char kChromiumKeyName[] = "chromium";
const char kChromiumLocalKeyName[] = "chromium_local";
#endif  // !defined(OS_ANDROID)

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

}  // namespace

#ifndef OS_ANDROID

namespace {

PrefPropertiesMap& GetRegisteredPrefPropertiesStorage() {
  static base::NoDestructor<PrefPropertiesMap> storage;
  return *storage;
}

}  // namespace

PrefProperties::PrefProperties() = default;
PrefProperties::PrefProperties(PrefProperties&&) = default;
PrefProperties::~PrefProperties() = default;

void PrefProperties::SetLocal(bool local_pref) {
  local_pref_ = local_pref;
}

void PrefProperties::SetEnumProperties(EnumPrefProperties enum_properties) {
  enum_properties_ =
      std::make_unique<EnumPrefProperties>(std::move(enum_properties));
}

PrefPropertiesMap ExtractLastRegisteredPrefsProperties() {
  // Should be called exactly once per profile after a call to
  // RegisterProfilePrefs.
  DCHECK(!GetRegisteredPrefPropertiesStorage().empty());
  return std::move(GetRegisteredPrefPropertiesStorage());
}

#endif  // ifndef OS_ANDROID

EnumPrefProperties::EnumPrefProperties() = default;
EnumPrefProperties::EnumPrefProperties(EnumPrefProperties&&) = default;
EnumPrefProperties::~EnumPrefProperties() = default;

absl::optional<int> EnumPrefProperties::FindValue(
    base::StringPiece name) const {
  for (const auto& i : name_value_pairs) {
    if (i.first == name)
      return i.second;
  }
  return absl::nullopt;
}

const std::string* EnumPrefProperties::FindName(int value) const {
  for (const auto& i : name_value_pairs) {
    if (i.second == value)
      return &i.first;
  }
  return nullptr;
}

bool IsMergeableListPreference(base::StringPiece name) {
  for (base::StringPiece list_name : vivaldiprefs::g_mergeable_lists) {
    if (list_name == name)
      return true;
  }
  return false;
}

constexpr int kDefaultLanguageListSize = 26;
constexpr const char* kDefaultLanguageList[kDefaultLanguageListSize] = {
    "ar", "cs", "de", "en", "es", "fa", "fi",      "fr",     "hi",
    "hu", "id", "is", "it", "ja", "ko", "nl",      "no",     "pl",
    "pt", "ru", "sv", "tr", "uk", "ur", "zh-Hans", "zh-Hant"};

void RegisterLocalState(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiAutoUpdateStandalone,
                                true);
  registry->RegisterStringPref(vivaldiprefs::kVivaldiUniqueUserId, "");
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextDailyPing,
                             base::Time());
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextWeeklyPing,
                             base::Time());
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextMonthlyPing,
                             base::Time());
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextTrimestrialPing,
                             base::Time());
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextSemestrialPing,
                             base::Time());
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsNextYearlyPing,
                             base::Time());
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiStatsExtraPing, 0);
  registry->RegisterTimePref(vivaldiprefs::kVivaldiStatsExtraPingTime,
                             base::Time());
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiStatsPingsSinceLastMonth,
                                0);
  registry->RegisterListPref(vivaldiprefs::kVivaldiProfileImagePath);
  registry->RegisterTimePref(
      vivaldiprefs::kVivaldiTranslateLanguageListLastUpdate, base::Time());
  registry->RegisterStringPref(vivaldiprefs::kVivaldiAccountServerUrlIdentity,
                               "https://login.vivaldi.net/oauth2/token");
  registry->RegisterStringPref(vivaldiprefs::kVivaldiAccountServerUrlOpenId,
                               "https://login.vivaldi.net/oauth2/userinfo");
  if (version_info::IsOfficialBuild()) {
    registry->RegisterStringPref(vivaldiprefs::kVivaldiSyncServerUrl,
                                 "https://bifrost.vivaldi.com/vivid-sync");
  } else {
    registry->RegisterStringPref(vivaldiprefs::kVivaldiSyncServerUrl,
                                 "https://bifrost.vivaldi.com:4433/vivid-sync");
  }
  registry->RegisterStringPref(vivaldiprefs::kVivaldiSyncNotificationsServerUrl,
                               "wss://bifrost.vivaldi.com:15674/ws");

  std::vector<base::Value> args;
  for (int i = 0; i < kDefaultLanguageListSize; i++) {
    args.push_back(base::Value(kDefaultLanguageList[i]));
  }
  registry->RegisterListPref(vivaldiprefs::kVivaldiTranslateLanguageList,
                             base::ListValue(std::move(args)));
}

namespace {

enum class PrefKind {
  kNone,
  kEnum,
  kString,
  kFilePath,
  kBoolean,
  kInteger,
  kDouble,
  kList,
  kDictionary,
};

base::Value GetComputedDefault(const std::string& path) {
  if (path == vivaldiprefs::kWebpagesCaptureDirectory) {
    base::FilePath path;
    base::PathService::Get(chrome::DIR_USER_PICTURES, &path);
    path = path.AppendASCII("Vivaldi Captures");
    return base::Value(path.AsUTF16Unsafe());
  }
  return GetPlatformComputedDefault(path);
}

void RegisterPrefsFromDefinition(base::Value definition,
                                 std::string current_path,
                                 user_prefs::PrefRegistrySyncable* registry,
                                 PrefPropertiesMap* pref_properties_map) {
  const base::Value* type_value = definition.FindKey(kTypeKeyName);
  if (!type_value) {
    for (auto child : definition.DictItems()) {
      std::string child_path = current_path;
      child_path += ".";
      child_path += child.first;
      if (!child.second.is_dict()) {
        LOG(FATAL) << "Expected a dictionary at '" << child_path << "'";
      }
      RegisterPrefsFromDefinition(std::move(child.second),
                                  std::move(child_path), registry,
                                  pref_properties_map);
    }
    return;
  }

  if (!type_value->is_string()) {
    LOG(FATAL) << "Expected a string or a list at '" << current_path << "."
               << kTypeKeyName << "'";
  }

  PrefKind pref_kind = PrefKind::kNone;
  base::Value::Type value_type = base::Value::Type::NONE;
  const std::string& type_str = type_value->GetString();
  if (type_str == kTypeEnumName) {
    pref_kind = PrefKind::kEnum;
    value_type = base::Value::Type::INTEGER;
  } else if (type_str == kTypeStringName) {
    pref_kind = PrefKind::kString;
    value_type = base::Value::Type::STRING;
  } else if (type_str == kTypeFilePathName) {
    pref_kind = PrefKind::kFilePath;
    value_type = base::Value::Type::STRING;
  } else if (type_str == kTypeBooleanName) {
    pref_kind = PrefKind::kBoolean;
    value_type = base::Value::Type::BOOLEAN;
  } else if (type_str == kTypeIntegerName) {
    pref_kind = PrefKind::kInteger;
    value_type = base::Value::Type::INTEGER;
  } else if (type_str == kTypeDoubleName) {
    pref_kind = PrefKind::kDouble;
    value_type = base::Value::Type::DOUBLE;
  } else if (type_str == kTypeListName) {
    pref_kind = PrefKind::kList;
    value_type = base::Value::Type::LIST;
  } else if (type_str == kTypeDictionaryName) {
    pref_kind = PrefKind::kDictionary;
    value_type = base::Value::Type::DICTIONARY;
  }
  if (pref_kind == PrefKind::kNone) {
    LOG(FATAL) << "Invalid type value at '" << current_path << "'";
  }

  absl::optional<bool> syncable = definition.FindBoolKey(kSyncableKeyName);
  if (!syncable) {
    LOG(FATAL) << "Expected a boolean at '" << current_path << "."
               << kSyncableKeyName << "'";
  }
  int flags = *syncable ? user_prefs::PrefRegistrySyncable::SYNCABLE_PREF : 0;

  const char* default_key_name = vivaldiprefs::kPlatformDefaultKeyName;
  base::Value* default_value_ptr = definition.FindKey(default_key_name);
  if (!default_value_ptr) {
    default_key_name = kDefaultKeyName;
    default_value_ptr = definition.FindKey(default_key_name);
  }
  base::Value default_value;
  if (default_value_ptr && !default_value_ptr->is_none()) {
    default_value = std::move(*default_value_ptr);
    base::Value::Type default_type =
        pref_kind == PrefKind::kEnum ? base::Value::Type::STRING : value_type;
    if (default_value.type() != default_type) {
      LOG(FATAL) << "Unexpected type for '" << current_path << "."
                 << default_key_name << "' - " << default_value.type();
    }
  } else {
    default_value = GetComputedDefault(current_path);
    if (default_value.is_none()) {
      // The preference is not defined for the current platform.
      return;
    }

    // For enums the computed default should be int.
    if (default_value.type() != value_type) {
      LOG(FATAL) << "Unexpected type of computed default value for '"
                 << current_path << "' - " << default_value.type();
    }
  }

#if !defined(OS_ANDROID)
  PrefProperties properties;
#endif
  switch (pref_kind) {
    case PrefKind::kEnum: {
      const base::Value* enum_values = definition.FindDictKey(kEnumValuesKey);
      if (!enum_values) {
        LOG(FATAL) << "Expected a dictionary at '" << current_path << "."
                   << kEnumValuesKey << "'";
        return;
      }

      EnumPrefProperties enum_properties;
      enum_properties.name_value_pairs.reserve(enum_values->DictSize());
      for (auto enum_name_value : enum_values->DictItems()) {
        const std::string& name = enum_name_value.first;
        if (!enum_name_value.second.is_int()) {
          LOG(FATAL) << "Expected an integer at '" << current_path << "."
                     << kEnumValuesKey << "." << enum_name_value.first << "'";
        }
        int value = enum_name_value.second.GetInt();
        if (enum_properties.FindValue(name)) {
          LOG(FATAL) << "Duplicated enum case at '" << current_path << "."
                     << kEnumValuesKey << "." << name << "'";
        }
        if (enum_properties.FindName(value)) {
          LOG(FATAL) << "Duplicated enum value at '" << current_path << "."
                     << kEnumValuesKey << "." << name << "'";
        }
        enum_properties.name_value_pairs.emplace_back(name, value);
      }

      absl::optional<int> enum_default;
      if (default_value.is_string()) {
        enum_default = enum_properties.FindValue(default_value.GetString());
      } else {
        // Computed value
        if (enum_properties.FindName(default_value.GetInt())) {
          enum_default = default_value.GetInt();
        }
      }
      if (!enum_default) {
        LOG(FATAL)
            << "Default value for enum isn't part of possible values at '"
            << current_path << "'";
      }
      registry->RegisterIntegerPref(current_path, *enum_default, flags);
#if !defined(OS_ANDROID)
      properties.SetEnumProperties(std::move(enum_properties));
#endif
      break;
    }
    case PrefKind::kString:
      registry->RegisterStringPref(current_path, default_value.GetString(),
                                   flags);
      break;
    case PrefKind::kFilePath:
      registry->RegisterFilePathPref(
          current_path,
          base::FilePath::FromUTF8Unsafe(default_value.GetString()), flags);
      break;
    case PrefKind::kBoolean:
      registry->RegisterBooleanPref(current_path, default_value.GetBool(),
                                    flags);
      break;
    case PrefKind::kInteger:
      registry->RegisterIntegerPref(current_path, default_value.GetInt(),
                                    flags);
      break;
    case PrefKind::kDouble:
      registry->RegisterDoublePref(current_path, default_value.GetDouble(),
                                   flags);
      break;
    case PrefKind::kList:
      registry->RegisterListPref(current_path, std::move(default_value), flags);

      break;
    case PrefKind::kDictionary:
      registry->RegisterDictionaryPref(current_path, std::move(default_value),
                                       flags);
      break;
    default:
      NOTREACHED();
  }

#if !defined(OS_ANDROID)
  pref_properties_map->emplace(std::move(current_path), std::move(properties));
#endif
}

#if !defined(OS_ANDROID)

void AddChromiumProperties(base::Value* value,
                           base::StringPiece current_path,
                           bool local_pref,
                           PrefPropertiesMap* pref_properties_map) {
  base::Value* chromium_prefs = value->FindDictKey(current_path);
  if (!chromium_prefs) {
    LOG(FATAL) << "Expected a dictionary at '" << current_path << "'";
  }

  for (const auto pref : chromium_prefs->DictItems()) {
    if (!pref.second.is_dict()) {
      LOG(FATAL) << "Expected a dictionary at '" << current_path << "."
                 << pref.first << "'";
    }
    std::string* pref_path = pref.second.FindStringKey("path");
    if (!pref_path) {
      LOG(FATAL) << "Expected a string at '" << current_path << "."
                 << pref.first << ".path'";
    }

    PrefProperties properties;
    properties.SetLocal(local_pref);
    pref_properties_map->emplace(std::move(*pref_path), std::move(properties));
  }
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

#endif  // !defined(OS_ANDROID)

base::Value ReadPrefsJson() {
  // This might be called outside the startup, eg. during creation of a guest
  // window, so need to allow IO.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FilePath prefs_definition_file;
  std::string prefs_definitions_content;

#if defined(OS_ANDROID)
  // For Android, get the prefs definitions from assets.
  prefs_definition_file = base::FilePath(FILE_PATH_LITERAL("assets"));
  prefs_definition_file =
      prefs_definition_file.Append(kPrefsDefinitionFileName);

  base::MemoryMappedFile::Region prefs_region;
  int prefs_fd =
      base::android::OpenApkAsset(prefs_definition_file.value(), &prefs_region);
  LOG_IF(FATAL, prefs_fd < 0)
      << "No preference definitions file in APK assets: "
      << prefs_definition_file.value();
  std::unique_ptr<base::MemoryMappedFile> mapped_file(
      new base::MemoryMappedFile());
  if (mapped_file->Initialize(base::File(prefs_fd), prefs_region)) {
    prefs_definitions_content.assign(
        reinterpret_cast<char*>(mapped_file->data()), mapped_file->length());
  }
#else   // defined(OS_ANDROID)
  GetDeveloperFilePath(kPrefsDefinitionFileName, &prefs_definition_file);
  if (prefs_definition_file.empty() ||
      !base::PathExists(prefs_definition_file)) {
    base::PathService::Get(chrome::DIR_RESOURCES, &prefs_definition_file);
    prefs_definition_file =
        prefs_definition_file.Append(kVivaldiResourcesFolder)
            .Append(kPrefsDefinitionFileName);
  }

  LOG_IF(FATAL, !base::PathExists(prefs_definition_file))
      << "Could not find the preference definitions file";

  base::ReadFileToString(prefs_definition_file, &prefs_definitions_content);
#endif  // OS_ANDROID

  absl::optional<base::Value> prefs_definitions =
      base::JSONReader::Read(prefs_definitions_content);
  if (!prefs_definitions || !prefs_definitions->is_dict()) {
    LOG(FATAL) << "Expected a dictionary at the root of the pref definitions";
  }
  return std::move(*prefs_definitions);
}

}  // namespace

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // This pref is obsolete.
  registry->RegisterBooleanPref(vivaldiprefs::kAutoUpdateEnabled, true);

  registry->RegisterDictionaryPref(
      vivaldiprefs::kVivaldiAccountPendingRegistration);
  registry->RegisterListPref(vivaldiprefs::kVivaldiExperiments);
  registry->RegisterInt64Pref(vivaldiprefs::kVivaldiLastTopSitesVacuumDate, 0);

  base::Value prefs_definitions = ReadPrefsJson();
  if (!prefs_definitions.is_dict()) {
    LOG(FATAL) << "Expected a dictionary at the root of the pref definitions";
  }

  base::Value* vivaldi_pref_definition =
      prefs_definitions.FindDictKey(kVivaldiKeyName);
  if (!vivaldi_pref_definition) {
    LOG(FATAL) << "Expected a dictionary at '" << kVivaldiKeyName << "'";
  }

  PrefPropertiesMap pref_properties_map;

  RegisterPrefsFromDefinition(std::move(*vivaldi_pref_definition),
                              kVivaldiKeyName, registry, &pref_properties_map);

#if !defined(OS_ANDROID)
  AddChromiumProperties(&prefs_definitions, kChromiumKeyName, false,
                        &pref_properties_map);
  AddChromiumProperties(&prefs_definitions, kChromiumLocalKeyName, true,
                        &pref_properties_map);

  DCHECK(GetRegisteredPrefPropertiesStorage().empty());
  GetRegisteredPrefPropertiesStorage() = std::move(pref_properties_map);

  RegisterOldPrefs(registry);
#endif  // !defined(OS_ANDROID)
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
