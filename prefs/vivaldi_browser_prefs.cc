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
#include "chrome/common/chrome_paths.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/version_info/version_info.h"
#include "prefs/vivaldi_pref_names.h"

namespace vivaldi {

const base::FilePath::CharType kVivaldiResourcesFolder[] =
    FILE_PATH_LITERAL("vivaldi");
const base::FilePath::CharType kPrefsDefinitionFileName[] =
    FILE_PATH_LITERAL("prefs_definitions.json");

PrefProperties::PrefProperties(
    std::unordered_map<std::string, int> string_to_value,
    std::unordered_map<int, std::string> value_to_string) {
  enum_properties.reset(new EnumProperties);
  enum_properties->value_to_string = std::move(value_to_string);
  enum_properties->string_to_value = std::move(string_to_value);
}

PrefProperties::PrefProperties() = default;
PrefProperties::PrefProperties(PrefProperties&&) = default;
PrefProperties::~PrefProperties() = default;

PrefProperties::EnumProperties::EnumProperties() = default;
PrefProperties::EnumProperties::EnumProperties(EnumProperties&&) = default;
PrefProperties::EnumProperties::~EnumProperties() = default;

void RegisterLocalState(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(vivaldiprefs::kVivaldiUniqueUserId, "");
}

void RegisterPrefsFromDefinition(const base::DictionaryValue* definition,
                                 const std::string& current_path,
                                 user_prefs::PrefRegistrySyncable* registry,
                                 PrefsProperties* prefs_properties) {
  if (!definition->HasKey("type")) {
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

  bool syncable;
  if (!definition->GetBoolean("syncable", &syncable)) {
    LOG(DFATAL) << "Expected a boolean at '" << current_path << ".syncable'";
  }

  int flags = syncable ? user_prefs::PrefRegistrySyncable::SYNCABLE_PREF : 0;

  std::string type;
  if (!definition->GetString("type", &type)) {
    LOG(DFATAL) << "Expected a string or a list at '" << current_path
                << ".type'";
  }

  if (type.compare("enum") == 0) {
    const base::DictionaryValue* enum_values;
    if (!definition->GetDictionary("enum_values", &enum_values))
      LOG(DFATAL) << "Expected a dictionary at '" << current_path
                  << ".enum_values'";

    std::string default_string;
    if (!definition->GetString("default", &default_string))
      LOG(DFATAL) << "Expected a string at '" << current_path << ".default'";

    std::unordered_map<std::string, int> string_to_value;
    std::unordered_map<int, std::string> value_to_string;
    bool found_default = false;
    int default_value = 0;
    for (const auto& enum_value : *enum_values) {
      if (!enum_value.second->is_int())
        LOG(DFATAL) << "Expected an integer at '" << current_path
                    << ".enum_values." << enum_value.first << "'";
      string_to_value.insert(
          std::make_pair(enum_value.first, enum_value.second->GetInt()));
      value_to_string.insert(
          std::make_pair(enum_value.second->GetInt(), enum_value.first));

      if (default_string == enum_value.first) {
        default_value = enum_value.second->GetInt();
        found_default = true;
      }
    }

    if (!found_default)
      LOG(DFATAL) << "Default value for enum isn't part of possible values at '"
                  << current_path << "'";

    prefs_properties->insert(std::make_pair(
        current_path, PrefProperties(std::move(string_to_value),
                                     std::move(value_to_string))));
    registry->RegisterIntegerPref(current_path, default_value, flags);
    return;
  }

  if (type.compare("string") == 0) {
    std::string default_value;
    if (!definition->GetString("default", &default_value))
      LOG(DFATAL) << "Expected a string at '" << current_path << ".default'";

    prefs_properties->insert(std::make_pair(current_path, PrefProperties()));
    registry->RegisterStringPref(current_path, default_value, flags);

  } else if (type.compare("file_path") == 0) {
    base::FilePath::StringType default_value;
    if (!definition->GetString("default", &default_value))
      LOG(DFATAL) << "Expected a string at '" << current_path << ".default'";

    prefs_properties->insert(std::make_pair(current_path, PrefProperties()));
    registry->RegisterFilePathPref(current_path, base::FilePath(default_value),
                                   flags);

  } else if (type.compare("boolean") == 0) {
    bool default_value;
    if (!definition->GetBoolean("default", &default_value))
      LOG(DFATAL) << "Expected a boolean at '" << current_path << ".default'";

    prefs_properties->insert(std::make_pair(current_path, PrefProperties()));
    registry->RegisterBooleanPref(current_path, default_value, flags);

  } else if (type.compare("integer") == 0) {
    int default_value;
    if (!definition->GetInteger("default", &default_value))
      LOG(DFATAL) << "Expected an integer at '" << current_path << ".default'";

    prefs_properties->insert(std::make_pair(current_path, PrefProperties()));
    registry->RegisterIntegerPref(current_path, default_value, flags);

  } else if (type.compare("double") == 0) {
    double default_value;
    if (!definition->GetDouble("default", &default_value))
      LOG(DFATAL) << "Expected a double at '" << current_path << ".default'";

    prefs_properties->insert(std::make_pair(current_path, PrefProperties()));
    registry->RegisterDoublePref(current_path, default_value, flags);

  } else if (type.compare("list") == 0) {
    const base::Value* default_value =
        definition->FindKeyOfType("default", base::Value::Type::LIST);
    if (!default_value)
      LOG(DFATAL) << "Expected a list at '" << current_path << ".default'";

    prefs_properties->insert(std::make_pair(current_path, PrefProperties()));
    registry->RegisterListPref(
        current_path,
        base::ListValue::From(
            std::make_unique<base::Value>(default_value->Clone())),
        flags);

  } else if (type.compare("dictionary") == 0) {
    const base::Value* default_value =
        definition->FindKeyOfType("default", base::Value::Type::DICTIONARY);
    if (!default_value)
      LOG(DFATAL) << "Expected a dictionary at '" << current_path
                  << ".default'";

    prefs_properties->insert(std::make_pair(current_path, PrefProperties()));
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
#endif // !OS_ANDROID
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
  if (!prefs_definitions->GetDictionary("vivaldi", &vivaldi_pref_definition)) {
    LOG(DFATAL) << "Expected a dictionary at 'vivaldi'";
  }

  base::DictionaryValue* chromium_prefs_list = nullptr;
  if (!prefs_definitions->GetDictionary("chromium", &chromium_prefs_list)) {
    LOG(DFATAL) << "Expected a dictionary at 'chromium'";
  }

  PrefsProperties prefs_properties;

  RegisterPrefsFromDefinition(vivaldi_pref_definition, "vivaldi", registry,
                              &prefs_properties);

  for (const auto& pref : *chromium_prefs_list) {
    std::string pref_path;
    if (!pref.second->GetAsString(&pref_path)) {
      LOG(DFATAL) << "Expected a string at 'chromium." << pref.first;
    }
    prefs_properties.insert(std::make_pair(pref_path, PrefProperties()));
  }

  return prefs_properties;
}

}  // namespace vivaldi
