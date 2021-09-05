// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULESET_FILE_PARSER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULESET_FILE_PARSER_H_

#include "base/strings/string_piece.h"
#include "components/request_filter/adblock_filter/adblock_rule_parser.h"

namespace adblock_filter {
struct ParseResult;

class RulesetFileParser {
 public:
  explicit RulesetFileParser(ParseResult* parse_result);
  ~RulesetFileParser();

  void Parse(base::StringPiece file_contents);

 private:
  void ParseLine(base::StringPiece line);

  ParseResult* parse_result_;
  RuleParser parser_;

  DISALLOW_COPY_AND_ASSIGN(RulesetFileParser);
};
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULESET_FILE_PARSER_H_
