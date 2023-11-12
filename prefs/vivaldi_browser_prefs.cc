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
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "components/browser/vivaldi_brand_select.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

#include "components/datasource/resource_reader.h"
#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

// Preference override is available on Linux or in internal desktop builds on
// any platform for testing convenience.
#if BUILDFLAG(IS_LINUX) || \
    !(defined(OFFICIAL_BUILD) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))
#define VIVALDI_PREFERENCE_OVERRIDE_FILE
#endif

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/browser_process.h"
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

#ifdef VIVALDI_PREFERENCE_OVERRIDE_FILE
#include "components/datasource/vivaldi_theme_io.h"
#endif

namespace vivaldi {

void RegisterOldPlatformPrefs(user_prefs::PrefRegistrySyncable* registry);
void MigrateOldPlatformPrefs(PrefService* prefs);
base::Value GetPlatformComputedDefault(const std::string& path);

namespace {

const char kPrefsDefinitionFileName[] = "prefs_definitions.json";

#ifdef VIVALDI_PREFERENCE_OVERRIDE_FILE

namespace prefs_overrides {

// Extra file to allow overriding of some preferences without changing the main
// file.
const char kFileName[] = "prefs_overrides.json";

// Suported keys in prefs_overrides.
const char kComment[] = "comment";

// Default theme to use.
const char kThemeDefault[] = "themeDefault";

// Default theme to use during day with active scheduling.
const char kThemeDefaultDay[] = "themeDefaultDay";

// Default theme to use during night with active scheduling.
const char kThemeDefaultNight[] = "themeDefaultNight";

// Default theme for private windows.
const char kThemeDefaultPrivate[] = "themeDefaultPrivate";

// Themes to prepend to the list of default themes.
const char kThemeExtra[] = "themeExtra";

}  // namespace prefs_overrides

#endif  // VIVALDI_PREFERENCE_OVERRIDE_FILE

const char kVivaldiKeyName[] = "vivaldi";
#if !BUILDFLAG(IS_ANDROID)
const char kChromiumKeyName[] = "chromium";
const char kChromiumLocalKeyName[] = "chromium_local";
#endif  // !BUILDFLAG(IS_ANDROID)

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

#if !BUILDFLAG(IS_ANDROID)

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

#endif  // !IS_ANDROID

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

  base::Value::List args;
  for (int i = 0; i < kDefaultLanguageListSize; i++) {
    args.Append(base::Value(kDefaultLanguageList[i]));
  }
  registry->RegisterListPref(vivaldiprefs::kVivaldiTranslateLanguageList,
                             std::move(args));

  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiClientHintsBrand,
                                int(BrandSelection::kChromeBrand));
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiClientHintsBrandAppendVivaldi, false);
  registry->RegisterStringPref(
      vivaldiprefs::kVivaldiClientHintsBrandCustomBrand, "");
  registry->RegisterStringPref(
      vivaldiprefs::kVivaldiClientHintsBrandCustomBrandVersion, "");
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
    base::FilePath pictures_path;
    base::PathService::Get(chrome::DIR_USER_PICTURES, &pictures_path);
    pictures_path = pictures_path.AppendASCII("Vivaldi Captures");
    return base::Value(pictures_path.AsUTF16Unsafe());
  }
  return GetPlatformComputedDefault(path);
}

