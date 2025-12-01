// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
#include "components/search_engines/search_engines_manager.h"

#include "base/version.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/parsed_search_engines.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_helper.h"
#include "components/version_utils/vivaldi_version_utils.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace {
using namespace TemplateURLPrepopulateData;

ParsedSearchEngines::EnginesListWithDefaults GetVersionedEngineForProfile(
    PrefService& prefs,
    const ParsedSearchEngines::VersionedEngines& engines) {
  CHECK(engines.contains("default"));
  ParsedSearchEngines::EnginesListWithDefaults default_engine =
      engines.at("default");

  const base::Version first_seen_version =
      prefs.HasPrefPath(vivaldiprefs::kStartupFirstSeenVersion)
          ? base::Version(
                prefs.GetString(vivaldiprefs::kStartupFirstSeenVersion))
          : base::Version();
  if (!first_seen_version.IsValid()) {
    return default_engine;
  }

  for (const auto& [key, engine] : engines) {
    const base::Version engine_version = base::Version(key);

    if (engine_version.IsValid() &&
        // Use the same or lower search engine version than the first version
        // seen.
        engine_version.CompareTo(first_seen_version) <= 0) {
      default_engine = engine;
    }
  }
  return default_engine;
}

std::string LanguageCodeFromApplicationLocale(
    const std::string_view application_locale) {
  std::string lang(
      application_locale.begin(),
      std::find(application_locale.begin(), application_locale.end(), '-'));
  return lang;
}

}  // namespace

namespace TemplateURLPrepopulateData {

base::span<const PrepopulatedEngine* const> kAllEngines;
}  // namespace TemplateURLPrepopulateData

SearchEnginesManager::SearchEnginesManager(
    std::unique_ptr<ParsedSearchEngines> search_engines)
    : search_engines_(std::move(search_engines)) {
  CHECK(search_engines_);
  TemplateURLPrepopulateData::kAllEngines = search_engines_->all_engines();
}

SearchEnginesManager::~SearchEnginesManager() = default;

ParsedSearchEngines::EnginesListWithDefaults
SearchEnginesManager::GetEnginesByCountryId(
    country_codes::CountryId country_id,
    const std::string& application_locale,
    PrefService& prefs) const {
  const auto& engines_for_locale = search_engines_->engines_for_locale();
  const auto& default_country_for_language =
      search_engines_->default_country_for_language();
  const std::string language =
      LanguageCodeFromApplicationLocale(application_locale);

  if (!engines_for_locale.contains(country_id.Serialize())) {
    // We were unable to find the country_id in `locales_for_country`
    // but we still have the language. We can try to choose the country by the
    // language.
    auto language_and_country_id = default_country_for_language.find(language);
    if (language_and_country_id != default_country_for_language.end()) {
      // We found the language, now we update the country id.
      country_id = language_and_country_id->second;
    }
  }

  if (!engines_for_locale.contains(country_id.Serialize())) {
    // No option left, return the default set of the search engines.
    return GetVersionedEngineForProfile(
        prefs, search_engines_->default_engines_list());
  }

  const auto language_and_engines =
      engines_for_locale.at(country_id.Serialize());

  // Enfored at parsing time.
  CHECK(!language_and_engines.empty());

  // Some countries have more than one language.
  // Example: Norway => ["nb", "NO", "nb_NO"] and ["nn", "NO", "nn_NO"]
  for (auto& [language_code, versioned_engines] : language_and_engines) {
    if (language_code == language) {
      return GetVersionedEngineForProfile(prefs, versioned_engines);
    }
  }
  // Take the first.
  return GetVersionedEngineForProfile(prefs,
                                      language_and_engines.begin()->second);
}

const PrepopulatedEngine* SearchEnginesManager::GetEngine(
    const std::string& name) const {
  auto search_engine = search_engines_->engines_map().find(name);
  if (search_engine == search_engines_->engines_map().end()) {
    return nullptr;
  }
  return search_engine->second;
}

const PrepopulatedEngine* SearchEnginesManager::GetMainDefaultEngine(
    PrefService* prefs) const {
  ParsedSearchEngines::EnginesListWithDefaults default_engines =
      search_engines_->default_engines_list().at("default");

  if (prefs) {
    default_engines = GetVersionedEngineForProfile(
        *prefs, search_engines_->default_engines_list());
  }
  return default_engines.list.at(default_engines.default_index);
}

int SearchEnginesManager::GetCurrentDataVersion() const {
  return search_engines_->current_data_version();
}

int SearchEnginesManager::GetMaxPrepopulatedEngineID() const {
  return search_engines_->max_prepopulated_engine_id();
}
