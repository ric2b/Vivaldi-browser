// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_RULESET_FILE_PARSER_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_RULESET_FILE_PARSER_H_

#include <string_view>

#include "components/ad_blocker/adblock_types.h"
#include "components/ad_blocker/adblock_rule_parser.h"

namespace adblock_filter {
struct ParseResult;

class RulesetFileParser {
 public:
  explicit RulesetFileParser(ParseResult* parse_result,
                             RuleSourceSettings source_settings);
  ~RulesetFileParser();
  RulesetFileParser(const RulesetFileParser&) = delete;
  RulesetFileParser& operator=(const RulesetFileParser&) = delete;

  void Parse(std::string_view file_contents);

 private:
  void ParseLine(std::string_view line);

  const raw_ptr<ParseResult> parse_result_;
  RuleParser parser_;
};
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_RULESET_FILE_PARSER_H_
