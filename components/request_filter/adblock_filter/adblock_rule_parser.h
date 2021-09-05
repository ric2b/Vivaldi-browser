// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_PARSER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_PARSER_H_

#include <vector>

#include "base/strings/string_piece.h"
#include "components/request_filter/adblock_filter/parse_result.h"

namespace adblock_filter {

class RuleParser {
 public:
  enum Result {
    kFilterRule = 0,
    kCosmeticRule,
    kComment,
    kMetadata,
    kUnsupported,
    kError
  };

  explicit RuleParser(ParseResult* parse_result);
  ~RuleParser();

  Result Parse(base::StringPiece rule_string);

 private:
  bool MaybeParseMetadata(base::StringPiece comment);

  Result MaybeParseCosmeticRule(base::StringPiece rule_string,
                                size_t separator,
                                CosmeticRule* rule);
  bool ParseDomains(base::StringPiece domain_string,
                    std::string separator,
                    std::vector<std::string>* included_domains,
                    std::vector<std::string>* excluded_domains);
  Result ParseFilterRule(base::StringPiece rule_string, FilterRule* rule);
  Result ParseFilterRuleOptions(base::StringPiece options, FilterRule* rule);

  ParseResult* parse_result_;

  DISALLOW_COPY_AND_ASSIGN(RuleParser);
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_PARSER_H_
