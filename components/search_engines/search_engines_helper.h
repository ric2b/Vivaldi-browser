#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_HELPER_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_HELPER_H_

#include <string>
#include <vector>

#include "components/search_engines/regulatory_extension_type.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_prepopulate_data.h"

namespace TemplateURLPrepopulateData {
struct PrepopulatedEngine;
SearchEngineType StringToSearchEngine(const std::string& s);
std::optional<RegulatoryExtensionType> StringToRegulatoryExtensionType(
    const std::string& s);

enum class SearchEngineTier {
  kTopEngines = 1,
  kTyingEngines,
  kRemainingEngines,
};

struct EngineAndTier {
  SearchEngineTier tier;
  const PrepopulatedEngine* search_engine;
};

const std::vector<EngineAndTier> GetPrepopulationSetFromCountryID(
    int country_id,
    std::string application_locale = "");

const PrepopulatedEngine* GetFallbackEngine(
    int country_id,
    std::string application_locale = "",
    SearchType search_type = SearchType::kMain);
}  // namespace TemplateURLPrepopulateData

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_HELPER_H_
