// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "browser/vivaldi_runtime_feature.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

#include "app/vivaldi_apptools.h"
#include "components/datasource/resource_reader.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "prefs/vivaldi_pref_names.h"

namespace vivaldi_runtime_feature {

namespace {

// We store the features that the user changed as a list in preferences. For the
// feature that the user enabled the list contain its name, for the feature that
// the user disabled the name is prefixed with this symbol.
constexpr char negation_prefix = '^';

bool g_initialized = false;

const char kRuntimeFeaturesFilename[] = "features.json";

struct EnabledSetHolder : public base::SupportsUserData::Data {
  EnabledSet enabled_set;
};

FeatureMap& GetFeatureMapStorage() {
  static base::NoDestructor<FeatureMap> feature_map;
  return *feature_map;
}

// Inlined Profile::FromBrowserContext() to avoid link dependency on
// chrome/browser.
Profile* ProfileFromBrowserContext(content::BrowserContext* browser_context) {
  return static_cast<Profile*>(browser_context);
}

std::optional<FeatureMap> ParseFeatures(base::Value json) {
  if (!json.is_dict())
    return std::nullopt;
  base::Value* flags_dict = json.GetDict().Find("flags");
  if (!flags_dict || !flags_dict->is_dict())
    return std::nullopt;

  std::vector<std::pair<std::string, Feature>> features;
  for (auto feature_iter : flags_dict->GetDict()) {
    const std::string& feature_name = feature_iter.first;
    base::Value& dict = feature_iter.second;
    if (!dict.is_dict()) {
      LOG(WARNING) << "Invalid entry in \"" << kRuntimeFeaturesFilename
                   << "\" file.";
      continue;
    }
    Feature feature;
    if (std::string* value = dict.GetDict().FindString("description")) {
      feature.description = std::move(*value);
    }
    if (std::string* value = dict.GetDict().FindString("friendly_name")) {
      feature.friendly_name = std::move(*value);
    }
    if (std::optional<bool> value = dict.GetDict().FindBool("value")) {
      if (*value) {
        feature.default_value = true;
      }
    }
#if !defined(OFFICIAL_BUILD)
    if (std::optional<bool> value = dict.GetDict().FindBool("internal_value")) {
      if (*value) {
        feature.default_value = true;
      }
    }
    if (std::optional<bool> value =
            dict.GetDict().FindBool("internal_locked")) {
      if (*value) {
        feature.locked = true;
      }
    }
#endif
    if (const std::string* value = dict.GetDict().FindString("os")) {
      std::vector<std::string_view> os_list = base::SplitStringPiece(
          *value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      std::string_view current_os;
#if BUILDFLAG(IS_MAC)
      current_os = "mac";
#elif BUILDFLAG(IS_WIN)
      current_os = "win";
#elif BUILDFLAG(IS_LINUX)
      current_os = "linux";
#else
#error "Unsupported platform"
#endif
      if (std::find(os_list.begin(), os_list.end(), current_os) ==
          os_list.end()) {
        feature.inactive = true;
      }
    }

    features.emplace_back(feature_name, std::move(feature));
  }

  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  // Check if this has been enabled or disabled on the cmd line.
  std::string switch_name;
  for (auto& name_feature : features) {
    const std::string& name = name_feature.first;
    Feature& feature = name_feature.second;
    switch_name = "enable-feature:";
    switch_name.append(name);

    if (command_line.HasSwitch(switch_name)) {
      // always enable this feature and force it always on
      feature.default_value = true;
      feature.locked = true;
    }
    switch_name = "disable-feature:";
    switch_name.append(name);
    if (command_line.HasSwitch(switch_name)) {
      // always disable this feature and force it always off
      feature.default_value = false;
      feature.locked = true;
    }
  }

  return FeatureMap(std::move(features));
}

EnabledSet CreateEnabledSet(PrefService* prefs) {
  // Build name->enabled map based on default values.
  std::vector<std::pair<std::string, bool>> name_enabled_list;
  for (const auto& name_feature : GetFeatureMapStorage()) {
    name_enabled_list.emplace_back(name_feature.first,
                                   name_feature.second.default_value);
  }
  base::flat_map<std::string, bool> name_enabled = std::move(name_enabled_list);

  // Override with the user preferences.
  const base::Value& list_value = prefs->GetValue(vivaldiprefs::kVivaldiExperiments);
  if (list_value.is_list()) {
    for (const base::Value& v : list_value.GetList()) {
      if (v.is_string()) {
        std::string_view name(v.GetString());
        bool enabled = true;
        if (!name.empty() && name[0] == negation_prefix) {
          enabled = false;
          name.remove_prefix(1);
        }
        auto i = name_enabled.find(name);
        if (i != name_enabled.end()) {
          i->second = enabled;
        }
      }
    }
  }

  // Override with locked values (those include values set from the command
  // line).
  for (const auto& name_feature : GetFeatureMapStorage()) {
    if (name_feature.second.locked) {
      name_enabled[name_feature.first] = name_feature.second.default_value;
    }
  }

  // Convert enabled names into a set.
  std::vector<std::string> enabled_list;
  for (auto& name_enabled_pair : std::move(name_enabled).extract()) {
    if (name_enabled_pair.second) {
      enabled_list.push_back(std::move(name_enabled_pair.first));
    }
  }
  return EnabledSet(std::move(enabled_list));
}

EnabledSet& GetEnabledImpl(content::BrowserContext* browser_context) {
  DCHECK(g_initialized);
  static const char kContextKey[1] = {'\0'};
  Profile* profile = ProfileFromBrowserContext(browser_context);
  profile = profile->GetOriginalProfile();
  EnabledSetHolder* holder =
      static_cast<EnabledSetHolder*>(profile->GetUserData(kContextKey));
  if (!holder) {
    auto holder_instance = std::make_unique<EnabledSetHolder>();
    holder_instance->enabled_set = CreateEnabledSet(profile->GetPrefs());
    holder = holder_instance.get();
    browser_context->SetUserData(kContextKey, std::move(holder_instance));
  }
  return holder->enabled_set;
}

}  // namespace

Feature::Feature() = default;
Feature::~Feature() = default;
Feature::Feature(const Feature&) = default;
Feature& Feature::operator=(const Feature&) = default;

void Init() {
  if (!vivaldi::IsVivaldiRunning())
    return;

  std::optional<base::Value> json =
      ResourceReader::ReadJSON("", kRuntimeFeaturesFilename);
  if (!json) {
    return;
  }
  std::optional<FeatureMap> feature_map = ParseFeatures(std::move(*json));
  if (!feature_map) {
    LOG(ERROR) << "Invalid structure of JSON in " << kRuntimeFeaturesFilename;
    return;
  }

  g_initialized = true;
  GetFeatureMapStorage() = std::move(*feature_map);
}

const FeatureMap& GetAllFeatures() {
  return GetFeatureMapStorage();
}

const EnabledSet* GetEnabled(content::BrowserContext* browser_context) {
  if (!g_initialized)
    return nullptr;
  return &GetEnabledImpl(browser_context);
}

bool IsEnabled(content::BrowserContext* browser_context,
               std::string_view feature_name) {
  if (!g_initialized)
    return false;

  // Feature must exist.
  DCHECK(GetFeatureMapStorage().contains(feature_name));
  bool enabled = GetEnabledImpl(browser_context).contains(feature_name);
  return enabled;
}

bool Enable(content::BrowserContext* browser_context,
            std::string_view feature_name,
            bool enabled) {
  DCHECK(g_initialized);
  const FeatureMap& feature_map = GetFeatureMapStorage();
  auto feature_iter = feature_map.find(feature_name);

  // Feature must exist
  DCHECK(feature_iter != feature_map.end());
  if (feature_iter == feature_map.end())
    return false;
  if (feature_iter->second.locked)
    return false;

  EnabledSet& enabled_set = GetEnabledImpl(browser_context);
  if (enabled) {
    enabled_set.insert(feature_iter->first);
  } else {
    enabled_set.erase(feature_name);
  }

  // Store the value in preferences.
  Profile* profile = ProfileFromBrowserContext(browser_context);
  profile = profile->GetOriginalProfile();
  auto& list_value =
      profile->GetPrefs()->GetList(vivaldiprefs::kVivaldiExperiments);
  base::Value::List updated;
  for (const base::Value& v : list_value) {
    if (!v.is_string())
      continue;
    std::string_view name = v.GetString();
    if (name.empty())
      continue;
    if (name[0] == negation_prefix) {
      name.remove_prefix(1);
    }

    // Remove no longer supported values from the list to prevent junk
    // accumulation.
    if (!feature_map.contains(name))
      continue;

    // The enabled/disabled feature is pushed to the list after the loop
    // according to enabled/disabled state.
    if (feature_name == name)
      continue;

    updated.Append(v.Clone());
  }

  std::string value_string(feature_name.data(), feature_name.size());
  if (!enabled) {
    value_string.insert(0, 1, negation_prefix);
  }
  updated.Append(std::move(value_string));

  profile->GetPrefs()->Set(vivaldiprefs::kVivaldiExperiments,
                           base::Value(std::move(updated)));

  return true;
}

}  // namespace vivaldi_runtime_feature
