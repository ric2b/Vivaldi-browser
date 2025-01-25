// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_PARSED_SEARCH_ENGINES_H_
#define COMPONENTS_SEARCH_ENGINES_PARSED_SEARCH_ENGINES_H_

#include <map>
#include <memory>
#include <vector>

#include "base/values.h"
#include "components/search_engines/prepopulated_engines.h"

class ParsedSearchEngines {
 public:
  using PrepopulateEnginesList =
      std::vector<const TemplateURLPrepopulateData::PrepopulatedEngine*>;

  struct EnginesListWithDefaults {
    PrepopulateEnginesList list;
    size_t default_index;
    size_t private_default_index;
    std::optional<size_t> default_image_search_index;
  };

  using EnginesMap =
      std::map<std::string, TemplateURLPrepopulateData::PrepopulatedEngine*>;
  using EnginesForLocale =
      std::map<int,
               std::vector<std::pair<std::string, EnginesListWithDefaults>>>;

  static std::unique_ptr<ParsedSearchEngines> FromJsonString(
      std::string_view json_string,
      std::string& error);

  ~ParsedSearchEngines();
  ParsedSearchEngines(const ParsedSearchEngines&) = delete;
  ParsedSearchEngines& operator=(const ParsedSearchEngines&) = delete;

  const PrepopulateEnginesList& all_engines() const { return all_engines_ptr_; }

  const EnginesMap& engines_map() const { return engines_map_; }

  const EnginesForLocale& engines_for_locale() const {
    return engines_for_locale_;
  }

  const EnginesListWithDefaults& default_engines_list() const {
    return default_engines_list_;
  }

  const std::unordered_map<std::string, int>& default_country_for_language()
      const {
    return default_country_for_language_;
  }

  int max_prepopulated_engine_id() const { return max_prepopulated_engine_id_; }
  int current_data_version() const { return current_data_version_; }

 private:
  class PrepopulatedEngineStorage;
  ParsedSearchEngines(
      std::vector<PrepopulatedEngineStorage> storage,
      std::vector<
          std::unique_ptr<TemplateURLPrepopulateData::PrepopulatedEngine>>
          all_engines,
      EnginesListWithDefaults default_engines_list,
      EnginesMap engines_map,
      EnginesForLocale engines_for_locale,
      std::unordered_map<std::string, int> default_locale_for_language,
      int max_prepopulated_engine_id,
      int current_data_version);
  const std::vector<PrepopulatedEngineStorage> storage_;

  const std::vector<
      std::unique_ptr<TemplateURLPrepopulateData::PrepopulatedEngine>>
      all_engines_;

  const PrepopulateEnginesList all_engines_ptr_;

  const EnginesListWithDefaults default_engines_list_;

  // Search engine entry name -> PrepopulatedEngine*
  const EnginesMap engines_map_;

  // Country -> [Language -> Vector of PrepopulatedEngine*]
  const EnginesForLocale engines_for_locale_;

  // Language -> Country
  const std::unordered_map<std::string, int> default_country_for_language_;

  const int max_prepopulated_engine_id_;
  const int current_data_version_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_PARSED_SEARCH_ENGINES_H_