// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_filter_rule.h"

#include <iomanip>

namespace adblock_filter {
namespace {
std::string PatternTypeToString(FilterRule::PatternType pattern_type) {
  switch (pattern_type) {
    case FilterRule::kPlain:
      return "Plain pattern:";
    case FilterRule::kWildcarded:
      return "Wildcarded pattern:";
    case FilterRule::kRegex:
      return "Regex pattern:";
  }
}
}  // namespace

FilterRule::FilterRule() = default;
FilterRule::~FilterRule() = default;
FilterRule::FilterRule(FilterRule&& filter_rule) = default;

bool FilterRule::operator==(const FilterRule& other) const {
  return is_allow_rule == other.is_allow_rule &&
         is_case_sensitive == other.is_case_sensitive &&
         is_csp_rule == other.is_csp_rule &&
         resource_types == other.resource_types &&
         activation_types == other.activation_types && party == other.party &&
         anchor_type == other.anchor_type &&
         pattern_type == other.pattern_type && pattern == other.pattern &&
         ngram_search_string == other.ngram_search_string &&
         host == other.host && redirect == other.redirect && csp == other.csp &&
         excluded_domains == other.excluded_domains &&
         included_domains == other.included_domains;
}

std::ostream& operator<<(std::ostream& os, const FilterRule& rule) {
  os << std::endl
     << std::setw(20) << PatternTypeToString(rule.pattern_type) << rule.pattern
     << std::endl
     << std::setw(20) << "NGram search string:" << rule.ngram_search_string
     << std::endl
     << std::setw(20) << "Anchored:" << rule.anchor_type << std::endl
     << std::setw(20) << "Party:" << rule.party << std::endl
     << std::setw(20) << "Resources:" << rule.resource_types << std::endl
     << std::setw(20) << "Activations:" << rule.activation_types << std::endl
     << std::setw(20) << "Allow rule:" << rule.is_allow_rule << std::endl
     << std::setw(20) << "Case sensitive:" << rule.is_case_sensitive
     << std::endl
     << std::setw(20) << "Host:" << rule.host << std::endl
     << std::setw(20) << "Redirect:" << rule.redirect << std::endl
     << std::setw(20) << "CSP rule:" << rule.is_csp_rule << std::endl
     << std::setw(20) << "CSP:" << rule.csp << std::endl
     << std::setw(20) << "Included domains:";
  for (const auto& included_domain : rule.included_domains)
    os << included_domain << "|";
  os << std::endl << std::setw(20) << "Excluded domains:";
  for (const auto& excluded_domain : rule.excluded_domains)
    os << excluded_domain << "|";
  return os << std::endl;
}

}  // namespace adblock_filter
