// Copyright (c) 2016-2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/runtime/runtime_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#if defined(OS_MACOSX)
#include "chrome/common/chrome_paths_internal.h"
#endif  // defined(OS_MACOSX)
#include "components/download/public/background_service/download_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/schema/runtime_private.h"
#include "prefs/vivaldi_pref_names.h"
#include "ui/devtools/devtools_connector.h"

using base::PathService;

namespace extensions {

const char kRuntimeFeaturesFilename[] = "features.json";

using vivaldi::runtime_private::FeatureFlagInfo;

FeatureEntry::FeatureEntry() {}

VivaldiRuntimeFeatures::VivaldiRuntimeFeatures(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  LoadRuntimeFeatures();
}

VivaldiRuntimeFeatures::VivaldiRuntimeFeatures()
    : profile_(nullptr), weak_ptr_factory_(this) {}

VivaldiRuntimeFeatures::~VivaldiRuntimeFeatures() {}

/* static */
bool VivaldiRuntimeFeatures::IsEnabled(Profile* profile,
                                       const std::string& feature_name) {
  extensions::VivaldiRuntimeFeatures* features =
      extensions::VivaldiRuntimeFeaturesFactory::GetForProfile(profile);
  DCHECK(features);
  FeatureEntry* feature = features->FindNamedFeature(feature_name);
  DCHECK(feature);
  return (feature && feature->enabled);
}

void VivaldiRuntimeFeatures::LoadRuntimeFeatures() {
  base::FilePath path;

#if defined(OS_MACOSX)
  path = chrome::GetVersionedDirectory();
  if (!path.empty()) {
#else
  if (PathService::Get(base::DIR_MODULE, &path)) {
#endif  // defined(OS_MACOSX)
    path = path.AppendASCII(kRuntimeFeaturesFilename);

    store_ = new JsonPrefStore(path,
                               base::CreateSequencedTaskRunnerWithTraits(
                                  { base::MayBlock()}) .get(),
                               std::unique_ptr<PrefFilter>());
    store_->ReadPrefs();

    if (!GetFlags(&flags_)) {
      LOG(ERROR) << "Unable to locate the \"flags\" preference file: "
                 << path.value();
    }
  } else {
    LOG(ERROR) << "Unable to locate the \"flags\" preference file.";
  }
}

bool VivaldiRuntimeFeatures::GetFlags(FeatureEntryMap* flags) {
  DCHECK(store_.get());

  const base::Value* flags_value;
  if (!store_->GetValue("flags", &flags_value))
    return false;

  const base::DictionaryValue* dict;
  if (!flags_value->GetAsDictionary(&dict))
    return false;

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    const base::Value* root = NULL;
    if (!dict->GetWithoutPathExpansion(it.key(), &root)) {
      LOG(WARNING) << "Invalid entry in \"" << kRuntimeFeaturesFilename
                   << "\" file.";
      continue;
    }
    const base::DictionaryValue* value = NULL;
    if (root->GetAsDictionary(&value)) {
      FeatureEntry* entry = new FeatureEntry;

      for (base::DictionaryValue::Iterator values_it(*value);
           !values_it.IsAtEnd(); values_it.Advance()) {
        const base::Value* sub_value = NULL;
        if (value->GetWithoutPathExpansion(values_it.key(), &sub_value)) {
          if (values_it.key() == "description") {
            sub_value->GetAsString(&entry->description);
          } else if (values_it.key() == "friendly_name") {
            sub_value->GetAsString(&entry->friendly_name);
          } else if (values_it.key() == "value") {
            std::string value;
            if (sub_value->GetAsString(&value)) {
              entry->enabled = (value == "true");
              entry->default_enabled = entry->enabled;
            }
          } else if (values_it.key() == "internal_value") {
            std::string value;
            if (sub_value->GetAsString(&value)) {
              entry->internal_enabled = (value == "true");
              entry->internal_default_enabled = entry->internal_enabled;
            }
          }
        }
      }
      // Check if this has been enabled or disabled on the cmd line.
      std::string cmd_switch = "enable-feature:";
      cmd_switch.append(it.key());

      if (cl->HasSwitch(cmd_switch)) {
        // always enable this feature and force it always on
        entry->internal_enabled = entry->enabled = true;
        entry->force_value = true;
      }
      cmd_switch = "disable-feature:";
      cmd_switch.append(it.key());
      if (cl->HasSwitch(cmd_switch)) {
        // always disable this feature and force it always off
        entry->internal_enabled = entry->enabled = false;
        entry->force_value = true;
      }
      flags->insert(std::make_pair(it.key(), entry));
    }
  }
  const base::ListValue* list =
      profile_->GetPrefs()->GetList(vivaldiprefs::kVivaldiExperiments);

  // Check for any features that might be enabled by the user now.
  for (auto& flag_entry : *flags) {
    base::ListValue::const_iterator it =
        list->Find(base::Value(flag_entry.first));
    if (it != list->end() && flag_entry.second->force_value == false) {
      flag_entry.second->enabled = true;
    }
  }
  return true;
}

FeatureEntry* VivaldiRuntimeFeatures::FindNamedFeature(
    const std::string& feature_name) {
  FeatureEntryMap::iterator it = flags_.find(feature_name);
  if (it != flags_.end()) {
    return (*it).second;
  }
  return nullptr;
}

const FeatureEntryMap& VivaldiRuntimeFeatures::GetAllFeatures() {
  return flags_;
}

VivaldiRuntimeFeatures* VivaldiRuntimeFeaturesFactory::GetForProfile(
    Profile* profile) {
  return static_cast<VivaldiRuntimeFeatures*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

VivaldiRuntimeFeaturesFactory* VivaldiRuntimeFeaturesFactory::GetInstance() {
  return base::Singleton<VivaldiRuntimeFeaturesFactory>::get();
}

VivaldiRuntimeFeaturesFactory::VivaldiRuntimeFeaturesFactory()
    : BrowserContextKeyedServiceFactory(
          "VivaldiRuntimeFeatures",
          BrowserContextDependencyManager::GetInstance()) {}

VivaldiRuntimeFeaturesFactory::~VivaldiRuntimeFeaturesFactory() {}

KeyedService* VivaldiRuntimeFeaturesFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new VivaldiRuntimeFeatures(Profile::FromBrowserContext(profile));
}

bool VivaldiRuntimeFeaturesFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

bool VivaldiRuntimeFeaturesFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* VivaldiRuntimeFeaturesFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Redirected in incognito.
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}

