// Copyright 2017 Vivaldi Technologies

#include "prefs/vivaldi_prefs_definitions.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "apps/switches.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
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
#include "components/datasource/resource_reader.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/vivaldi_pref_names.h"
#include "components/sync_preferences/syncable_prefs_database.h"
#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if !BUILDFLAG(IS_IOS)
#include "chrome/common/pref_names.h"
#endif // !IS_IOS

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

base::Value GetPlatformComputedDefault(const std::string& path);

namespace {

// Not an enum class to ease cast to int.
namespace syncable_prefs_ids {
// According to chromium code, These values are only used for histograms. While
// we don't care about those, we should probably give them sensible values in
// case they actually end up being used for sync itself.
enum {
  // Starts with 1000000 to avoid clash with prefs listed in
  // chrome_syncable_prefs_database.cc,
  // common_syncable_prefs_database.cc and
  // ios_chrome_syncable_prefs_database.cc.
  kSyncedDefaultPrivateSearchProviderGUID = 1000000,
  kSyncedDefaultSearchFieldProviderGUID = 1000001,
  kSyncedDefaultPrivateSearchFieldProviderGUID = 1000002,
  kSyncedDefaultSpeedDialsSearchProviderGUID = 1000003,
  kSyncedDefaultSpeedDialsPrivateSearchProviderGUID = 1000004,
  kSyncedDefaultImageSearchProviderGUID = 1000005,
};

// Prefs from the prefs_definitions.json have their own ids starting at 1.
// We add this number so that they don't collide with anything.
constexpr int kLowestIdForPrefsDefinitions = 1500000;
}  // namespace syncable_prefs_ids

const auto& SyncablePreferences() {
  // Non-iOS specific list of syncable preferences.
  static const auto kVivaldiSyncablePrefsAllowlist = base::MakeFixedFlatMap<
      std::string_view, sync_preferences::SyncablePrefMetadata>({
      {prefs::kSyncedDefaultPrivateSearchProviderGUID,
       {syncable_prefs_ids::kSyncedDefaultPrivateSearchProviderGUID,
        syncer::PREFERENCES, sync_preferences::PrefSensitivity::kNone,
        sync_preferences::MergeBehavior::kNone}},
      {prefs::kSyncedDefaultSearchFieldProviderGUID,
       {syncable_prefs_ids::kSyncedDefaultSearchFieldProviderGUID,
        syncer::PREFERENCES, sync_preferences::PrefSensitivity::kNone,
        sync_preferences::MergeBehavior::kNone}},
      {prefs::kSyncedDefaultPrivateSearchFieldProviderGUID,
       {syncable_prefs_ids::kSyncedDefaultPrivateSearchFieldProviderGUID,
        syncer::PREFERENCES, sync_preferences::PrefSensitivity::kNone,
        sync_preferences::MergeBehavior::kNone}},
      {prefs::kSyncedDefaultSpeedDialsSearchProviderGUID,
       {syncable_prefs_ids::kSyncedDefaultSpeedDialsSearchProviderGUID,
        syncer::PREFERENCES, sync_preferences::PrefSensitivity::kNone,
        sync_preferences::MergeBehavior::kNone}},
      {prefs::kSyncedDefaultSpeedDialsPrivateSearchProviderGUID,
       {syncable_prefs_ids::kSyncedDefaultSpeedDialsPrivateSearchProviderGUID,
        syncer::PREFERENCES, sync_preferences::PrefSensitivity::kNone,
        sync_preferences::MergeBehavior::kNone}},
      {prefs::kSyncedDefaultImageSearchProviderGUID,
       {syncable_prefs_ids::kSyncedDefaultImageSearchProviderGUID,
        syncer::PREFERENCES, sync_preferences::PrefSensitivity::kNone,
        sync_preferences::MergeBehavior::kNone}},
  });
  return kVivaldiSyncablePrefsAllowlist;
}

const char kPrefsDefinitionFileName[] = "prefs_definitions.json";

const char kVivaldiKeyName[] = "vivaldi";
#if !BUILDFLAG(IS_ANDROID)
const char kChromiumKeyName[] = "chromium";
const char kChromiumLocalKeyName[] = "chromium_local";
#endif  // !BUILDFLAG(IS_ANDROID)

const char kTypeKeyName[] = "type";
const char kDefaultKeyName[] = "default";
const char kSyncableKeyName[] = "syncable";
const char kSyncIdKeyName[] = "id";
const char kSyncMergeMethodKeyName[] = "merge_method";
const char kEnumValuesKey[] = "enum_values";

const char kTypeEnumName[] = "enum";
const char kTypeStringName[] = "string";
const char kTypeFilePathName[] = "file_path";
const char kTypeBooleanName[] = "boolean";
const char kTypeIntegerName[] = "integer";
const char kTypeDoubleName[] = "double";
const char kTypeListName[] = "list";
const char kTypeDictionaryName[] = "dictionary";

#ifdef VIVALDI_PREFERENCE_OVERRIDE_FILE

namespace prefs_overrides {
// Extra file to allow overriding of some preferences without changing the
// main file.
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

struct PrefOverrideValues {
  std::string theme_default;
  std::string theme_default_day_or_night[2];
  std::string theme_default_private;

