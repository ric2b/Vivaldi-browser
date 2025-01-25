// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_STYLESHEET_BUILDER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_STYLESHEET_BUILDER_H_

#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "components/ad_blocker/adblock_types.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"

namespace flatbuffers {
struct String;
}

namespace adblock_filter {

const flat::CosmeticRule* GetCosmeticRule(
    const flat::CosmeticRule* cosmetic_rule);

template <typename U>
const flat::CosmeticRule* GetCosmeticRule(
    const std::pair<const flat::CosmeticRule* const, U>& selector_item) {
  return selector_item.first;
}

// AdBlock Plus uses this same limit to avoid running into a chromium limitation
// regarding CSS rules maximu length.
const int kMaxSelectorsPerCssRule = 1024;

template <typename T>
std::string BuildStyleSheet(const T& selectors) {
  std::string stylesheet;
  int selector_count = 0;
  for (const auto& selector_item : selectors) {
    if (selector_count >= kMaxSelectorsPerCssRule) {
      selector_count = 0;
      stylesheet += " {display: none !important;}\n";
    } else if (selector_count != 0) {
      stylesheet += ", ";
    }
    selector_count++;
    stylesheet += GetCosmeticRule(selector_item)->selector()->str();
  }

  if (selector_count > 0)
    stylesheet += " {display: none !important;}\n";

  return stylesheet;
}

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_STYLESHEET_BUILDER_H_
