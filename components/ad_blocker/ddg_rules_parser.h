// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_DDG_RULES_PARSER_H_
#define COMPONENTS_AD_BLOCKER_DDG_RULES_PARSER_H_

#include "base/values.h"
#include "components/ad_blocker/adblock_request_filter_rule.h"
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
                                const base::Value::List* excluded_origins);
  void ParseRule(const base::Value& rule,
                 const std::string& domain,
                 bool default_ignore,
                 const base::Value::List* excluded_origins);
  std::optional<std::bitset<RequestFilterRule::kTypeCount>> GetTypes(
      const base::Value* rule_properties);
  std::optional<std::set<std::string>> GetDomains(
      const base::Value* rule_properties);
  const raw_ptr<ParseResult> parse_result_;
};
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_DDG_RULES_PARSER_H_
