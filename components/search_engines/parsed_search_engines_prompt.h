// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_PARSED_SEARCH_ENGINES_PROMPT_H_
#define COMPONENTS_SEARCH_ENGINES_PARSED_SEARCH_ENGINES_PROMPT_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "components/search_engines/search_engine_type.h"

class ParsedSearchEnginesPrompt {
 public:
  static std::unique_ptr<ParsedSearchEnginesPrompt> FromJsonString(
      std::string_view json_string,
      std::string& error);

  ~ParsedSearchEnginesPrompt();
  ParsedSearchEnginesPrompt(const ParsedSearchEnginesPrompt&) = delete;
  ParsedSearchEnginesPrompt& operator=(const ParsedSearchEnginesPrompt&) =
      delete;

  std::vector<std::string> prompt_if_domain() const {
    return prompt_if_domain_;
  }
  std::set<SearchEngineType> prompt_if_type() const { return prompt_if_type_; }
  int version() const { return version_; }
  int search_engines_data_version_required() const {
    return search_engines_data_version_required_;
  }

 private:
  ParsedSearchEnginesPrompt(std::vector<std::string> prompt_if_domain,
                            std::set<SearchEngineType> prompt_if_type,
                            int version,
                            int search_engines_data_version_required);

  const std::vector<std::string> prompt_if_domain_;
  const std::set<SearchEngineType> prompt_if_type_;
  const int version_;
  const int search_engines_data_version_required_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_PARSED_SEARCH_ENGINES_PROMPT_H_
