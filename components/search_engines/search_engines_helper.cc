#include <string>

#include "app/vivaldi_apptools.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_manager.h"
#include "components/search_engines/search_engines_managers_factory.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "search_engines_helper.h"

namespace TemplateURLPrepopulateData {

namespace {
const std::vector<EngineAndTier> GetSearchEngineDetails(
    int country_id,
    std::string application_locale,
    size_t* default_search_provider_index,
    SearchType search_type) {
  if (default_search_provider_index)
    *default_search_provider_index = 0;

  std::vector<EngineAndTier> result;

  ParsedSearchEngines::EnginesListWithDefaults prepopulated_engines_list;

  std::string lang(
      application_locale.begin(),
      std::find(application_locale.begin(), application_locale.end(), '-'));

  prepopulated_engines_list = SearchEnginesManagersFactory::GetInstance()
                                  ->GetSearchEnginesManager()
                                  ->GetEnginesByCountryId(country_id, lang);

  CHECK(!prepopulated_engines_list.list.empty());
  CHECK(prepopulated_engines_list.default_index >= 0 &&
        prepopulated_engines_list.default_index <
            prepopulated_engines_list.list.size());
  CHECK(prepopulated_engines_list.private_default_index >= 0 &&
        prepopulated_engines_list.private_default_index <
            prepopulated_engines_list.list.size());

  // Vivaldi

  for (const auto* prepopulated_engine : prepopulated_engines_list.list) {
    EngineAndTier engine_and_tier;
    engine_and_tier.tier = SearchEngineTier::kTopEngines;
    engine_and_tier.search_engine = prepopulated_engine;
    result.push_back(engine_and_tier);
  }

  CHECK(result.size() == prepopulated_engines_list.list.size());

  if (!default_search_provider_index)
    return result;

  switch (search_type) {
    case SearchType::kMain:
      *default_search_provider_index = prepopulated_engines_list.default_index;
      return result;
    case SearchType::kPrivate:

      *default_search_provider_index =
          prepopulated_engines_list.private_default_index;
      return result;
    case SearchType::kImage:
      *default_search_provider_index =
          prepopulated_engines_list.default_image_search_index.value_or(0);
      return result;
  }
}

}  // namespace

const std::vector<EngineAndTier> GetPrepopulationSetFromCountryID(
    int country_id,
    std::string application_locale) {
  return GetSearchEngineDetails(country_id, application_locale, nullptr,
                                SearchType::kMain);
}

const PrepopulatedEngine* GetFallbackEngine(int country_id,
                                            std::string application_locale,
                                            SearchType search_type) {
  size_t default_engine_index;
  return GetSearchEngineDetails(country_id, application_locale,
                                &default_engine_index, search_type)
      .at(default_engine_index)
      .search_engine;
}

SearchEngineType StringToSearchEngine(const std::string& s) {
  base::flat_map<std::string, SearchEngineType> search_engine_types_map = {
      {"SEARCH_ENGINE_OTHER", SEARCH_ENGINE_OTHER},
      {"SEARCH_ENGINE_AOL", SEARCH_ENGINE_AOL},
      {"SEARCH_ENGINE_ASK", SEARCH_ENGINE_ASK},
      {"SEARCH_ENGINE_ATLAS", SEARCH_ENGINE_ATLAS},
      {"SEARCH_ENGINE_AVG", SEARCH_ENGINE_AVG},
      {"SEARCH_ENGINE_BAIDU", SEARCH_ENGINE_BAIDU},
      {"SEARCH_ENGINE_BABYLON", SEARCH_ENGINE_BABYLON},
      {"SEARCH_ENGINE_BING", SEARCH_ENGINE_BING},
      {"SEARCH_ENGINE_CONDUIT", SEARCH_ENGINE_CONDUIT},
      {"SEARCH_ENGINE_DAUM", SEARCH_ENGINE_DAUM},
      {"SEARCH_ENGINE_DELFI", SEARCH_ENGINE_DELFI},
      {"SEARCH_ENGINE_DELTA", SEARCH_ENGINE_DELTA},
      {"SEARCH_ENGINE_FUNMOODS", SEARCH_ENGINE_FUNMOODS},
      {"SEARCH_ENGINE_GOO", SEARCH_ENGINE_GOO},
      {"SEARCH_ENGINE_GOOGLE", SEARCH_ENGINE_GOOGLE},
      {"SEARCH_ENGINE_IMINENT", SEARCH_ENGINE_IMINENT},
      {"SEARCH_ENGINE_IMESH", SEARCH_ENGINE_IMESH},
      {"SEARCH_ENGINE_IN", SEARCH_ENGINE_IN},
      {"SEARCH_ENGINE_INCREDIBAR", SEARCH_ENGINE_INCREDIBAR},
      {"SEARCH_ENGINE_KVASIR", SEARCH_ENGINE_KVASIR},
      {"SEARCH_ENGINE_LIBERO", SEARCH_ENGINE_LIBERO},
      {"SEARCH_ENGINE_MAILRU", SEARCH_ENGINE_MAILRU},
      {"SEARCH_ENGINE_NAJDI", SEARCH_ENGINE_NAJDI},
      {"SEARCH_ENGINE_NATE", SEARCH_ENGINE_NATE},
      {"SEARCH_ENGINE_NAVER", SEARCH_ENGINE_NAVER},
      {"SEARCH_ENGINE_NETI", SEARCH_ENGINE_NETI},
      {"SEARCH_ENGINE_NIGMA", SEARCH_ENGINE_NIGMA},
      {"SEARCH_ENGINE_OK", SEARCH_ENGINE_OK},
      {"SEARCH_ENGINE_ONET", SEARCH_ENGINE_ONET},
      {"SEARCH_ENGINE_RAMBLER", SEARCH_ENGINE_RAMBLER},
      {"SEARCH_ENGINE_SAPO", SEARCH_ENGINE_SAPO},
      {"SEARCH_ENGINE_SEARCHNU", SEARCH_ENGINE_SEARCHNU},
      {"SEARCH_ENGINE_SEARCH_RESULTS", SEARCH_ENGINE_SEARCH_RESULTS},
      {"SEARCH_ENGINE_SEZNAM", SEARCH_ENGINE_SEZNAM},
      {"SEARCH_ENGINE_SNAPDO", SEARCH_ENGINE_SNAPDO},
      {"SEARCH_ENGINE_SOFTONIC", SEARCH_ENGINE_SOFTONIC},
      {"SEARCH_ENGINE_SOGOU", SEARCH_ENGINE_SOGOU},
      {"SEARCH_ENGINE_SOSO", SEARCH_ENGINE_SOSO},
      {"SEARCH_ENGINE_SWEETPACKS", SEARCH_ENGINE_SWEETPACKS},
      {"SEARCH_ENGINE_TERRA", SEARCH_ENGINE_TERRA},
      {"SEARCH_ENGINE_TUT", SEARCH_ENGINE_TUT},
      {"SEARCH_ENGINE_VINDEN", SEARCH_ENGINE_VINDEN},
      {"SEARCH_ENGINE_VIRGILIO", SEARCH_ENGINE_VIRGILIO},
      {"SEARCH_ENGINE_WALLA", SEARCH_ENGINE_WALLA},
      {"SEARCH_ENGINE_WP", SEARCH_ENGINE_WP},
      {"SEARCH_ENGINE_YAHOO", SEARCH_ENGINE_YAHOO},
      {"SEARCH_ENGINE_YANDEX", SEARCH_ENGINE_YANDEX},
      {"SEARCH_ENGINE_ZOZNAM", SEARCH_ENGINE_ZOZNAM},
      {"SEARCH_ENGINE_360", SEARCH_ENGINE_360},
      {"SEARCH_ENGINE_COCCOC", SEARCH_ENGINE_COCCOC},
      {"SEARCH_ENGINE_DUCKDUCKGO", SEARCH_ENGINE_DUCKDUCKGO},
      {"SEARCH_ENGINE_PARSIJOO", SEARCH_ENGINE_PARSIJOO},
      {"SEARCH_ENGINE_QWANT", SEARCH_ENGINE_QWANT},
      {"SEARCH_ENGINE_GIVERO", SEARCH_ENGINE_GIVERO},
      {"SEARCH_ENGINE_GMX", SEARCH_ENGINE_GMX},
      {"SEARCH_ENGINE_INFO_COM", SEARCH_ENGINE_INFO_COM},
      {"SEARCH_ENGINE_METAGER", SEARCH_ENGINE_METAGER},
      {"SEARCH_ENGINE_OCEANHERO", SEARCH_ENGINE_OCEANHERO},
      {"SEARCH_ENGINE_PRIVACYWALL", SEARCH_ENGINE_PRIVACYWALL},
      {"SEARCH_ENGINE_ECOSIA", SEARCH_ENGINE_ECOSIA},
      {"SEARCH_ENGINE_PETALSEARCH", SEARCH_ENGINE_PETALSEARCH},
      {"SEARCH_ENGINE_STARTER_PACK_BOOKMARKS",
       SEARCH_ENGINE_STARTER_PACK_BOOKMARKS},
      {"SEARCH_ENGINE_STARTER_PACK_HISTORY",
       SEARCH_ENGINE_STARTER_PACK_HISTORY},
      {"SEARCH_ENGINE_STARTER_PACK_TABS", SEARCH_ENGINE_STARTER_PACK_TABS},
      {"SEARCH_ENGINE_MOJEEK", SEARCH_ENGINE_MOJEEK},
      {"SEARCH_ENGINE_PANDASEARCH", SEARCH_ENGINE_PANDASEARCH},
      {"SEARCH_ENGINE_PRESEARCH", SEARCH_ENGINE_PRESEARCH},
      {"SEARCH_ENGINE_YEP", SEARCH_ENGINE_YEP},
      {"SEARCH_ENGINE_NONA", SEARCH_ENGINE_NONA},
      {"SEARCH_ENGINE_QUENDU", SEARCH_ENGINE_QUENDU},
      {"SEARCH_ENGINE_BRAVE", SEARCH_ENGINE_BRAVE},
      {"SEARCH_ENGINE_KARMA", SEARCH_ENGINE_KARMA},
      {"VIVALDI_SEARCH_ENGINE_STARTPAGE_COM",
       VIVALDI_SEARCH_ENGINE_STARTPAGE_COM},
      {"VIVALDI_SEARCH_ENGINE_WIKIPEDIA", VIVALDI_SEARCH_ENGINE_WIKIPEDIA},
      {"VIVALDI_SEARCH_ENGINE_WOLFRAM_ALPHA",
       VIVALDI_SEARCH_ENGINE_WOLFRAM_ALPHA},
      {"VIVALDI_SEARCH_ENGINE_OZON", VIVALDI_SEARCH_ENGINE_OZON},
      {"VIVALDI_SEARCH_ENGINE_AMAZON", VIVALDI_SEARCH_ENGINE_AMAZON},
      {"VIVALDI_SEARCH_ENGINE_EBAY", VIVALDI_SEARCH_ENGINE_EBAY},
      {"VIVALDI_SEARCH_ENGINE_QWANT", VIVALDI_SEARCH_ENGINE_QWANT},
      {"VIVALDI_SEARCH_ENGINE_YELP", VIVALDI_SEARCH_ENGINE_YELP},
      {"VIVALDI_SEARCH_ENGINE_YOU", VIVALDI_SEARCH_ENGINE_YOU},
  };

  auto it = search_engine_types_map.find(s);
  if (it == search_engine_types_map.end())
    return SEARCH_ENGINE_UNKNOWN;
  return it->second;
}

std::optional<RegulatoryExtensionType> StringToRegulatoryExtensionType(
    const std::string& s) {
  base::flat_map<std::string, RegulatoryExtensionType>
      regulatory_extension_types_map = {
          {"RegulatoryExtensionType::kDefault",
           RegulatoryExtensionType::kDefault},
          {"RegulatoryExtensionType::kAndroidEEA",
           RegulatoryExtensionType::kAndroidEEA},
      };

  auto it = regulatory_extension_types_map.find(s);
  if (it == regulatory_extension_types_map.end())
    return std::nullopt;
  return it->second;
}
}  // namespace TemplateURLPrepopulateData
