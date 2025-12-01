// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CORE_PARSE_RESULT_H_
#define COMPONENTS_AD_BLOCKER_CORE_PARSE_RESULT_H_

#include "base/values.h"
#include "components/ad_blocker/core/adblock_content_injection_rule.h"
#include "components/ad_blocker/core/adblock_request_filter_rule.h"
#include "components/ad_blocker/public/core/adblock_types.h"

namespace adblock_filter {
struct ParseResult {
 public:
  ParseResult();
  ~ParseResult();
  ParseResult(ParseResult&& parse_result);

  AdBlockMetadata metadata;
  RequestFilterRules request_filter_rules;
  CosmeticRules cosmetic_rules;
  ScriptletInjectionRules scriptlet_injection_rules;
  FetchResult fetch_result = FetchResult::kSuccess;
  RulesInfo rules_info;
  std::optional<base::Value::Dict> tracker_infos;
};

}  // namespace adblock_filter
#endif  // COMPONENTS_AD_BLOCKER_CORE_PARSE_RESULT_H_
