// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_STYLESHEET_BUILDER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_STYLESHEET_BUILDER_H_

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "components/request_filter/adblock_filter/adblock_metadata.h"

namespace flatbuffers {
struct String;
}

namespace adblock_filter {

const std::string& GetSelector(const std::string& selector_item);

template <typename U>
const std::string& GetSelector(
    const std::pair<const std::string, U>& selector_item) {
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
    stylesheet += GetSelector(selector_item);
  }

  if (selector_count > 0)
    stylesheet += " {display: none !important;}\n";

  return stylesheet;
}

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_STYLESHEET_BUILDER_H_
