// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_PROMPT_MANAGER_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_PROMPT_MANAGER_H_

#include <memory>
#include "components/search_engines/search_engine_type.h"

class GURL;
class PrefService;
class ParsedSearchEnginesPrompt;
class TemplateURL;
class TemplateURLService;

namespace adblock_filter {
class RuleService;
}

class SearchEnginesPromptManager {
 public:
  explicit SearchEnginesPromptManager(
      std::unique_ptr<ParsedSearchEnginesPrompt> prompt);
  ~SearchEnginesPromptManager();

  bool IsValid() const;

  // Returns TemplateURL to a default search engine for the profile's
  // locale, or nullptr when the search engine prompt should not be displayed.
  TemplateURL* GetDefaultSearchEngineToPrompt(
      PrefService* prefs,
      TemplateURLService* template_url_service,
      adblock_filter::RuleService* rule_service) const;
  void MarkCurrentPromptAsSeen(PrefService* prefs) const;

  int GetCurrentVersion() const;
  int GetSearchEnginesDataVersionRequired() const;

 private:
  bool ShouldPromptForTypeOrURL(const SearchEngineType& type,
                                const GURL& url) const;

  std::unique_ptr<ParsedSearchEnginesPrompt> prompt_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_PROMPT_MANAGER_H_
