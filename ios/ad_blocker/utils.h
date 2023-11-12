// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_AD_BLOCKER_UTILS_H_
#define IOS_AD_BLOCKER_UTILS_H_

#include <string>

namespace adblock_filter {
int GetIntermediateRepresentationVersionNumber();

std::string CalculateBufferChecksum(const std::string& data);
}  // namespace adblock_filter

#endif  // IOS_AD_BLOCKER_UTILS_H_
