// This header was originally generated from the
// prepopulated_engines_schema.json file. Due to VB-102933, it became necessary
// to move it under the Vivaldi umbrella.
//
// Specifically, the strings could no longer remain as 'const char* const' because
// it would be impossible to initialize them in any way other than by assigning
// hard-coded strings at startup.
//
#ifndef COMPONENTS_SEARCH_ENGINES_PREPOPULATED_ENGINES_H_
#define COMPONENTS_SEARCH_ENGINES_PREPOPULATED_ENGINES_H_

#include <cstddef>
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/regulatory_extension_type.h"

namespace TemplateURLPrepopulateData {

struct RegulatoryExtension {
  //RegulatoryExtension();
  RegulatoryExtensionType variant = RegulatoryExtensionType::kDefault;
  const char* search_params = nullptr;
  const char* suggest_params = nullptr;
};

struct PrepopulatedEngine {
  PrepopulatedEngine();
  const char16_t* name = nullptr;
  const char16_t* keyword = nullptr;
  const char* favicon_url = nullptr;
  const char* search_url = nullptr;
  const char* encoding = nullptr;
  const char* suggest_url = nullptr;
  const char* image_url = nullptr;
  const char* image_translate_url = nullptr;
  const char* new_tab_url = nullptr;
  const char* contextual_search_url = nullptr;
  const char* logo_url = nullptr;
  const char* doodle_url = nullptr;
  const char* search_url_post_params = nullptr;
  const char* suggest_url_post_params = nullptr;
  const char* image_url_post_params = nullptr;
  const char* side_search_param = nullptr;
  const char* side_image_search_param = nullptr;
  const char* image_translate_source_language_param_key = nullptr;
  const char* image_translate_target_language_param_key = nullptr;
  const char16_t* image_search_branding_label = nullptr;
  const char* * search_intent_params = nullptr;
  size_t search_intent_params_size = 0;
  const char* * alternate_urls = nullptr;
  size_t alternate_urls_size = 0;
  SearchEngineType type = SEARCH_ENGINE_UNKNOWN;
  const char* preconnect_to_search_url = nullptr;
  const char* prefetch_likely_navigations = nullptr;
  const RegulatoryExtension* regulatory_extensions = nullptr;
  size_t regulatory_extensions_size = 0;
  int id = 0;
};

extern PrepopulatedEngine google;

}  // namespace TemplateURLPrepopulateData

#endif // COMPONENTS_SEARCH_ENGINES_PREPOPULATED_ENGINES_H_
