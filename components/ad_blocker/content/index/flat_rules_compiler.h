// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_FLAT_RULES_COMPILER_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_FLAT_RULES_COMPILER_H_

#include "base/files/file_path.h"
#include "components/ad_blocker/core/parse_result.h"

namespace adblock_filter {
bool CompileFlatRules(const ParseResult& parse_result,
                      const RuleSourceSettings& source_settings,
                      const base::FilePath& output_path,
                      std::string& checksum);
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_FLAT_RULES_COMPILER_H_
