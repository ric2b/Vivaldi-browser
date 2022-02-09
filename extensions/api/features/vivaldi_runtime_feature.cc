// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/features/vivaldi_runtime_feature.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "prefs/vivaldi_pref_names.h"

namespace vivaldi_runtime_feature {

namespace {

const char kRuntimeFeaturesFilename[] = "features.json";

class FeatureService : public extensions::BrowserContextKeyedAPI {
 public:
  explicit FeatureService(content::BrowserContext* browser_context);
  ~FeatureService() override = default;

  FeatureService(const FeatureService&) = delete;
  FeatureService& operator=(const FeatureService&) = delete;

  EntryMap& entries() { return entries_; }

  // BrowserContextKeyedAPI implementation.
  using Factory = extensions::BrowserContextKeyedAPIFactory<FeatureService>;
  friend Factory;
  static const char* service_name() { return "FeatureService"; }
  static const bool kServiceIsNULLWhileTesting = false;
  static const bool kServiceIsCreatedWithBrowserContext = true;
  static const bool kServiceRedirectedInIncognito = true;
  static Factory* GetFactoryInstance() {
    static base::NoDestructor<Factory> instance;
    return &*instance;
  }

 private:
  bool LoadFlags(Profile* profile, base::Value json);

  EntryMap entries_;
};

std::string LoadRuntimeFeaturesFile() {
  // This might be called outside the startup, eg. during creation of a guest
  // window, so need to allow IO.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FilePath path;

#if defined(OS_MAC)
  if (base::PathService::Get(chrome::DIR_RESOURCES, &path)) {
#else
  if (base::PathService::Get(base::DIR_MODULE, &path)) {
#endif  // defined(OS_MAC)
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

FeatureService::FeatureService(content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  std::string features_text = LoadRuntimeFeaturesFile();
  absl::optional<base::Value> json = base::JSONReader::Read(features_text);
  if (!json) {
    LOG(ERROR) << kRuntimeFeaturesFilename << " does not contain a valid JSON.";
  } else if (!LoadFlags(profile, std::move(*json))) {
    LOG(ERROR) << "Invalid structure of JSON in " << kRuntimeFeaturesFilename;
  }
}

bool FeatureService::LoadFlags(Profile* profile, base::Value json) {
  if (!json.is_dict())
    return false;
  base::Value* dict = json.FindKey("flags");
  if (!dict || !dict->is_dict())
    return false;

  std::vector<std::pair<std::string, Entry>> entries;
  for (auto entry_iter : dict->DictItems()) {
    const std::string& entry_name = entry_iter.first;
    base::Value& entry_value = entry_iter.second;
    if (!entry_value.is_dict()) {
      LOG(WARNING) << "Invalid entry in \"" << kRuntimeFeaturesFilename
                   << "\" file.";
      continue;
    }
    Entry entry;
    for (auto iter : entry_value.DictItems()) {
      const std::string& key = iter.first;
      base::Value& sub_value = iter.second;
      if (key == "description") {
        if (sub_value.is_string()) {
          entry.description = std::move(sub_value.GetString());
        }
      } else if (key == "friendly_name") {
        if (sub_value.is_string()) {
          entry.friendly_name = std::move(sub_value.GetString());
        }
      } else if (key == "internal_value") {
        if (sub_value.is_bool()) {
          entry.internal_default_enabled = sub_value.GetBool();
        }
      }
    }
    entries.emplace_back(entry_name, std::move(entry));
  }

  entries_ = EntryMap(std::move(entries));

#if !defined(OFFICIAL_BUILD)
  for (auto& i : entries_) {
    Entry& entry = i.second;
    entry.enabled = entry.internal_default_enabled;
    if (entry.internal_default_enabled) {
      // Features enabled by default in internal builds can't be turned
      // off, they pop up again as enabled after restart. Disable the
      // ability to turn them off in the UI to avoid confusion.
      entry.force_value = true;
    }
  }
#endif

  // Check if this has been enabled or disabled on the cmd line.
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  std::string cmd_switch;
  for (auto& i : entries_) {
    const std::string& name = i.first;
    Entry& entry = i.second;
    cmd_switch = "enable-feature:";
    cmd_switch.append(name);

    if (cl->HasSwitch(cmd_switch)) {
      // always enable this feature and force it always on
      entry.enabled = true;
      entry.force_value = true;
    }
    cmd_switch = "disable-feature:";
    cmd_switch.append(name);
    if (cl->HasSwitch(cmd_switch)) {
      // always disable this feature and force it always off
      entry.enabled = false;
      entry.force_value = true;
    }
  }

  // Check for any features that might be enabled by the user now.
  const base::Value* list_value =
      profile->GetPrefs()->Get(vivaldiprefs::kVivaldiExperiments);
  if (list_value && list_value->is_list()) {
    base::Value::ConstListView list = list_value->GetList();
    for (auto& i : entries_) {
      const std::string& name = i.first;
      Entry& entry = i.second;
      if (!entry.force_value) {
        auto it = std::find(list.begin(), list.end(), base::Value(name));
        if (it != list.end()) {
          entry.enabled = true;
        }
      }
    }
  }

  return true;
}

EntryMap* GetEntryMap(content::BrowserContext* browser_context) {
  FeatureService* features = FeatureService::Factory::Get(browser_context);
  DCHECK(features);
  if (!features)
    return nullptr;
  return &features->entries();
}

Entry* FindNamedFeature(EntryMap* entries, base::StringPiece feature_name) {
  if (!entries)
    return nullptr;
  EntryMap::iterator it = entries->find(feature_name);
  if (it == entries->end())
    return nullptr;
  return &it->second;
}

}  // namespace

void Init() {
  FeatureService::GetFactoryInstance();
}

const EntryMap* GetAll(content::BrowserContext* browser_context) {
  return GetEntryMap(browser_context);
}

bool IsEnabled(content::BrowserContext* browser_context,
               base::StringPiece feature_name) {
  EntryMap* entries = GetEntryMap(browser_context);
  Entry* entry = FindNamedFeature(entries, feature_name);
  return entry && entry->enabled;
}

bool Enable(content::BrowserContext* browser_context,
            base::StringPiece feature_name,
            bool enabled) {
  EntryMap* entries = GetEntryMap(browser_context);
  Entry* entry = FindNamedFeature(entries, feature_name);
  if (!entry || entry->force_value)
    return false;

  // Update the entry in-memory only so the list is always up-to-date.
  entry->enabled = enabled;
  ListPrefUpdate update(
      Profile::FromBrowserContext(browser_context)->GetPrefs(),
      vivaldiprefs::kVivaldiExperiments);
  base::ListValue* experiments_list = update.Get();

  experiments_list->Clear();
  for (const auto& i : *entries) {
    // Enable it if it's different than the default
    if (i.second.enabled
#if !defined(OFFICIAL_BUILD)
        && !i.second.internal_default_enabled
#endif
    ) {
      experiments_list->AppendString(i.first);
    }
  }
  return true;
}

}  // namespace vivaldi_runtime_feature
