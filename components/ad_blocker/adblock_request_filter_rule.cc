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

bool RequestFilterRule::operator==(const RequestFilterRule& other) const =
    default;

std::ostream& operator<<(std::ostream& os, const RequestFilterRule& rule) {
  auto print_strings = [&os](std::set<std::string> strings) {
    if (strings.empty()) {
      os << ":<NULL>" << std::endl;
      return;
    }

    std::string result;
    for (const auto& string : strings) {
      os << ':' << string << std::endl << std::string(20, ' ');
    }
    os.seekp(-20, std::ios_base::cur);
  };

  os << std::endl
     << std::setw(20) << "Decision:" << rule.decision << std::endl
     << std::setw(20) << "Modify block:" << rule.modify_block << std::endl
     << std::setw(20) << "Modifier:" << rule.modifier << std::endl
     << std::setw(19) << "Modifier value";

  print_strings(rule.modifier_values);

  os << std::setw(20) << PatternTypeToString(rule.pattern_type) << rule.pattern
     << std::endl
     << std::setw(20)
     << "NGram search string:" << rule.ngram_search_string.value_or("<NULL>")
     << std::endl
     << std::setw(20) << "Anchored:" << rule.anchor_type << std::endl
     << std::setw(20) << "Party:" << rule.party << std::endl
     << std::setw(20) << "Resources:" << rule.resource_types << std::endl
     << std::setw(20) << "Explicit resources:" << rule.explicit_types
     << std::endl
     << std::setw(20) << "Activations:" << rule.activation_types << std::endl
     << std::setw(20) << "Case sensitive:" << rule.is_case_sensitive
     << std::endl
     << std::setw(20) << "Host:" << rule.host.value_or("<NULL>") << std::endl
     << std::setw(19) << "Included domains";
  print_strings(rule.included_domains);
  os << std::setw(19) << "Excluded domains";
  print_strings(rule.excluded_domains);

  os << std::setw(20) << "Ad domains and id query params:" << std::endl;
  print_strings(rule.ad_domains_and_query_triggers);
  return os;
}

}  // namespace adblock_filter
