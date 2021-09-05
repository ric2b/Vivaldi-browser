// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_cosmetic_rule.h"

#include <iomanip>

namespace adblock_filter {

CosmeticRule::CosmeticRule() = default;
CosmeticRule::~CosmeticRule() = default;
CosmeticRule::CosmeticRule(CosmeticRule&& filter_rule) = default;

bool CosmeticRule::operator==(const CosmeticRule& other) const {
  return is_allow_rule == other.is_allow_rule &&
         excluded_domains == other.excluded_domains &&
         included_domains == other.included_domains &&
         selector == other.selector;
}

std::ostream& operator<<(std::ostream& os, const CosmeticRule& rule) {
  os << std::endl
     << std::setw(20) << rule.selector << std::endl
     << std::setw(20) << "Allow rule:" << rule.is_allow_rule << std::endl
     << std::setw(20) << "Included domains:";
  for (const auto& included_domain : rule.included_domains)
    os << included_domain << "|";
  os << std::endl << std::setw(20) << "Excluded domains:";
  for (const auto& excluded_domain : rule.excluded_domains)
    os << excluded_domain << "|";
  return os << std::endl;
}

}  // namespace adblock_filter
