// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/utils.h"

namespace {
const base::FilePath::CharType kParsedRulesFolder[] =
    FILE_PATH_LITERAL("AdBlockRules");

const base::FilePath::CharType kTrackingRulesFolder[] =
    FILE_PATH_LITERAL("TrackerBlocking");

const base::FilePath::CharType kAdBlockingRulesFolder[] =
    FILE_PATH_LITERAL("AdBlocking");
}  // namespace

namespace adblock_filter {
base::FilePath::StringType GetRulesFolderName() {
  return kParsedRulesFolder;
}

base::FilePath::StringType GetGroupFolderName(RuleGroup group) {
  switch (group) {
    case RuleGroup::kTrackingRules:
      return kTrackingRulesFolder;
    case RuleGroup::kAdBlockingRules:
      return kAdBlockingRulesFolder;
  }
}
}  // namespace adblock_filter