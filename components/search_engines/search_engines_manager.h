// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGER_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGER_H_

#include <memory>
#include <string>

#include "components/search_engines/parsed_search_engines.h"

class PrefService;

class SearchEnginesManager {
 public:
  explicit SearchEnginesManager(
      std::unique_ptr<ParsedSearchEngines> search_engines);
  ~SearchEnginesManager();

  using PrepopulatedEngine = TemplateURLPrepopulateData::PrepopulatedEngine;

  ParsedSearchEngines::EnginesListWithDefaults GetEnginesByCountryId(
      country_codes::CountryId country_id,
      const std::string& application_locale,
      PrefService& prefs) const;

  const PrepopulatedEngine* GetEngine(const std::string& name) const;

  // This returns our main default engine. It will never return a nullptr.
  const PrepopulatedEngine* GetMainDefaultEngine(PrefService* prefs = nullptr) const;

  int GetCurrentDataVersion() const;

  int GetMaxPrepopulatedEngineID() const;

 private:
  std::unique_ptr<ParsedSearchEngines> search_engines_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGER_H_
