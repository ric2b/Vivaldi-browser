// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_PROMPT_MANAGER_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_PROMPT_MANAGER_H_

#include <memory>
#include <string_view>
#include <vector>
#include "components/search_engines/search_engine_type.h"

class GURL;
class PrefService;
class ParsedSearchEnginesPrompt;
class TemplateURL;
class TemplateURLService;

namespace country_codes {
class CountryId;
}
namespace adblock_filter {
class RuleService;
}

class SearchEnginesPromptManager {
 public:
  explicit SearchEnginesPromptManager(
      std::unique_ptr<ParsedSearchEnginesPrompt> prompt);
  ~SearchEnginesPromptManager();

  bool IsValid() const;

  // Return true or false whenever should show the search engine prompt.
  bool ShouldPrompt(PrefService* prefs,
                    TemplateURLService* template_url_service,
                    adblock_filter::RuleService* rule_service) const;

  // Returns vector of TemplateURL, that are partner search engine for the
  // profile's locale.
  std::vector<TemplateURL*> GetPartnerSearchEnginesToPrompt(
      country_codes::CountryId country_id,
      const std::string_view application_locale,
      PrefService& prefs,
      TemplateURLService* template_url_service) const;
  void MarkCurrentPromptAsSeen(PrefService* prefs) const;
  void IgnoreCurrentPromptVersion(PrefService* prefs) const;

  int GetCurrentVersion() const;
  std::string GetDialogType() const;
  int GetSearchEnginesDataVersionRequired() const;

 private:
  bool ShouldPromptForTypeOrURL(const SearchEngineType& type,
                                const GURL& url) const;
  bool IsInExcludeList(const SearchEngineType& type, const GURL& url) const;

  bool IsQuarantined(PrefService* prefs) const;

  std::unique_ptr<ParsedSearchEnginesPrompt> prompt_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_PROMPT_MANAGER_H_
