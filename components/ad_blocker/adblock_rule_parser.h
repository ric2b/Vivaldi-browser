// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_PARSER_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_PARSER_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "components/ad_blocker/adblock_types.h"
#include "components/ad_blocker/parse_result.h"

namespace adblock_filter {

class RuleParser {
 public:
  enum Result {
    kRequestFilterRule = 0,
    kCosmeticRule,
    kScriptletInjectionRule,
    kComment,
    kMetadata,
    kUnsupported,
    kError
  };

  explicit RuleParser(ParseResult* parse_result,
                      RuleSourceSettings source_settings);
  ~RuleParser();
  RuleParser(const RuleParser&) = delete;
  RuleParser& operator=(const RuleParser&) = delete;

  Result Parse(std::string_view rule_string);

 private:
  bool MaybeParseMetadata(std::string_view comment);

  std::optional<Result> ParseContentInjectionRule(std::string_view rule_string,
                                                  size_t first_sseparator);
  bool ParseCosmeticRule(std::string_view body,
                         ContentInjectionRuleCore rule_core);
  bool ParseScriptletInjectionRule(std::string_view body,
                                   ContentInjectionRuleCore rule_core);
  Result ParseRequestFilterRule(std::string_view rule_string,
                                RequestFilterRule& rule);
  Result ParseRequestFilterRuleOptions(std::string_view options,
                                       RequestFilterRule& rule,
                                       bool& can_strict_block);
  std::optional<Result> ParseHostsFileOrNakedHost(std::string_view rule_string);
  bool MaybeAddPureHostRule(std::string_view maybe_hostname,
                            std::string_view original_rule_text);

  const raw_ptr<ParseResult> parse_result_;
  RuleSourceSettings source_settings_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_PARSER_H_
