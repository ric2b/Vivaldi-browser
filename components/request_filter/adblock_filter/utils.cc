// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/utils.h"

#include <map>
#include <set>

#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"

namespace {
// Increment this whenever an incompatible change is made to
// adblock_rules_list.fbs or to the parser
constexpr int kRulesListFormatVersion = 10;

// Increment this whenever an incompatible change is made to
// adblock_rules_index.fbs
constexpr int kIndexFormatVersion = 6;

enum RulePriorities {
  kModifyPriority = 0,
  kPassPriority,
  kPassAdAttributionPriority,
  kPassAllPriority,
  kModifyImportantPriority,
  kMaxPriority = kModifyImportantPriority
};
}  // namespace

namespace adblock_filter {

std::string GetIndexVersionHeader() {
  return base::StringPrintf("---------Version=%d", kIndexFormatVersion);
}

std::string GetRulesListVersionHeader() {
  return base::StringPrintf("---------Version=%d", kRulesListFormatVersion);
}

std::string CalculateBufferChecksum(base::span<const uint8_t> data) {
  return base::NumberToString(base::PersistentHash(data));
}

int SizePrioritizedStringCompare(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size())
    return lhs.size() > rhs.size() ? -1 : 1;
  return lhs.compare(rhs);
}

std::string_view ToStringPiece(const flatbuffers::String* string) {
  DCHECK(string);
  return std::string_view(string->c_str(), string->size());
}

int GetMaxRulePriority() {
  return kMaxPriority;
}

int GetRulePriority(const flat::RequestFilterRule& rule) {
  switch (rule.decision()) {
    case flat::Decision_MODIFY:
      return kModifyPriority;
    case flat::Decision_PASS:
      if (rule.ad_domains_and_query_triggers()) {
        return kPassAdAttributionPriority;
      }
      if (IsFullModifierPassRule(rule)) {
        return kPassAllPriority;
      }
      return kMaxPriority;
    case flat::Decision_MODIFY_IMPORTANT:
      return kModifyImportantPriority;
  }
}

bool IsFullModifierPassRule(const flat::RequestFilterRule& rule) {
  return rule.decision() == flat::Decision_PASS &&
         rule.modifier() != flat::Modifier_NO_MODIFIER &&
         !rule.modifier_values();
}

bool IsThirdParty(const GURL& url, const url::Origin& origin) {
  return origin.opaque() ||
         !net::registry_controlled_domains::SameDomainOrHost(
             url, origin,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool ContentInjectionRuleBodyCompare::operator()(
    const flat::CosmeticRule* lhs,
    const flat::CosmeticRule* rhs) const {
  return std::lexicographical_compare(
      lhs->selector()->begin(), lhs->selector()->end(),
      rhs->selector()->begin(), rhs->selector()->end());
}

// The goal of this comparator is to provide some sort of order as fast as
// possible to make inserting into a map or set fast. We don't care about
// whether the order makes any logical sense.
bool ContentInjectionRuleBodyCompare::operator()(
    const flat::ScriptletInjectionRule* lhs,
    const flat::ScriptletInjectionRule* rhs) const {
  if (lhs->arguments()->size() < rhs->arguments()->size())
    return true;

  if (lhs->arguments()->size() > rhs->arguments()->size())
    return false;

  auto lhs_argument = lhs->arguments()->begin();
  auto rhs_argument = rhs->arguments()->begin();
  while (lhs_argument != lhs->arguments()->end()) {
    DCHECK(rhs_argument != rhs->arguments()->end());

    // This approach lets us know if two arguments are identical faster than
    // doing two lexicographical_compare

    if (lhs_argument->size() < rhs_argument->size())
      return true;

    if (lhs_argument->size() > rhs_argument->size())
      return false;

    auto mismatch = std::mismatch(lhs_argument->begin(), lhs_argument->end(),
                                  rhs_argument->begin());
    if (mismatch.first != lhs_argument->end())
      return *mismatch.first < *mismatch.second;
    DCHECK(mismatch.second == rhs_argument->end());

    lhs_argument++;
    rhs_argument++;
  }

  // If we get this far, all arguments match.
  // We compare the scriptlet name last, since rules will use only a few
  // different scriptlets, so we are guaranteed to have many matches.
  return std::lexicographical_compare(
      lhs->scriptlet_name()->begin(), lhs->scriptlet_name()->end(),
      rhs->scriptlet_name()->begin(), rhs->scriptlet_name()->end());
}

}  // namespace adblock_filter
