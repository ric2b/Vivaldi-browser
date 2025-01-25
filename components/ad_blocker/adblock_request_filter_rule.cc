// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_request_filter_rule.h"

#include <iomanip>

#include "base/strings/string_util.h"

namespace adblock_filter {
namespace {
std::string PatternTypeToString(RequestFilterRule::PatternType pattern_type) {
  switch (pattern_type) {
    case RequestFilterRule::kPlain:
      return "Plain pattern:";
    case RequestFilterRule::kWildcarded:
      return "Wildcarded pattern:";
    case RequestFilterRule::kRegex:
      return "Regex pattern:";
  }
}
}  // namespace

RequestFilterRule::RequestFilterRule() = default;
RequestFilterRule::~RequestFilterRule() = default;
RequestFilterRule::RequestFilterRule(RequestFilterRule&& request_filter_rule) =
    default;
RequestFilterRule& RequestFilterRule::operator=(
    RequestFilterRule&& request_filter_rule) = default;

bool RequestFilterRule::operator==(const RequestFilterRule& other) const {
  return decision == other.decision && modify_block == other.modify_block &&
         modifier == other.modifier && modifier_value == other.modifier_value &&
         activation_types == other.activation_types &&
         is_case_sensitive == other.is_case_sensitive &&
         resource_types == other.resource_types && party == other.party &&
         anchor_type == other.anchor_type &&
         pattern_type == other.pattern_type && pattern == other.pattern &&
         ngram_search_string == other.ngram_search_string &&
         host == other.host && excluded_domains == other.excluded_domains &&
         included_domains == other.included_domains;
}

std::ostream& operator<<(std::ostream& os, const RequestFilterRule& rule) {
  return os << std::endl
            << std::setw(20) << "Decision:" << rule.decision << std::endl
            << std::setw(20) << "Modify block:" << rule.modify_block
            << std::endl
            << std::setw(20) << "Modifier:" << rule.modifier << std::endl
            << std::setw(20)
            << "Modifier value:" << rule.modifier_value.value_or("<NULL>")
            << std::endl
            << std::setw(20) << PatternTypeToString(rule.pattern_type)
            << rule.pattern << std::endl
            << std::setw(20) << "NGram search string:"
            << rule.ngram_search_string.value_or("<NULL>") << std::endl
            << std::setw(20) << "Anchored:" << rule.anchor_type << std::endl
            << std::setw(20) << "Party:" << rule.party << std::endl
            << std::setw(20) << "Resources:" << rule.resource_types << std::endl
            << std::setw(20) << "Activations:" << rule.activation_types
            << std::endl
            << std::setw(20) << "Case sensitive:" << rule.is_case_sensitive
            << std::endl
            << std::setw(20) << "Host:" << rule.host.value_or("<NULL>")
            << std::endl
            << std::setw(20) << "Included domains:"
            << base::JoinString(rule.included_domains, "|") << std::endl
            << std::setw(20) << "Excluded domains:"
            << base::JoinString(rule.excluded_domains, "|") << std::endl;
}

}  // namespace adblock_filter
