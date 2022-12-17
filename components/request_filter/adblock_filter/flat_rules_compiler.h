// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_FLAT_RULES_COMPILER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_FLAT_RULES_COMPILER_H_

#include "base/files/file_path.h"
#include "components/ad_blocker/parse_result.h"

namespace adblock_filter {
bool CompileFlatRules(const ParseResult& parse_result,
                      const base::FilePath& output_path,
                      std::string& checksum);
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_FLAT_RULES_COMPILER_H_
