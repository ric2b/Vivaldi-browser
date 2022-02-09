// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_DDG_RULES_PARSER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_DDG_RULES_PARSER_H_

#include "base/values.h"
#include "components/request_filter/adblock_filter/adblock_request_filter_rule.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace adblock_filter {
struct ParseResult;

class DuckDuckGoRulesParser {
 public:
  explicit DuckDuckGoRulesParser(ParseResult* parse_result);
  ~DuckDuckGoRulesParser();
  DuckDuckGoRulesParser(const DuckDuckGoRulesParser&) = delete;
  DuckDuckGoRulesParser& operator=(const DuckDuckGoRulesParser&) = delete;

  void Parse(const base::Value& root);

 private:
  void AddBlockingRuleForDomain(const std::string& domain,
                                const base::Value* excluded_origins);
  void ParseRule(const base::Value& rule,
                 const std::string& domain,
                 bool default_ignore,
                 const base::Value* excluded_origins);
  absl::optional<std::bitset<RequestFilterRule::kTypeCount>> GetTypes(
      const base::Value* rule_properties);
  absl::optional<std::vector<std::string>> GetDomains(
      const base::Value* rule_properties);
  ParseResult* parse_result_;
};
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_DDG_RULES_PARSER_H_
