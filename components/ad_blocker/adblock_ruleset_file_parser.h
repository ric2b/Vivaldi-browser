// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_RULESET_FILE_PARSER_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_RULESET_FILE_PARSER_H_

#include "base/strings/string_piece.h"
#include "components/ad_blocker/adblock_rule_parser.h"

namespace adblock_filter {
struct ParseResult;

class RulesetFileParser {
 public:
  explicit RulesetFileParser(ParseResult* parse_result,
                             bool allow_abp_snippets);
  ~RulesetFileParser();
  RulesetFileParser(const RulesetFileParser&) = delete;
  RulesetFileParser& operator=(const RulesetFileParser&) = delete;

  void Parse(base::StringPiece file_contents);

 private:
  void ParseLine(base::StringPiece line);

  const raw_ptr<ParseResult> parse_result_;
  RuleParser parser_;
};
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_RULESET_FILE_PARSER_H_
