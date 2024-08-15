// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_PARSE_RESULT_H_
#define COMPONENTS_AD_BLOCKER_PARSE_RESULT_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "components/ad_blocker/adblock_content_injection_rule.h"
#include "components/ad_blocker/adblock_types.h"
#include "components/ad_blocker/adblock_request_filter_rule.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
#endif  // COMPONENTS_AD_BLOCKER_PARSE_RESULT_H_
