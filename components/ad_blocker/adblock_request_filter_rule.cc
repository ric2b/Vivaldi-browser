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
  constexpr int kAlignemntPosition = 35;
  constexpr int kAlignemntPositionNoColon = kAlignemntPosition - 1;

  auto print_strings = [&os](std::set<std::string> strings) {
    if (strings.empty()) {
      os << ":<NULL>\n";
      return;
    }

    std::string result;
    for (const auto& string : strings) {
      os << ':' << string << "\n" << std::string(kAlignemntPosition, ' ');
    }
    os.seekp(-kAlignemntPosition, std::ios_base::cur);
  };

  os << "\n"
     << std::setw(kAlignemntPosition) << "Rule text:" << rule.original_rule_text << "\n"
     << std::setw(kAlignemntPosition) << "Decision:" << rule.decision << "\n"
     << std::setw(kAlignemntPosition) << "Modify block:" << rule.modify_block
     << "\n"
     << std::setw(kAlignemntPosition) << "Modifier:" << rule.modifier << "\n"
     << std::setw(kAlignemntPositionNoColon) << "Modifier value";

  print_strings(rule.modifier_values);

  os << std::setw(kAlignemntPosition) << PatternTypeToString(rule.pattern_type)
     << rule.pattern << "\n"
     << std::setw(kAlignemntPosition)
     << "NGram search string:" << rule.ngram_search_string.value_or("<NULL>")
     << "\n"
     << std::setw(kAlignemntPosition) << "Anchored:" << rule.anchor_type << "\n"
     << std::setw(kAlignemntPosition) << "Party:" << rule.party << "\n"
     << std::setw(kAlignemntPosition) << "Resources:" << rule.resource_types
     << "\n"
     << std::setw(kAlignemntPosition)
     << "Explicit resources:" << rule.explicit_types << "\n"
     << std::setw(kAlignemntPosition) << "Activations:" << rule.activation_types
     << "\n"
     << std::setw(kAlignemntPosition)
     << "Case sensitive:" << rule.is_case_sensitive << "\n"
     << std::setw(kAlignemntPosition) << "Host:" << rule.host.value_or("<NULL>")
     << "\n"
     << std::setw(kAlignemntPositionNoColon) << "Included domains";
  print_strings(rule.included_domains);
  os << std::setw(kAlignemntPositionNoColon) << "Excluded domains";
  print_strings(rule.excluded_domains);

  os << std::setw(kAlignemntPositionNoColon)
     << "Ad domains and id query params";
  print_strings(rule.ad_domains_and_query_triggers);
  return os;
}

}  // namespace adblock_filter