void RegisterPrefsFromDefinition(base::Value::Dict& definition,
                                 std::string current_path,
                                 user_prefs::PrefRegistrySyncable* registry,
                                 PrefPropertiesMap* pref_properties_map) {
  const base::Value* type_value = definition.Find(kTypeKeyName);
  if (!type_value) {
    for (auto child : definition) {
      std::string child_path = current_path;
      child_path += ".";
      child_path += child.first;
      if (!child.second.is_dict()) {
        LOG(FATAL) << "Expected a dictionary at '" << child_path << "'";
      }
      RegisterPrefsFromDefinition(child.second.GetDict(), std::move(child_path),
                                  registry, pref_properties_map);
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
    value_type = base::Value::Type::DICT;
  }
  if (pref_kind == PrefKind::kNone) {
    LOG(FATAL) << "Invalid type value at '" << current_path << "'";
  }

  absl::optional<bool> syncable = definition.FindBool(kSyncableKeyName);
  if (!syncable) {
    LOG(FATAL) << "Expected a boolean at '" << current_path << "."
               << kSyncableKeyName << "'";
  }
  int flags = *syncable ? user_prefs::PrefRegistrySyncable::SYNCABLE_PREF : 0;

  const char* default_key_name = vivaldiprefs::kPlatformDefaultKeyName;
  base::Value* default_value_ptr = definition.Find(default_key_name);
  if (!default_value_ptr) {
    default_key_name = kDefaultKeyName;
    default_value_ptr = definition.Find(default_key_name);
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

#if !BUILDFLAG(IS_ANDROID)
  PrefProperties properties;
#endif
  switch (pref_kind) {
    case PrefKind::kEnum: {
      const base::Value::Dict* enum_dict = definition.FindDict(kEnumValuesKey);
      if (!enum_dict) {
        LOG(FATAL) << "Expected a dictionary at '" << current_path << "."
                   << kEnumValuesKey << "'";
        return;
      }

      EnumPrefProperties enum_properties;
      enum_properties.name_value_pairs.reserve(enum_dict->size());
      for (auto enum_name_value : *enum_dict) {
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
#if !BUILDFLAG(IS_ANDROID)
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
      registry->RegisterListPref(current_path,
                                 std::move(default_value.GetList()), flags);

      break;
    case PrefKind::kDictionary:
      registry->RegisterDictionaryPref(
          current_path, std::move(default_value.GetDict()), flags);
      break;
    default:
      NOTREACHED();
  }

#if !BUILDFLAG(IS_ANDROID)
  pref_properties_map->emplace(std::move(current_path), std::move(properties));
#endif
}

#if !BUILDFLAG(IS_ANDROID)

void AddChromiumProperties(base::Value::Dict& prefs,
                           base::StringPiece current_path,
                           bool local_pref,
                           PrefPropertiesMap* pref_properties_map) {
  base::Value::Dict* chromium_prefs = prefs.FindDict(current_path);
  if (!chromium_prefs) {
    LOG(FATAL) << "Expected a dictionary at '" << current_path << "'";
  }

  for (const auto pref : *chromium_prefs) {
    if (!pref.second.is_dict()) {
      LOG(FATAL) << "Expected a dictionary at '" << current_path << "."
                 << pref.first << "'";
    }
    std::string* pref_path = pref.second.GetDict().FindString("path");
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

#endif  // !BUILDFLAG(IS_ANDROID)

#ifdef VIVALDI_PREFERENCE_OVERRIDE_FILE

namespace prefs_overrides {

struct PrefOverrideValues {
  std::string theme_default;
  std::string theme_default_day_or_night[2];
  std::string theme_default_private;

  base::Value theme_extra;
};

// Patch default preferences according to the override JSON. The code trusts
// prefs structure (in the release builds erroneous assumptions will CHECK() or
// crash on null pointer), but verifies the overrides reporting errors there.
void PatchPrefsJson(base::Value::Dict& prefs, base::Value& overrides) {
  bool has_errors = false;
  auto error = [&](std::string message) {
    LOG(ERROR) << prefs_overrides::kFileName << ": " << message;
    has_errors = true;
  };
  auto append_default = [](base::StringPiece path) {
    return std::string(path) + ".default";
  };

  if (!overrides.is_dict()) {
    error("JSON is not an object");
    return;
  }

  constexpr size_t kDayIndex = 0;
  constexpr size_t kNightIndex = 1;

  PrefOverrideValues values;
  for (auto name_value : overrides.GetDict()) {
    base::StringPiece name = name_value.first;
    base::Value& value = name_value.second;
    std::string* values_string = nullptr;
    if (name == kComment) {
      continue;
    } else if (name == kThemeDefault) {
      values_string = &values.theme_default;
    } else if (name == kThemeDefaultDay) {
      values_string = &values.theme_default_day_or_night[kDayIndex];
    } else if (name == kThemeDefaultNight) {
      values_string = &values.theme_default_day_or_night[kNightIndex];
    } else if (name == kThemeDefaultPrivate) {
      values_string = &values.theme_default_private;
    } else if (name == kThemeExtra) {
      if (!value.is_list()) {
        error("the value of " + std::string(name) +
              " property is not an array");
        continue;
      }
      std::vector<std::string> all_theme_ids;
      for (base::Value& elem : value.GetList()) {
        std::string verify_error;
        vivaldi_theme_io::VerifyAndNormalizeJson({.allow_named_id = true}, elem,
                                                 verify_error);
        if (!verify_error.empty()) {
          error(verify_error);
          break;
        }
        std::string* theme_id =
            elem.GetDict().FindString(vivaldi_theme_io::kIdKey);
        if (base::StartsWith(*theme_id, vivaldi_theme_io::kVivaldiIdPrefix)) {
          error(std::string("id of an extra theme cannot start with ") +
                vivaldi_theme_io::kVivaldiIdPrefix + ". Use " +
                vivaldi_theme_io::kVendorIdPrefix + " instead");
          break;
        }
        all_theme_ids.push_back(*theme_id);
      }
      if (base::flat_set<std::string>(std::move(all_theme_ids)).size() !=
          value.GetList().size()) {
        error("Duplicated theme ids in " + std::string(name));
      }
      if (has_errors)
        continue;
      values.theme_extra = std::move(value);
    } else {
      error("unsupported object property - " + std::string(name));
      continue;
    }
    if (values_string) {
      if (!value.is_string()) {
        error("the value of " + std::string(name) +
              " property is not a string");
        continue;
      }
      *values_string = std::move(value.GetString());
    }
    VLOG(2) << prefs_overrides::kFileName << ": processed " << name
            << " override";
  }
  if (has_errors)
    return;

  if (!values.theme_default.empty()) {
    prefs.FindByDottedPath(append_default(vivaldiprefs::kThemesCurrent))
        ->GetString() = std::move(values.theme_default);
  }
  if (!values.theme_default_private.empty()) {
    prefs.FindByDottedPath(append_default(vivaldiprefs::kThemesCurrentPrivate))
        ->GetString() = std::move(values.theme_default_private);
  }
  for (size_t i = 0; i < 2; ++i) {
    DCHECK(i == kDayIndex || i == kNightIndex);
    std::string& value = values.theme_default_day_or_night[i];
    if (value.empty()) {
      value = values.theme_default;
    }
    if (value.empty())
      continue;
    prefs
        .FindByDottedPath(append_default(vivaldiprefs::kThemeScheduleTimeline))
        ->GetList()[i]
        .GetDict()
        .Find("themeId")
        ->GetString() = value;
    std::string schedule_path = append_default(vivaldiprefs::kThemeScheduleOS);
    schedule_path.append(i == kDayIndex ? ".light" : ".dark");
    prefs.FindByDottedPath(schedule_path)->GetString() = std::move(value);
  }
  if (!values.theme_extra.GetList().empty()) {
    // Prepent value elements to the original list.
    base::Value* themes_value =
        prefs.FindByDottedPath(append_default(vivaldiprefs::kThemesSystem));
    base::Value themes = std::move(*themes_value);
    auto themes_start = themes.GetList().begin();
    for (auto& it : values.theme_extra.GetList())
      themes.GetList().Insert(themes_start, std::move(it));
    *themes_value = std::move(themes);
  }

  auto pretty_print_patched_prefs = [](const base::Value::Dict& prefs) {
    std::string text;
    base::JSONWriter::WriteWithOptions(base::Value(prefs.Clone()),
                                       base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                       &text);
    return text;
  };
  VLOG(7) << "patched default preferences: "
          << pretty_print_patched_prefs(prefs);
}

}  // namespace prefs_overrides

#endif  // VIVALDI_PREFERENCE_OVERRIDE_FILE

base::Value::Dict ReadPrefsJson() {
  // This might be called outside the startup, eg. during creation of a guest
  // window, so need to allow IO.
  base::VivaldiScopedAllowBlocking allow_blocking;

  ResourceReader reader_main(kPrefsDefinitionFileName);
  absl::optional<base::Value> dictionary_value = reader_main.ParseJSON();
  if (!dictionary_value) {
    // Any error in the primary preference file is fatal.
    LOG(FATAL) << reader_main.GetError();
  }
  base::Value::Dict* dictionary = dictionary_value->GetIfDict();
  if (!dictionary) {
    LOG(FATAL) << kPrefsDefinitionFileName << ": JSON is not an object";
  }

#ifdef VIVALDI_PREFERENCE_OVERRIDE_FILE
  ResourceReader reader_overrides(prefs_overrides::kFileName);
  absl::optional<base::Value> overrides = reader_overrides.ParseJSON();
  if (!overrides) {
    if (!reader_overrides.IsNotFoundError()) {
      LOG(ERROR) << prefs_overrides::kFileName << ": "
                 << reader_overrides.GetError();
    }
  } else {
    prefs_overrides::PatchPrefsJson(*dictionary, *overrides);
  }
#endif  // VIVALDI_PREFERENCE_OVERRIDE_FILE

  return std::move(*dictionary);
}

}  // namespace

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // This pref is obsolete.
  registry->RegisterBooleanPref(vivaldiprefs::kAutoUpdateEnabled, true);

  registry->RegisterDictionaryPref(
      vivaldiprefs::kVivaldiAccountPendingRegistration);
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiAddressBarSearchDirectMatchEnabled,
      g_browser_process->GetApplicationLocale() == "en-US",
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  registry->RegisterListPref(vivaldiprefs::kVivaldiExperiments);
  registry->RegisterInt64Pref(vivaldiprefs::kVivaldiLastTopSitesVacuumDate, 0);
  registry->RegisterDictionaryPref(vivaldiprefs::kVivaldiPIPPlacement);

  base::Value::Dict prefs_definitions = ReadPrefsJson();

  base::Value::Dict* vivaldi_pref_definition =
      prefs_definitions.FindDict(kVivaldiKeyName);
  if (!vivaldi_pref_definition) {
    LOG(FATAL) << "Expected a dictionary at '" << kVivaldiKeyName << "'";
  }

  PrefPropertiesMap pref_properties_map;

  RegisterPrefsFromDefinition(*vivaldi_pref_definition, kVivaldiKeyName,
                              registry, &pref_properties_map);

#if !BUILDFLAG(IS_ANDROID)
  AddChromiumProperties(prefs_definitions, kChromiumKeyName, false,
                        &pref_properties_map);
  AddChromiumProperties(prefs_definitions, kChromiumLocalKeyName, true,
                        &pref_properties_map);

  DCHECK(GetRegisteredPrefPropertiesStorage().empty());
  GetRegisteredPrefPropertiesStorage() = std::move(pref_properties_map);

  RegisterOldPrefs(registry);
#else
#if defined(OEM_MERCEDES_BUILD) || defined(OEM_LYNKCO_BUILD)
  registry->RegisterBooleanPref(vivaldiprefs::kBackgroundMediaPlaybackAllowed,
                                true);
#else
  registry->RegisterBooleanPref(vivaldiprefs::kBackgroundMediaPlaybackAllowed,
                                false);
#endif
#endif  // !BUILDFLAG(IS_ANDROID)
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

#if !BUILDFLAG(IS_ANDROID)
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
