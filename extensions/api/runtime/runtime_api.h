// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_RUNTIME_RUNTIME_API_H_
#define EXTENSIONS_API_RUNTIME_RUNTIME_API_H_

#include <map>
#include <string>

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/json_pref_store.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class VivaldiRuntimeFeatures;

typedef struct FeatureEntry {
  FeatureEntry();

  std::string friendly_name;
  std::string description;
  // in-memory active value
  bool enabled;
  // in-memory active value for internal builds
  bool internal_enabled = false;
  // The value set in the config file
  bool default_enabled;
  // Like default, except for internal (and sopranos builds)
  bool internal_default_enabled = false;
  // true if set by cmd line, value is fixed.
  bool force_value = false;
} * FeatureEntryPtr;

typedef std::map<std::string, FeatureEntryPtr> FeatureEntryMap;

class VivaldiRuntimeFeaturesFactory : public BrowserContextKeyedServiceFactory {
 public:
  static VivaldiRuntimeFeatures* GetForProfile(Profile* profile);

  static VivaldiRuntimeFeaturesFactory* GetInstance();

  static const bool kServiceRedirectedInIncognito = true;

 private:
  friend struct base::DefaultSingletonTraits<VivaldiRuntimeFeaturesFactory>;

  VivaldiRuntimeFeaturesFactory();
  ~VivaldiRuntimeFeaturesFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

class VivaldiRuntimeFeatures : public KeyedService {
 public:
  explicit VivaldiRuntimeFeatures(Profile*);
  VivaldiRuntimeFeatures();
  ~VivaldiRuntimeFeatures() override;

  // Call to check if a named feature is enabled.
  static bool IsEnabled(Profile* profile, const std::string& feature_name);

  FeatureEntry* FindNamedFeature(const std::string& feature_name);
  const FeatureEntryMap& GetAllFeatures();

 private:
  void LoadRuntimeFeatures();
  bool GetFlags(FeatureEntryMap* flags);

  Profile* profile_;
  scoped_refptr<JsonPrefStore> store_;
  FeatureEntryMap flags_;

  base::WeakPtrFactory<VivaldiRuntimeFeatures> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiRuntimeFeatures);
};

class RuntimePrivateExitFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.exit", RUNTIME_EXIT)

  RuntimePrivateExitFunction();

 private:
  ~RuntimePrivateExitFunction() override;
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateExitFunction);
};

class RuntimePrivateGetAllFeatureFlagsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.getAllFeatureFlags",
                             RUNTIME_SETFEATUREENABLED)

  RuntimePrivateGetAllFeatureFlagsFunction();

 private:
  ~RuntimePrivateGetAllFeatureFlagsFunction() override;

  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateGetAllFeatureFlagsFunction);
};

class RuntimePrivateSetFeatureEnabledFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtimePrivate.setFeatureEnabled",
                             RUNTIME_GETALLFEATUREFLAGS)

  RuntimePrivateSetFeatureEnabledFunction();

 private:
  ~RuntimePrivateSetFeatureEnabledFunction() override;

  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(RuntimePrivateSetFeatureEnabledFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_RUNTIME_RUNTIME_API_H_