  base::Value theme_extra;
};

// Patch default preferences according to the override JSON. The code trusts
// prefs structure (in the release builds erroneous assumptions will CHECK()
// or crash on null pointer), but verifies the overrides reporting errors
// there.
void PatchPrefsJson(base::Value::Dict& prefs, base::Value& overrides) {
  bool has_errors = false;
  auto error = [&](std::string message) {
    LOG(ERROR) << prefs_overrides::kFileName << ": " << message;
    has_errors = true;
  };
  auto append_default = [](std::string_view path) {
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
    std::string_view name = name_value.first;
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
  ResourceReader reader_main(kPrefsDefinitionFileName);
  std::optional<base::Value> dictionary_value = reader_main.ParseJSON();
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
  std::optional<base::Value> overrides = reader_overrides.ParseJSON();
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

VivaldiPrefsDefinitions::PrefProperties::PrefProperties() = default;
VivaldiPrefsDefinitions::PrefProperties::~PrefProperties() = default;
VivaldiPrefsDefinitions::PrefProperties::PrefProperties(PrefProperties&&) =
    default;
VivaldiPrefsDefinitions::PrefProperties&
VivaldiPrefsDefinitions::PrefProperties::operator=(PrefProperties&&) = default;

VivaldiPrefsDefinitions::PrefDefinition::PrefDefinition() = default;
VivaldiPrefsDefinitions::PrefDefinition::~PrefDefinition() = default;
VivaldiPrefsDefinitions::PrefDefinition::PrefDefinition(PrefDefinition&&) =
    default;
VivaldiPrefsDefinitions::PrefDefinition&
VivaldiPrefsDefinitions::PrefDefinition::operator=(PrefDefinition&&) = default;

VivaldiPrefsDefinitions::EnumPrefValues::EnumPrefValues() = default;
VivaldiPrefsDefinitions::EnumPrefValues::~EnumPrefValues() = default;
VivaldiPrefsDefinitions::EnumPrefValues::EnumPrefValues(EnumPrefValues&&) =
    default;
VivaldiPrefsDefinitions::EnumPrefValues&
VivaldiPrefsDefinitions::EnumPrefValues::operator=(EnumPrefValues&&) = default;

VivaldiPrefsDefinitions::SyncedPrefProperties::SyncedPrefProperties() = default;
VivaldiPrefsDefinitions::SyncedPrefProperties::~SyncedPrefProperties() =
    default;
VivaldiPrefsDefinitions::SyncedPrefProperties::SyncedPrefProperties(
    SyncedPrefProperties&&) = default;
VivaldiPrefsDefinitions::SyncedPrefProperties&
VivaldiPrefsDefinitions::SyncedPrefProperties::operator=(
    SyncedPrefProperties&&) = default;

std::optional<int> VivaldiPrefsDefinitions::EnumPrefValues::FindValue(
    std::string_view name) const {
  for (const auto& i : name_value_pairs) {
    if (i.first == name)
      return i.second;
  }
  return std::nullopt;
}

const std::string* VivaldiPrefsDefinitions::EnumPrefValues::FindName(
    int value) const {
  for (const auto& i : name_value_pairs) {
    if (i.second == value)
      return &i.first;
  }
  return nullptr;
}

/*static*/
VivaldiPrefsDefinitions* VivaldiPrefsDefinitions::GetInstance() {
  static base::NoDestructor<VivaldiPrefsDefinitions> instance;
  return instance.get();
}

VivaldiPrefsDefinitions::VivaldiPrefsDefinitions() {
  base::Value::Dict prefs_definitions = ReadPrefsJson();

  base::Value::Dict* vivaldi_pref_definitions =
      prefs_definitions.FindDict(kVivaldiKeyName);
  if (!vivaldi_pref_definitions) {
    LOG(FATAL) << "Expected a dictionary at '" << kVivaldiKeyName << "'";
  }

  base::Value::Dict* syncable_pref_paths =
      prefs_definitions.FindDict(kSyncableKeyName);
  if (!syncable_pref_paths) {
    LOG(FATAL) << "Expected a dictionary at '" << kSyncableKeyName << "'";
  }

  AddPropertiesFromDefinition(*vivaldi_pref_definitions, *syncable_pref_paths,
                              kVivaldiKeyName);

#if !BUILDFLAG(IS_ANDROID)
  AddChromiumProperties(prefs_definitions, kChromiumKeyName, false);
  AddChromiumProperties(prefs_definitions, kChromiumLocalKeyName, true);
#endif  // !BUILDFLAG(IS_ANDROID)
}

VivaldiPrefsDefinitions::~VivaldiPrefsDefinitions() = default;

#if !BUILDFLAG(IS_ANDROID)

void VivaldiPrefsDefinitions::AddChromiumProperties(
    base::Value::Dict& prefs,
    std::string_view current_path,
    bool local_pref) {
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
    properties.local_pref = local_pref;
    pref_properties_.emplace(std::move(*pref_path), std::move(properties));
  }
}

#endif  // !BUILDFLAG(IS_ANDROID)

void VivaldiPrefsDefinitions::AddPropertiesFromDefinition(
    base::Value::Dict& definition,
    base::Value::Dict& syncable_paths,
    std::string current_path) {
  const base::Value* type_value = definition.Find(kTypeKeyName);
  if (!type_value) {
    for (auto child : definition) {
      std::string child_path = current_path;
      child_path += ".";
      child_path += child.first;
      if (!child.second.is_dict()) {
        LOG(FATAL) << "Expected a dictionary at '" << child_path << "'";
      }
      AddPropertiesFromDefinition(child.second.GetDict(), syncable_paths,
                                  std::move(child_path));
    }
    return;
  }

  if (!type_value->is_string()) {
    LOG(FATAL) << "Expected a string or a list at '" << current_path << "."
               << kTypeKeyName << "'";
  }

  PrefDefinition result;
  base::Value::Type value_type = base::Value::Type::NONE;
  const std::string& type_str = type_value->GetString();
  if (type_str == kTypeEnumName) {
    result.pref_kind = PrefKind::kEnum;
    value_type = base::Value::Type::INTEGER;
  } else if (type_str == kTypeStringName) {
    result.pref_kind = PrefKind::kString;
    value_type = base::Value::Type::STRING;
  } else if (type_str == kTypeFilePathName) {
    result.pref_kind = PrefKind::kFilePath;
    value_type = base::Value::Type::STRING;
  } else if (type_str == kTypeBooleanName) {
    result.pref_kind = PrefKind::kBoolean;
    value_type = base::Value::Type::BOOLEAN;
  } else if (type_str == kTypeIntegerName) {
    result.pref_kind = PrefKind::kInteger;
    value_type = base::Value::Type::INTEGER;
  } else if (type_str == kTypeDoubleName) {
    result.pref_kind = PrefKind::kDouble;
    value_type = base::Value::Type::DOUBLE;
  } else if (type_str == kTypeListName) {
    result.pref_kind = PrefKind::kList;
    value_type = base::Value::Type::LIST;
  } else if (type_str == kTypeDictionaryName) {
    result.pref_kind = PrefKind::kDictionary;
    value_type = base::Value::Type::DICT;
  }
  if (result.pref_kind == PrefKind::kNone) {
    LOG(FATAL) << "Invalid type value at '" << current_path << "'";
  }

  base::Value::Dict* syncable_path = syncable_paths.FindDict(current_path);
  if (syncable_path) {
    result.sync_properties.emplace();
    std::optional<int> sync_id = syncable_path->FindInt(kSyncIdKeyName);
    if (!sync_id) {
      LOG(FATAL) << "Expected an integer at '" << kSyncableKeyName << ".\""
                 << current_path << "\"." << kSyncIdKeyName;
    }
    result.sync_properties->id = *sync_id;
    std::string* sync_method =
        syncable_path->FindString(kSyncMergeMethodKeyName);
    if (sync_method && *sync_method != "none") {
      if (*sync_method == "merge") {
        result.sync_properties->merge_behavior =
            sync_preferences::MergeBehavior::kMergeableListWithRewriteOnUpdate;
      } else {
        LOG(FATAL) << "Expected one of 'none' or 'merge' at '"
                   << kSyncableKeyName << ".\"" << current_path << "\"."
                   << kSyncMergeMethodKeyName;
      }
    }
  }

  const char* default_key_name = vivaldiprefs::kPlatformDefaultKeyName;
  base::Value* default_value_ptr = definition.Find(default_key_name);
  if (!default_value_ptr) {
    default_key_name = kDefaultKeyName;
    default_value_ptr = definition.Find(default_key_name);
  }

  bool default_is_computed = !default_value_ptr || default_value_ptr->is_none();
  base::Value default_value;

  if (!default_is_computed) {
    default_value = std::move(*default_value_ptr);
  } else {
    default_value = GetComputedDefault(current_path);
    if (default_value.is_none()) {
      // The preference is not defined for the current platform.
      return;
    }
  }

  if (result.pref_kind == PrefKind::kEnum) {
    const base::Value::Dict* enum_dict = definition.FindDict(kEnumValuesKey);
    if (!enum_dict) {
      LOG(FATAL) << "Expected a dictionary at '" << current_path << "."
                 << kEnumValuesKey << "'";
    }

    EnumPrefValues enum_values;
    enum_values.name_value_pairs.reserve(enum_dict->size());
    for (auto enum_name_value : *enum_dict) {
      const std::string& name = enum_name_value.first;
      std::optional<int> value = enum_name_value.second.GetIfInt();
      if (!value) {
        LOG(FATAL) << "Expected an integer at '" << current_path << "."
                   << kEnumValuesKey << "." << enum_name_value.first << "'";
      }
      if (enum_values.FindValue(name)) {
        LOG(FATAL) << "Duplicated enum case at '" << current_path << "."
                   << kEnumValuesKey << "." << name << "'";
      }
      if (enum_values.FindName(*value)) {
        LOG(FATAL) << "Duplicated enum value at '" << current_path << "."
                   << kEnumValuesKey << "." << name << "'";
      }
      enum_values.name_value_pairs.emplace_back(name, *value);
    }

    if (default_value.is_string()) {
      std::optional<int> int_value =
          enum_values.FindValue(default_value.GetString());
      if (int_value)
        result.default_value = base::Value(*int_value);
    } else if (default_value.is_int() &&
               enum_values.FindName(default_value.GetInt())) {
      result.default_value = std::move(default_value);
    }

    if (result.default_value.is_none()) {
      LOG(FATAL) << "Default value for enum isn't part of possible values at '"
                 << current_path << "'";
    }
    result.enum_values = std::move(enum_values);
  } else {
    if (default_value.type() != value_type) {
      if (default_is_computed) {
        LOG(FATAL) << "Unexpected type of computed default value for '"
                   << current_path << "' - " << default_value.type();
      } else {
        LOG(FATAL) << "Unexpected type for '" << current_path << "."
                   << default_key_name << "' - " << default_value.type();
      }
    }
    result.default_value = std::move(default_value);
  }

  PrefProperties properties;
  properties.local_pref = false;
  properties.definition.emplace(std::move(result));
  pref_properties_.emplace(std::move(current_path), std::move(properties));
}

base::Value VivaldiPrefsDefinitions::GetComputedDefault(
    const std::string& path) {
  if (path == vivaldiprefs::kWebpagesCaptureDirectory) {
    base::FilePath pictures_path;
    base::PathService::Get(chrome::DIR_USER_PICTURES, &pictures_path);
    pictures_path = pictures_path.AppendASCII("Vivaldi Captures");
    return base::Value(pictures_path.AsUTF16Unsafe());
  }
  return GetPlatformComputedDefault(path);
}

void VivaldiPrefsDefinitions::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // This pref is obsolete.
  registry->RegisterBooleanPref(vivaldiprefs::kAutoUpdateEnabled, true);

  registry->RegisterDictionaryPref(
      vivaldiprefs::kVivaldiAccountPendingRegistration);
  registry->RegisterListPref(vivaldiprefs::kVivaldiExperiments);
  registry->RegisterInt64Pref(vivaldiprefs::kVivaldiLastTopSitesVacuumDate, 0);
  registry->RegisterDictionaryPref(vivaldiprefs::kVivaldiPIPPlacement);

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(vivaldiprefs::kPWADisabled,
                                true);
#if defined(OEM_MERCEDES_BUILD) || defined(OEM_LYNKCO_BUILD)
  registry->RegisterBooleanPref(vivaldiprefs::kBackgroundMediaPlaybackAllowed,
                                true);
#else
  registry->RegisterBooleanPref(vivaldiprefs::kBackgroundMediaPlaybackAllowed,
                                false);
#endif
#endif  // !BUILDFLAG(IS_ANDROID)

  for (auto& [path, properties] : pref_properties_) {
    if (!properties.definition)
      continue;

    int flags = properties.definition->sync_properties
                    ? user_prefs::PrefRegistrySyncable::SYNCABLE_PREF
                    : 0;
    const base::Value& default_value = properties.definition->default_value;

    switch (properties.definition->pref_kind) {
      case PrefKind::kEnum:
        registry->RegisterIntegerPref(path, default_value.GetInt(), flags);
        break;
      case PrefKind::kString:
        registry->RegisterStringPref(path, default_value.GetString(), flags);
        break;
      case PrefKind::kFilePath:
        registry->RegisterFilePathPref(
            path, base::FilePath::FromUTF8Unsafe(default_value.GetString()),
            flags);
        break;
      case PrefKind::kBoolean:
        registry->RegisterBooleanPref(path, default_value.GetBool(), flags);
        break;
      case PrefKind::kInteger:
        registry->RegisterIntegerPref(path, default_value.GetInt(), flags);
        break;
      case PrefKind::kDouble:
        registry->RegisterDoublePref(path, default_value.GetDouble(), flags);
        break;
      case PrefKind::kList:
        registry->RegisterListPref(path, default_value.GetList().Clone(),
                                   flags);

        break;
      case PrefKind::kDictionary:
        registry->RegisterDictionaryPref(path, default_value.GetDict().Clone(),
                                         flags);
        break;
      case PrefKind::kNone:
        NOTREACHED();
    }
  }
}

void VivaldiPrefsDefinitions::MigrateObsoleteProfilePrefs(
      PrefService* profile_prefs) {
#if !BUILDFLAG(IS_IOS)
  if (profile_prefs->HasPrefPath(vivaldiprefs::kAddressBarInlineSearchSuggestEnabled)) {
    profile_prefs->SetBoolean(
        prefs::kSearchSuggestEnabled,
        profile_prefs->GetBoolean(vivaldiprefs::kAddressBarInlineSearchSuggestEnabled));
    profile_prefs->ClearPref(vivaldiprefs::kAddressBarInlineSearchSuggestEnabled);
  }
#endif // !IS_IOS
}

std::optional<sync_preferences::SyncablePrefMetadata>
VivaldiPrefsDefinitions::GetSyncablePrefMetadata(
    const std::string_view pref_name) const {
  const auto it = SyncablePreferences().find(pref_name);
  if (it != SyncablePreferences().end()) {
    return it->second;
  }

  const auto& item = pref_properties_.find(std::string(pref_name));
  if (item == pref_properties_.end() || !item->second.definition ||
      !item->second.definition->sync_properties) {
    return std::nullopt;
  }
  const SyncedPrefProperties& sync_properties =
      *(item->second.definition->sync_properties);

  return sync_preferences::SyncablePrefMetadata(
      sync_properties.id + syncable_prefs_ids::kLowestIdForPrefsDefinitions,
      syncer::PREFERENCES, sync_preferences::PrefSensitivity::kNone,
      sync_properties.merge_behavior);
}

}  // namespace vivaldi
