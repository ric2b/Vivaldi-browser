// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"

namespace adblock_filter {

bool ActivationResult::IsDecision(RuleDecision match_decision) const {
  return rule_stub && rule_stub->decision == match_decision;
}

bool ActivationResults::IsDocumentDecision(RuleDecision decision) const {
  return document_exception
             ? decision == RuleDecision::kPass
             : by_type[ActivationType::kWholeDocument].IsDecision(decision);
}
}  // namespace adblock_filter
