// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
#include "components/search_engines/search_engines_manager.h"

#include "components/search_engines/search_engine_type.h"

#include "components/search_engines/parsed_search_engines.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_helper.h"

namespace {
using namespace TemplateURLPrepopulateData;
}

namespace TemplateURLPrepopulateData{
// The built-in version of the google search engine is used throughout chromium,
// mainly for its type. We define a dummy version here which will serve only for
// that purpose and only that purpose.
const PrepopulatedEngine google = {{
  .name = u"Google",
  .keyword = nullptr,
  .favicon_url = nullptr,
  .search_url = nullptr,
  .encoding = nullptr,
  .suggest_url = nullptr,
  .image_url = nullptr,
  .image_translate_url = nullptr,
  .new_tab_url = nullptr,
  .contextual_search_url = nullptr,
  .logo_url = nullptr,
  .doodle_url = nullptr,
  .search_url_post_params = nullptr,
  .suggest_url_post_params = nullptr,
  .image_url_post_params = nullptr,
  .side_search_param = nullptr,
  .side_image_search_param = nullptr,
  .image_translate_source_language_param_key = nullptr,
  .image_translate_target_language_param_key = nullptr,
  .image_search_branding_label = nullptr,
  .search_intent_params = {},
  .alternate_urls = {},
  .type = SEARCH_ENGINE_GOOGLE,
  .preconnect_to_search_url = nullptr,
  .prefetch_likely_navigations = nullptr,
  .id = 1,
  .regulatory_extensions = {},
}};

base::span<const PrepopulatedEngine* const> kAllEngines;
}  // namespace

SearchEnginesManager::SearchEnginesManager(
    std::unique_ptr<ParsedSearchEngines> search_engines)
    : search_engines_(std::move(search_engines)) {
  CHECK(search_engines_);
  TemplateURLPrepopulateData::kAllEngines = search_engines_->all_engines();
}

SearchEnginesManager::~SearchEnginesManager() = default;

ParsedSearchEngines::EnginesListWithDefaults
SearchEnginesManager::GetEnginesByCountryId(int country_id,
                                            const std::string& language) const {
  const auto& engines_for_locale = search_engines_->engines_for_locale();
  const auto& default_country_for_language =
      search_engines_->default_country_for_language();

  if (!engines_for_locale.contains(country_id)) {
    // We were unable to find the country_id in `locales_for_country`
    // but we still have the language. We can try to choose the country by the
    // language.
    auto language_and_country_id = default_country_for_language.find(language);
    if (language_and_country_id != default_country_for_language.end()) {
      // We found the language, now we update the country id.
      country_id = language_and_country_id->second;
    }
  }

  if (!engines_for_locale.contains(country_id)) {
    // No option left, return the default set of the search engines.
    return search_engines_->default_engines_list();
  }

  auto language_and_engines = engines_for_locale.at(country_id);

  // Enfored at parsing time.
  CHECK(!language_and_engines.empty());

  // Some countries have more than one language.
  // Example: Norway => ["nb", "NO", "nb_NO"] and ["nn", "NO", "nn_NO"]
  for (auto& [language_code, engines] : language_and_engines) {
    if (language_code == language) {
      return engines;
    }
  }
  // Take the first.
  return language_and_engines.front().second;
}

const PrepopulatedEngine* SearchEnginesManager::GetEngine(
    const std::string& name) const {
  auto search_engine = search_engines_->engines_map().find(name);
  if (search_engine == search_engines_->engines_map().end()) {
    return nullptr;
  }
  return search_engine->second;
}

/*const std::vector<const PrepopulatedEngine*>&
SearchEnginesManager::GetAllEngines() const {
  CHECK(IsInitialized());
  return search_engines_->all_engines();
}*/

/*const PrepopulatedEngine* SearchEnginesManager::GetGoogleEngine() const {
  const PrepopulatedEngine* result = GetEngine(kGoogle);
  CHECK(result);
  return result;
}*/

const PrepopulatedEngine* SearchEnginesManager::GetMainDefaultEngine() const {
  const ParsedSearchEngines::EnginesListWithDefaults& default_list =
      search_engines_->default_engines_list();
  return default_list.list.at(default_list.default_index);
}

int SearchEnginesManager::GetCurrentDataVersion() const {
  return search_engines_->current_data_version();
}

int SearchEnginesManager::GetMaxPrepopulatedEngineID() const {
  return search_engines_->max_prepopulated_engine_id();
}
