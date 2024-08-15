// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_UTILS_H_
#define COMPONENTS_AD_BLOCKER_UTILS_H_

#include "base/files/file_path.h"

#include "components/ad_blocker/adblock_types.h"

namespace adblock_filter {
base::FilePath::StringType GetRulesFolderName();
base::FilePath::StringType GetGroupFolderName(RuleGroup group);
}

#endif  // COMPONENTS_AD_BLOCKER_UTILS_H_
