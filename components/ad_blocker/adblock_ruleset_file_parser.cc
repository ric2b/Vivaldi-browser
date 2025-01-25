// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_ruleset_file_parser.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/ad_blocker/parse_result.h"

namespace adblock_filter {

RulesetFileParser::RulesetFileParser(ParseResult* parse_result,
                                     RuleSourceSettings source_settings)
    : parse_result_(parse_result), parser_(parse_result, source_settings) {
  DCHECK_NE(nullptr, parse_result);
}
RulesetFileParser::~RulesetFileParser() = default;

void RulesetFileParser::Parse(std::string_view file_contents) {
  parse_result_->rules_info = RulesInfo();

  for (auto rule_string :
       base::SplitStringPiece(file_contents, "\r\n", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    ParseLine(rule_string);
  }

  if (parse_result_->request_filter_rules.empty() &&
      parse_result_->cosmetic_rules.empty() &&
      parse_result_->scriptlet_injection_rules.empty()) {
    parse_result_->fetch_result = FetchResult::kFileUnsupported;
  }
}

void RulesetFileParser::ParseLine(std::string_view string_piece) {
  switch (parser_.Parse(string_piece)) {
    case RuleParser::kMetadata:
    case RuleParser::kComment:
      break;
    case RuleParser::kUnsupported:
      parse_result_->rules_info.unsupported_rules++;
      break;
    case RuleParser::kError:
      parse_result_->rules_info.invalid_rules++;
      break;
    case RuleParser::kRequestFilterRule:
    case RuleParser::kCosmeticRule:
    case RuleParser::kScriptletInjectionRule:
      parse_result_->rules_info.valid_rules++;
      break;
  }
}

}  // namespace adblock_filter
