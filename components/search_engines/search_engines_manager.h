// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGER_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"

#include "components/search_engines/parsed_search_engines.h"
#include "components/search_engines/prepopulated_engines.h"

class SearchEnginesManager {
 public:
  explicit SearchEnginesManager(
      std::unique_ptr<ParsedSearchEngines> search_engines);
  ~SearchEnginesManager();

  using PrepopulatedEngine = TemplateURLPrepopulateData::PrepopulatedEngine;

  ParsedSearchEngines::EnginesListWithDefaults GetEnginesByCountryId(
      int country_id,
      const std::string& lang) const;

/*  const std::vector<const PrepopulatedEngine*>& GetAllEngines() const;*/
  const PrepopulatedEngine* GetEngine(const std::string& name) const;

  // Google is necessary for some chromium code to work.
//  const PrepopulatedEngine* GetGoogleEngine() const;
  // This returns our main default engine. It will never return a nullptr.
  const PrepopulatedEngine* GetMainDefaultEngine() const;

  int GetCurrentDataVersion() const;

  int GetMaxPrepopulatedEngineID() const;

 private:
  std::unique_ptr<ParsedSearchEngines> search_engines_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_MANAGER_H_
