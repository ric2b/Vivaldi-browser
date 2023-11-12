// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "ios/ad_blocker/utils.h"

#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"

namespace adblock_filter {
namespace {
constexpr int kIntermediateRepresentationVersionNumber = 1;
constexpr int kOrganizedRulesVersionNumber = 1;
}

int GetIntermediateRepresentationVersionNumber() {
  return kIntermediateRepresentationVersionNumber;
}

int GetOrganizedRulesVersionNumber() {
  return kOrganizedRulesVersionNumber;
}

std::string CalculateBufferChecksum(const std::string& data) {
  return base::NumberToString(base::PersistentHash(data));
}

}  // namespace adblock_filter
