// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_prepopulate_data.h"

#include <algorithm>
#include <random>
#include <variant>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/country_codes/country_codes.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/eea_countries_ids.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/version_info/version_info.h"
//#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
//#include "third_party/search_engines_data/resources/definitions/regional_settings.h"

#include "app/vivaldi_apptools.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_helper.h"
#include "components/search_engines/search_engines_manager.h"
#include "components/search_engines/search_engines_managers_factory.h"
#include "components/search_engines/vivaldi_pref_names.h"
namespace TemplateURLPrepopulateData {

// Helpers --------------------------------------------------------------------

namespace {
// The number of search engines for each country falling into the "top"
// category.
// constexpr size_t kTopSearchEnginesThreshold = 5;


#if 0 // Vivaldi
#include "components/search_engines/search_engine_countries-inc.cc"
#endif // #if 0 // Vivaldi

// Vivaldi
std::string GetLangFromPrefs(PrefService& prefs) {
  if (!prefs.FindPreference(prefs::kLanguageAtInstall))
    return "";

  // Expecting that the first run language value was set before reaching this,
  // sine there isn't a practical way to pass it to the search code otherwise.
  DCHECK(prefs.HasPrefPath(prefs::kLanguageAtInstall) ||
         !vivaldi::IsVivaldiRunning());
  return prefs.GetString(prefs::kLanguageAtInstall);
}
// End Vivaldi

std::vector<std::unique_ptr<TemplateURLData>>
GetPrepopulatedEnginesForEeaRegionCountries(CountryId country_id,
                                            PrefService& prefs) {
  CHECK(regional_capabilities::IsEeaCountry(country_id));

  uint64_t profile_seed = prefs.GetInt64(
      prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed);
  int seed_version_number = prefs.GetInteger(
      prefs::kDefaultSearchProviderChoiceScreenShuffleMilestone);
  int current_version_number = version_info::GetMajorVersionNumberAsInt();
  // Ensure that the generated seed is not 0 to avoid accidental re-seeding
  // and re-shuffle on every chrome update.
  while (profile_seed == 0 || current_version_number != seed_version_number) {
    profile_seed = base::RandUint64();
    prefs.SetInt64(prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed,
                   profile_seed);
    prefs.SetInteger(prefs::kDefaultSearchProviderChoiceScreenShuffleMilestone,
                     current_version_number);
    seed_version_number = current_version_number;
  }

  std::vector<std::unique_ptr<TemplateURLData>> t_urls; // =
      // base::ToVector(GetRegionalSettings(country_id).search_engines,
      //               &PrepopulatedEngineToTemplateURLData);

  std::default_random_engine generator;
  generator.seed(profile_seed);
  std::shuffle(t_urls.begin(), t_urls.end(), generator);

  CHECK_LE(t_urls.size(), kMaxEeaPrepopulatedEngines);
  return t_urls;
}

std::vector<std::unique_ptr<TemplateURLData>> GetPrepopulatedTemplateURLData(
    CountryId country_id,
    PrefService& prefs,
    const std::string application_locale
  ) {
  if (regional_capabilities::HasSearchEngineCountryListOverride() && !vivaldi::IsVivaldiRunning()) {
    auto country_override =
        std::get<regional_capabilities::SearchEngineCountryListOverride>(
            regional_capabilities::GetSearchEngineCountryOverride().value());

    switch (country_override) {
      case regional_capabilities::SearchEngineCountryListOverride::kEeaAll:
        return GetAllEeaRegionPrepopulatedEngines();
      case regional_capabilities::SearchEngineCountryListOverride::kEeaDefault:
        return GetDefaultPrepopulatedEngines();
    }
  }

  if (regional_capabilities::IsEeaCountry(country_id) && !vivaldi::IsVivaldiRunning()) {
    return GetPrepopulatedEnginesForEeaRegionCountries(country_id, prefs);
  }

  std::vector<std::unique_ptr<TemplateURLData>> t_urls;
  std::vector<EngineAndTier> engines =
      GetPrepopulationSetFromCountryID(country_id, application_locale, prefs);
  for (const EngineAndTier& engine : engines) {
    if (engine.tier == SearchEngineTier::kTopEngines) {
      t_urls.push_back(
          TemplateURLDataFromPrepopulatedEngine(*engine.search_engine));
    }
  }
  return t_urls;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SearchProviderOverrideStatus {
  // No preferences are available for `prefs::kSearchProviderOverrides`.
  kNoPref = 0,

  // The preferences for `prefs::kSearchProviderOverrides` do not contain valid
  // template URLs.
  kEmptyPref = 1,

  // The preferences for `prefs::kSearchProviderOverrides` contain valid
  // template URL(s).
  kPrefHasValidUrls = 2,

  kMaxValue = kPrefHasValidUrls
};

std::vector<std::unique_ptr<TemplateURLData>> GetOverriddenTemplateURLData(
    PrefService& prefs) {
  std::vector<std::unique_ptr<TemplateURLData>> t_urls;

  const base::Value::List& list =
      prefs.GetList(prefs::kSearchProviderOverrides);

  for (const base::Value& engine : list) {
    if (engine.is_dict()) {
      auto t_url = TemplateURLDataFromOverrideDictionary(engine.GetDict());
      if (t_url) {
        t_urls.push_back(std::move(t_url));
      }
    }
  }

  base::UmaHistogramEnumeration(
      "Search.SearchProviderOverrideStatus",
      !t_urls.empty() ? SearchProviderOverrideStatus::kPrefHasValidUrls
                      : (prefs.HasPrefPath(prefs::kSearchProviderOverrides)
                             ? SearchProviderOverrideStatus::kEmptyPref
                             : SearchProviderOverrideStatus::kNoPref));

  return t_urls;
}

std::unique_ptr<TemplateURLData> FindPrepopulatedEngineInternal(
    PrefService& prefs,
    CountryId country_id,
    int prepopulated_id,
    bool use_first_as_fallback) {
  // This could be more efficient. We load all URLs but keep only one.
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_engines =
      GetPrepopulatedEngines(prefs, country_id);
  if (prepopulated_engines.empty()) {
    // Not expected to be a real possibility, branch to be removed when this is
    // verified.
    if (vivaldi::IsVivaldiRunning())
    NOTREACHED(base::NotFatalUntil::M132);
    return nullptr;
  }

  for (auto& engine : prepopulated_engines) {
    if (engine->prepopulate_id == prepopulated_id) {
      return std::move(engine);
    }
  }

  if (use_first_as_fallback) {
    return std::move(prepopulated_engines[0]);
  }

  return nullptr;
}

}  // namespace

// Global functions -----------------------------------------------------------

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  country_codes::RegisterProfilePrefs(registry);
  registry->RegisterListPref(prefs::kSearchProviderOverrides);
  registry->RegisterIntegerPref(prefs::kSearchProviderOverridesVersion, -1);
  registry->RegisterInt64Pref(
      prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed, 0);
  registry->RegisterIntegerPref(
      prefs::kDefaultSearchProviderChoiceScreenShuffleMilestone, 0);
}

int GetDataVersion(PrefService* prefs) {
  // Allow tests to override the local version.
  return (prefs && prefs->HasPrefPath(prefs::kSearchProviderOverridesVersion)) ?
      prefs->GetInteger(prefs::kSearchProviderOverridesVersion) :
        SearchEnginesManagersFactory::GetInstance()
                   ->GetSearchEnginesManager()
                   ->GetCurrentDataVersion();
}

std::vector<std::unique_ptr<TemplateURLData>> GetPrepopulatedEngines(
    PrefService& prefs,
    CountryId country_id) {
  // If there is a set of search engines in the preferences file, it overrides
  // the built-in set.
  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      GetOverriddenTemplateURLData(prefs);
  if (!t_urls.empty()) {
    return t_urls;
  }

  return GetPrepopulatedTemplateURLData(country_id, prefs, GetLangFromPrefs(prefs));
}

std::unique_ptr<TemplateURLData> GetPrepopulatedEngine(PrefService& prefs,
                                                       CountryId country_id,
                                                       int prepopulated_id) {
  return FindPrepopulatedEngineInternal(prefs, country_id, prepopulated_id,
                                        /*use_first_as_fallback=*/false);
}

#if BUILDFLAG(IS_ANDROID)

std::vector<std::unique_ptr<TemplateURLData>> GetLocalPrepopulatedEngines(
    const std::string& country_code,
    PrefService& prefs,
    std::string application_locale) {
  CountryId country_id(country_code);
  if (!country_id.IsValid()) {
    LOG(ERROR) << "Unknown country code specified: " << country_code;
    return std::vector<std::unique_ptr<TemplateURLData>>();
  }

  return GetPrepopulatedTemplateURLData(country_id, prefs, application_locale);
}

#endif

std::unique_ptr<TemplateURLData> GetPrepopulatedEngineFromFullList(
    PrefService& prefs,
    CountryId country_id,
    int prepopulated_id) {
  // TODO(crbug.com/40940777): Refactor to better share code with
  // `GetPrepopulatedEngine()`.

  // If there is a set of search engines in the preferences file, we look for
  // the ID there first.
  for (std::unique_ptr<TemplateURLData>& data :
       GetOverriddenTemplateURLData(prefs)) {
    if (data->prepopulate_id == prepopulated_id) {
      return std::move(data);
    }
  }

  auto engine_matcher = [&](const PrepopulatedEngine* engine) {
    return engine->id == prepopulated_id;
  };

  // We look in the profile country's prepopulated set first. This is intended
  // to help using the right entry for the case where we have multiple ones in
  // the full list that share a same prepopulated id.
  for (const EngineAndTier& engine_and_tier :
    GetPrepopulationSetFromCountryID(country_id, GetLangFromPrefs(prefs), prefs)) {
    if (engine_and_tier.search_engine->id == prepopulated_id) {
      return TemplateURLDataFromPrepopulatedEngine(
          *engine_and_tier.search_engine);
    }
  }

  // Fallback: just grab the first matching entry from the complete list. In
  // case of IDs shared across multiple entries, we might be returning the
  // wrong one for the profile country. We can look into better heuristics in
  // future work.
  for (const PrepopulatedEngine* engine : kAllEngines) {
    if (engine->id == prepopulated_id) {
      return TemplateURLDataFromPrepopulatedEngine(*engine);
    }
  }

  // Fallback: just grab the first matching entry from the complete list.
  // In case of IDs shared across multiple entries, we might be returning
  // the wrong one for the profile country. We can look into better
  // heuristics in future work.
  if (auto iter = std::ranges::find_if(kAllEngines, engine_matcher);
      iter != kAllEngines.end()) {
    return {};  // PrepopulatedEngineToTemplateURLData(*iter);
  }

  return {};
}

void ClearPrepopulatedEnginesInPrefs(PrefService* prefs) {
  if (!prefs)
    return;

  prefs->ClearPref(prefs::kSearchProviderOverrides);
  prefs->ClearPref(prefs::kSearchProviderOverridesVersion);
}

std::unique_ptr<TemplateURLData> GetPrepopulatedFallbackSearch(
    PrefService& prefs,
    CountryId country_id, SearchType search_type) {
  if (vivaldi::IsVivaldiRunning()) {
    return TemplateURLDataFromPrepopulatedEngine(*GetFallbackEngine(
        country_id, GetLangFromPrefs(prefs), prefs, search_type));
  }
  return FindPrepopulatedEngineInternal(prefs, country_id, google.id,
                                        /*use_first_as_fallback=*/true);
}

const base::span<const PrepopulatedEngine* const> GetAllPrepopulatedEngines() {
  return kAllEngines;
}

std::vector<std::unique_ptr<TemplateURLData>>
GetAllEeaRegionPrepopulatedEngines() {
  std::vector<std::unique_ptr<TemplateURLData>> result;

  // We use a `flat_set` to filter out engines that have the same prepopulated
  // id. For example, `yahoo_fr` and `yahoo_de` have the same prepopulated id
  // because they point to the same search engine so we only want to record one
  // instance.
  /*
  base::flat_set<int> used_engines;
  for (CountryId eea_country_id :
       regional_capabilities::kEeaChoiceCountriesIds) {
    const auto& search_engines =
        GetRegionalSettings(eea_country_id).search_engines;

    for (const auto* engine : search_engines) {
      if (auto [_, added] = used_engines.emplace(engine->id); added) {
        result.push_back(PrepopulatedEngineToTemplateURLData(engine));
      }
    }
  }
  */

  return result;
}

std::vector<std::unique_ptr<TemplateURLData>> GetDefaultPrepopulatedEngines() {
  /* return base::ToVector(GetRegionalSettings(CountryId()).search_engines,
                        &PrepopulatedEngineToTemplateURLData);*/
  return {};

}

}  // namespace TemplateURLPrepopulateData
