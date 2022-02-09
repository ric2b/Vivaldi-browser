// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "browser/vivaldi_runtime_feature.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

#include "app/vivaldi_apptools.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "prefs/vivaldi_pref_names.h"

namespace vivaldi_runtime_feature {

namespace {

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

std::string LoadRuntimeFeaturesFile() {
  // This might be called outside the startup, eg. during creation of a guest
  // window, so need to allow IO.
  // base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FilePath path;

  bool ok;
#if defined(OS_MAC)
  ok = base::PathService::Get(chrome::DIR_RESOURCES, &path);
#else
  ok = base::PathService::Get(base::DIR_MODULE, &path);
#endif  // defined(OS_MAC)
  if (ok) {
    path = path.AppendASCII(kRuntimeFeaturesFilename);

    std::string features_text;
    if (base::ReadFileToString(path, &features_text))
      return features_text;
    LOG(ERROR) << "failed to read " << path;
  } else {
    LOG(ERROR) << "Unable to locate the \"flags\" preference file.";
  }
  return std::string();
}

absl::optional<FeatureMap> ParseFeatures(base::Value json) {
  if (!json.is_dict())
    return absl::nullopt;
  base::Value* flags_dict = json.FindKey("flags");
  if (!flags_dict || !flags_dict->is_dict())
    return absl::nullopt;

  std::vector<std::pair<std::string, Feature>> features;
  for (auto feature_iter : flags_dict->DictItems()) {
    const std::string& feature_name = feature_iter.first;
    base::Value& dict = feature_iter.second;
    if (!dict.is_dict()) {
      LOG(WARNING) << "Invalid entry in \"" << kRuntimeFeaturesFilename
                   << "\" file.";
      continue;
    }
    Feature feature;
    if (std::string* value = dict.FindStringKey("description")) {
      feature.description = std::move(*value);
    }
    if (std::string* value = dict.FindStringKey("friendly_name")) {
      feature.friendly_name = std::move(*value);
    }
#if !defined(OFFICIAL_BUILD)
    if (absl::optional<bool> value = dict.FindBoolKey("internal_value")) {
      if (*value) {
        feature.forced = true;
      }
    }
#endif
    if (const std::string* value = dict.FindStringKey("os")) {
      std::vector<base::StringPiece> os_list = base::SplitStringPiece(
          *value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      base::StringPiece current_os;
#if defined(OS_MAC)
      current_os = "mac";
#elif defined(OS_WIN)
      current_os = "win";
#elif defined(OS_LINUX)
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
      feature.forced = true;
    }
    switch_name = "disable-feature:";
    switch_name.append(name);
    if (command_line.HasSwitch(switch_name)) {
      // always disable this feature and force it always off
      feature.forced = false;
    }
  }

  return FeatureMap(std::move(features));
}

EnabledSet CreateEnabledSet(PrefService* prefs) {
  std::vector<std::string> enabled_list;
  base::Value::ConstListView list_view;
  const base::Value* list_value = prefs->Get(vivaldiprefs::kVivaldiExperiments);
  if (list_value && list_value->is_list()) {
    list_view = list_value->GetList();
  }
  for (const auto& name_feature : GetFeatureMapStorage()) {
    bool enabled;
    if (name_feature.second.forced) {
      enabled = name_feature.second.forced.value();
    } else {
      auto i = std::find_if(
          list_view.begin(), list_view.end(), [&](const base::Value& v) {
            return v.is_string() && v.GetString() == name_feature.first;
          });
      enabled = (i != list_view.end());
    }
    if (enabled) {
      enabled_list.push_back(name_feature.first);
    }
  }
  return EnabledSet(enabled_list);
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

  std::string features_text = LoadRuntimeFeaturesFile();
  absl::optional<base::Value> json = base::JSONReader::Read(features_text);
  if (!json) {
    LOG(ERROR) << kRuntimeFeaturesFilename << " does not contain a valid JSON.";
    return;
  }
  absl::optional<FeatureMap> feature_map = ParseFeatures(std::move(*json));
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
               base::StringPiece feature_name) {
  if (!g_initialized)
    return false;

  // Feature must exist.
  DCHECK(GetFeatureMapStorage().contains(feature_name));
  bool enabled = GetEnabledImpl(browser_context).contains(feature_name);
  return enabled;
}

bool Enable(content::BrowserContext* browser_context,
            base::StringPiece feature_name,
            bool enabled) {
  DCHECK(g_initialized);
  const FeatureMap& feature_map = GetFeatureMapStorage();
  auto feature_iter = feature_map.find(feature_name);

  // Feature must exist
  DCHECK(feature_iter != feature_map.end());
  if (feature_iter == feature_map.end())
    return false;
  if (feature_iter->second.forced)
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
  ListPrefUpdate update(profile->GetPrefs(), vivaldiprefs::kVivaldiExperiments);
  base::Value* list_value = update.Get();
  if (enabled) {
    base::Value::ConstListView list_view = list_value->GetList();
    base::Value v(feature_name);
    auto i = std::find(list_view.begin(), list_view.end(), v);
    if (i == list_view.end()) {
      list_value->Append(std::move(v));
    }
  } else {
    list_value->EraseListValue(base::Value(feature_name));
  }
  // Remove no longer supported values from the list to prevent junk
  // accumulation.
  list_value->EraseListValueIf([&](const base::Value& v) {
    return !v.is_string() || !feature_map.contains(v.GetString());
  });
  return true;
}

}  // namespace vivaldi_runtime_feature
