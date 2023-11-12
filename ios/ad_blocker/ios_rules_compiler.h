// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_AD_BLOCKER_IOS_RULES_COMPILER_H_
#define IOS_AD_BLOCKER_IOS_RULES_COMPILER_H_

#include <set>

#include "base/files/file_path.h"
#include "components/ad_blocker/parse_result.h"

namespace adblock_filter {
// Used in tests
std::string CompileIosRulesToString(const ParseResult& parse_result,
                                    bool pretty_print);

bool CompileIosRules(const ParseResult& parse_result,
                     const base::FilePath& output_path,
                     std::string& checksum);

base::Value CompileExceptionsRule(const std::set<std::string>& exceptions,
                                  bool process_list);
}  // namespace adblock_filter

#endif  // IOS_AD_BLOCKER_IOS_RULES_COMPILER_H_
