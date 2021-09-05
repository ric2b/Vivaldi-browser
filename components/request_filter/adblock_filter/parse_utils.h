// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_PARSE_UTILS_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_PARSE_UTILS_H_

#include <map>

#include "base/strings/string_piece.h"

namespace adblock_filter {
extern const std::map<base::StringPiece, int> kTypeStringMap;
std::string BuildNgramSearchString(const base::StringPiece& pattern);
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_PARSE_UTILS_H_