RuntimePrivateExitFunction::RuntimePrivateExitFunction() {}

RuntimePrivateExitFunction::~RuntimePrivateExitFunction() {}

ExtensionFunction::ResponseAction RuntimePrivateExitFunction::Run() {
  // Free any open devtools if the user selects Exit from the menu.
  extensions::DevtoolsConnectorAPI* api =
      extensions::DevtoolsConnectorAPI::GetFactoryInstance()->Get(
          Profile::FromBrowserContext(browser_context()));
  DCHECK(api);
  api->CloseAllDevtools();

  extensions::VivaldiUtilitiesAPI* utils_api =
      extensions::VivaldiUtilitiesAPI::GetFactoryInstance()->Get(
          browser_context());
  DCHECK(utils_api);
  utils_api->CloseAllThumbnailWindows();

  chrome::CloseAllBrowsersAndQuit();

  return RespondNow(NoArguments());
}

RuntimePrivateGetAllFeatureFlagsFunction::
    RuntimePrivateGetAllFeatureFlagsFunction() {}

RuntimePrivateGetAllFeatureFlagsFunction::
    ~RuntimePrivateGetAllFeatureFlagsFunction() {}

bool RuntimePrivateGetAllFeatureFlagsFunction::RunAsync() {
  extensions::VivaldiRuntimeFeatures* features =
      extensions::VivaldiRuntimeFeaturesFactory::GetForProfile(GetProfile());

  const FeatureEntryMap& flags = features->GetAllFeatures();
  std::vector<FeatureFlagInfo> results;

  for (auto& entry : flags) {
    FeatureFlagInfo* flag = new FeatureFlagInfo;
    flag->name = entry.first;
    flag->friendly_name = entry.second->friendly_name;
    flag->description = entry.second->description;
    flag->value = entry.second->enabled ? "true" : "false";
    flag->internal_value = entry.second->internal_enabled ? "true" : "false";
    flag->locked = entry.second->force_value;

    results.push_back(std::move(*flag));
  }
  results_ =
      vivaldi::runtime_private::GetAllFeatureFlags::Results::Create(results);

  SendResponse(true);
  return true;
}

RuntimePrivateSetFeatureEnabledFunction::
    RuntimePrivateSetFeatureEnabledFunction() {}

RuntimePrivateSetFeatureEnabledFunction::
    ~RuntimePrivateSetFeatureEnabledFunction() {}

bool RuntimePrivateSetFeatureEnabledFunction::RunAsync() {
  std::unique_ptr<vivaldi::runtime_private::SetFeatureEnabled::Params> params(
      vivaldi::runtime_private::SetFeatureEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  extensions::VivaldiRuntimeFeatures* features =
      extensions::VivaldiRuntimeFeaturesFactory::GetForProfile(GetProfile());

  bool found = false;
  FeatureEntry* entry = features->FindNamedFeature(params->feature_name);
  bool enable_entry = params->enable;
  DCHECK(entry);
  if (entry && !entry->force_value) {
    // Update the entry in-memory only so the list is always up-to-date.
    entry->internal_enabled = entry->enabled = enable_entry;
    found = true;
    ListPrefUpdate update(GetProfile()->GetPrefs(),
                          vivaldiprefs::kVivaldiExperiments);
    base::ListValue* experiments_list = update.Get();

    experiments_list->Clear();
    const extensions::FeatureEntryMap& flags = features->GetAllFeatures();
    for (auto& flag : flags) {
      // Enable it if it's different than the default
      if (flag.second->enabled &&
          flag.second->default_enabled != flag.second->enabled) {
        experiments_list->AppendString(flag.first);
      }
    }
  }
  results_ =
      vivaldi::runtime_private::SetFeatureEnabled::Results::Create(found);

  SendResponse(true);
  return true;
}

}  // namespace extensions
