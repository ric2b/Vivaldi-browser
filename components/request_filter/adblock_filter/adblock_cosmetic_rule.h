// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_COSMETIC_RULE_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_COSMETIC_RULE_H_

#include <string>
#include <vector>

#include "base/macros.h"

namespace adblock_filter {

struct CosmeticRule {
 public:
  CosmeticRule();
  ~CosmeticRule();
  CosmeticRule(CosmeticRule&& filter_rule);
  bool operator==(const CosmeticRule& other) const;

  bool is_allow_rule = false;

  std::vector<std::string> included_domains;
  std::vector<std::string> excluded_domains;

  std::string selector;

  DISALLOW_COPY_AND_ASSIGN(CosmeticRule);
};

using CosmeticRules = std::vector<CosmeticRule>;

// Used for unit tests.
std::ostream& operator<<(std::ostream& os, const CosmeticRule& rule);
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_COSMETIC_RULE_H_
