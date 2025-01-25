// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_PARSER_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_PARSER_H_

#include <optional>
#include <string>
#include <string_view>
#include <set>

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

  Result IsContentInjectionRule(std::string_view rule_string,
                                size_t separator,
                                ContentInjectionRuleCore* core);
  bool ParseCosmeticRule(std::string_view body,
                         ContentInjectionRuleCore rule_core);
  bool ParseScriptletInjectionRule(std::string_view body,
                                   ContentInjectionRuleCore rule_core);
  bool ParseDomains(std::string_view domain_string,
                    std::string separator,
                    std::set<std::string>& included_domains,
                    std::set<std::string>& excluded_domains);
  Result ParseRequestFilterRule(std::string_view rule_string,
                                RequestFilterRule& rule);
  Result ParseRequestFilterRuleOptions(std::string_view options,
                                       RequestFilterRule& rule);
  std::optional<Result> ParseHostsFileOrNakedHost(std::string_view rule_string);
  bool MaybeAddPureHostRule(std::string_view maybe_hostname);

  bool SetModifier(RequestFilterRule& rule,
                   RequestFilterRule::ModifierType type,
                   std::optional<std::string_view> value);

  bool SetModifier(RequestFilterRule& rule,
                   RequestFilterRule::ModifierType type,
                   std::set<std::string> value);

  const raw_ptr<ParseResult> parse_result_;
  RuleSourceSettings source_settings_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_PARSER_H_
