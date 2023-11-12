// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "ios/ad_blocker/utils.h"

#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace adblock_filter {
namespace {
constexpr int kIntermediateRepresentationVersionNumber = 1;
constexpr int kOrganizedRulesVersionNumber = 1;
}  // namespace

int GetIntermediateRepresentationVersionNumber() {
  return kIntermediateRepresentationVersionNumber;
}

int GetOrganizedRulesVersionNumber() {
  return kOrganizedRulesVersionNumber;
}

std::string CalculateBufferChecksum(const std::string& data) {
  return base::NumberToString(base::PersistentHash(data));
}

// The goal of this comparator is to provide some sort of order as fast as
// possible to make inserting into a map or set fast. We don't care about
// whether the order makes any logical sense.
bool ContentInjectionArgumentsCompare::operator()(
    const base::Value::List* lhs,
    const base::Value::List* rhs) const {
  if (lhs->size() < rhs->size())
    return true;

  if (lhs->size() > rhs->size())
    return false;

  auto lhs_argument = lhs->begin();
  auto rhs_argument = rhs->begin();
  while (lhs_argument != lhs->end()) {
    DCHECK(rhs_argument != rhs->end());
    DCHECK(lhs_argument->is_string());
    DCHECK(rhs_argument->is_string());

    // This approach lets us know if two arguments are identical faster than
    // doing two lexicographical_compare

    if (lhs_argument->GetString().size() < rhs_argument->GetString().size())
      return true;

    if (lhs_argument->GetString().size() > rhs_argument->GetString().size())
      return false;

    auto mismatch = std::mismatch(lhs_argument->GetString().begin(),
                                  lhs_argument->GetString().end(),
                                  rhs_argument->GetString().begin());
    if (mismatch.first != lhs_argument->GetString().end())
      return *mismatch.first < *mismatch.second;
    DCHECK(mismatch.second == rhs_argument->GetString().end());

    lhs_argument++;
    rhs_argument++;
  }

  return false;
}
}  // namespace adblock_filter
