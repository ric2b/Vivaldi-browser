// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"

#include "base/notreached.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide::model_execution::prefs {

const char kTabOrganizationEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.tab_organization_enterprise_policy_"
    "allowed";

const char kComposeEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.compose_enterprise_policy_allowed";

const char kWallpaperSearchEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.wallpaper_search_enterprise_policy_"
    "allowed";

const char kHistorySearchEnterprisePolicyAllowed[] =
    "optimization_guide.model_execution.history_search_"
    "enterprise_policy_allowed";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kTabOrganizationEnterprisePolicyAllowed,
      static_cast<int>(ModelExecutionEnterprisePolicyValue::kAllow),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(
      kComposeEnterprisePolicyAllowed,
      static_cast<int>(ModelExecutionEnterprisePolicyValue::kAllow),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(
      kWallpaperSearchEnterprisePolicyAllowed,
      static_cast<int>(ModelExecutionEnterprisePolicyValue::kAllow),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(
      kHistorySearchEnterprisePolicyAllowed,
      static_cast<int>(ModelExecutionEnterprisePolicyValue::kAllow),
      PrefRegistry::LOSSY_PREF);
}

const char* GetEnterprisePolicyPrefName(UserVisibleFeatureKey feature) {
  switch (feature) {
    case UserVisibleFeatureKey::kCompose:
      return kComposeEnterprisePolicyAllowed;
    case UserVisibleFeatureKey::kTabOrganization:
      return kTabOrganizationEnterprisePolicyAllowed;
    case UserVisibleFeatureKey::kWallpaperSearch:
      return kWallpaperSearchEnterprisePolicyAllowed;
    case UserVisibleFeatureKey::kHistorySearch:
      return kHistorySearchEnterprisePolicyAllowed;
  }
}

namespace localstate {

// Preference of the last version checked. Used to determine when the
// disconnect count is reset.
const char kOnDeviceModelChromeVersion[] =
    "optimization_guide.on_device.last_version";

// Preference where number of disconnects (crashes) of on device model is
// stored.
const char kOnDeviceModelCrashCount[] =
    "optimization_guide.on_device.model_crash_count";

// Preference where number of timeouts of on device model is stored.
const char kOnDeviceModelTimeoutCount[] =
    "optimization_guide.on_device.timeout_count";

const char kOnDeviceModelValidationResult[] =
    "optimization_guide.on_device.model_validation_result";

// Stores the last computed `OnDeviceModelPerformanceClass` of the device.
const char kOnDevicePerformanceClass[] =
    "optimization_guide.on_device.performance_class";

// A timestamp for the last time various features were used which could have
// benefited from the on-device model. These are on-device eligible features,
// and this will be used to help decide whether to acquire the on device base
// model and the adaptation model.
//
// For historical reasons, the compose pref was named generically and is
// continued to be used.

const char kLastTimeComposeWasUsed[] =
    "optimization_guide.last_time_on_device_eligible_feature_used";

const char kLastTimePromptApiWasUsed[] =
    "optimization_guide.model_execution.last_time_prompt_api_"
    "used";

const char kLastTimeTestFeatureWasUsed[] =
    "optimization_guide.model_execution.last_time_test_used";

const char kLastTimeHistorySearchWasUsed[] =
    "optimization_guide.model_execution.last_time_history_search_used";

// A timestamp for the last time the on-device model was eligible for download.
const char kLastTimeEligibleForOnDeviceModelDownload[] =
    "optimization_guide.on_device.last_time_eligible_for_download";

// An integer pref that contains the user's client id.
const char kModelQualityLogggingClientId[] =
    "optimization_guide.model_quality_logging_client_id";

// An integer pref for the on-device GenAI foundational model enterprise policy
// settings.
const char kGenAILocalFoundationalModelEnterprisePolicySettings[] =
    "optimization_guide.gen_ai_local_foundational_model_settings";

}  // namespace localstate

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(localstate::kOnDeviceModelChromeVersion,
                               std::string());
  registry->RegisterIntegerPref(localstate::kOnDeviceModelCrashCount, 0);
  registry->RegisterIntegerPref(localstate::kOnDeviceModelTimeoutCount, 0);
  registry->RegisterIntegerPref(localstate::kOnDevicePerformanceClass, 0);
  registry->RegisterTimePref(localstate::kLastTimeComposeWasUsed,
                             base::Time::Min());
  registry->RegisterTimePref(localstate::kLastTimePromptApiWasUsed,
                             base::Time::Min());
  registry->RegisterTimePref(localstate::kLastTimeTestFeatureWasUsed,
                             base::Time::Min());
  registry->RegisterTimePref(localstate::kLastTimeHistorySearchWasUsed,
                             base::Time::Min());
  registry->RegisterTimePref(
      localstate::kLastTimeEligibleForOnDeviceModelDownload, base::Time::Min());
  registry->RegisterDictionaryPref(localstate::kOnDeviceModelValidationResult);
  registry->RegisterInt64Pref(localstate::kModelQualityLogggingClientId, 0,
                              PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(
      localstate::kGenAILocalFoundationalModelEnterprisePolicySettings, 0);
}

// LINT.IfChange(GetOnDeviceFeatureRecentlyUsedPref)
const char* GetOnDeviceFeatureRecentlyUsedPref(
    ModelBasedCapabilityKey feature) {
  switch (feature) {
    case ModelBasedCapabilityKey::kCompose:
      return prefs::localstate::kLastTimeComposeWasUsed;
    case ModelBasedCapabilityKey::kPromptApi:
      return prefs::localstate::kLastTimePromptApiWasUsed;
    case ModelBasedCapabilityKey::kTest:
      return prefs::localstate::kLastTimeTestFeatureWasUsed;
    case ModelBasedCapabilityKey::kHistorySearch:
      return prefs::localstate::kLastTimeHistorySearchWasUsed;
    case ModelBasedCapabilityKey::kWallpaperSearch:
    case ModelBasedCapabilityKey::kTabOrganization:
    case ModelBasedCapabilityKey::kTextSafety:
      // This should not be called for features that are not on-device.
      NOTREACHED_NORETURN();
  }
}
// LINT.ThenChange(IsOnDeviceModelEnabled)

}  // namespace optimization_guide::model_execution::prefs
