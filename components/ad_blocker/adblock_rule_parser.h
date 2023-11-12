// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_PARSER_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_PARSER_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
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

  explicit RuleParser(ParseResult* parse_result, bool allow_abp_snippets);
  ~RuleParser();
  RuleParser(const RuleParser&) = delete;
  RuleParser& operator=(const RuleParser&) = delete;

  Result Parse(base::StringPiece rule_string);

 private:
  bool MaybeParseMetadata(base::StringPiece comment);

  Result IsContentInjectionRule(base::StringPiece rule_string,
                                size_t separator,
                                ContentInjectionRuleCore* core);
  bool ParseCosmeticRule(base::StringPiece body,
                         ContentInjectionRuleCore rule_core);
  bool ParseScriptletInjectionRule(base::StringPiece body,
                                   ContentInjectionRuleCore rule_core);
  bool ParseDomains(base::StringPiece domain_string,
                    std::string separator,
                    std::vector<std::string>* included_domains,
                    std::vector<std::string>* excluded_domains);
  Result ParseRequestFilterRule(base::StringPiece rule_string,
                                RequestFilterRule* rule);
  Result ParseRequestFilterRuleOptions(base::StringPiece options,
                                       RequestFilterRule* rule);

  const raw_ptr<ParseResult> parse_result_;
  bool allow_abp_snippets_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_RULE_PARSER_H_
