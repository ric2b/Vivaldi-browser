// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_PARSE_RESULT_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_PARSE_RESULT_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "components/request_filter/adblock_filter/adblock_cosmetic_rule.h"
#include "components/request_filter/adblock_filter/adblock_filter_rule.h"
#include "components/request_filter/adblock_filter/adblock_metadata.h"

namespace adblock_filter {
struct ParseResult {
 public:
  ParseResult();
  ~ParseResult();
  ParseResult(ParseResult&& parse_result);

  AdBlockMetadata metadata;
  FilterRules filter_rules;
  CosmeticRules cosmetic_rules;
  FetchResult fetch_result = FetchResult::kSuccess;
  RulesInfo rules_info;
  base::Value tracker_infos;

  DISALLOW_COPY_AND_ASSIGN(ParseResult);
};

}  // namespace adblock_filter
#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_PARSE_RESULT_H_
