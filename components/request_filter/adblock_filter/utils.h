// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_UTILS_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_UTILS_H_

#include <compare>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"

namespace url {
class Origin;
}

class GURL;

namespace adblock_filter {
using FlatStringOffset = flatbuffers::Offset<flatbuffers::String>;
using PartyMatcher = base::RepeatingCallback<bool(flat::Party party)>;

std::string GetIndexVersionHeader();
std::string GetRulesListVersionHeader();

std::string CalculateBufferChecksum(base::span<const uint8_t> data);

int SizePrioritizedStringCompare(std::string_view lhs, std::string_view rhs);
std::string_view ToStringPiece(const flatbuffers::String* string);
int GetRulePriority(const flat::RequestFilterRule& rule);
int GetMaxRulePriority();
bool IsFullModifierPassRule(const flat::RequestFilterRule& rule);
PartyMatcher GetPartyMatcher(const GURL& url, const url::Origin& origin);

std::weak_ordering FastCompareFlatString(const flatbuffers::String* lhs,
                                         const flatbuffers::String* rhs);

std::weak_ordering FastCompareFlatStringVector(
    const flatbuffers::Vector<FlatStringOffset>* lhs,
    const flatbuffers::Vector<FlatStringOffset>* rhs);

// These comparators only look at the rules body. This allows to avoid a string
// copy of the body from the rule when building maps/sets keyed on those bodies.
// However, maps/sets built using those comparators must be reasoned about
// carefully because a rule match means only the body matches and the core
// might be different.
struct ContentInjectionRuleBodyCompare {
  bool operator()(const flat::CosmeticRule* lhs,
                  const flat::CosmeticRule* rhs) const;
  bool operator()(const flat::ScriptletInjectionRule* lhs,
                  const flat::ScriptletInjectionRule* rhs) const;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_UTILS_H_
