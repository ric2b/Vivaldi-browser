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
// adblock_rules_list.fbs
constexpr int kRulesListFormatVersion = 5;

// Increment this whenever an incompatible change is made to
// adblock_rules_index.fbs
constexpr int kIndexFormatVersion = 4;

enum RulePriorities {
  kBlockPriority = 0,
  kRedirectPriority,
  kAllowPriority,
  kMaxPriority = kAllowPriority
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
  return base::NumberToString(base::PersistentHash(data.data(), data.size()));
}

int CompareDomains(base::StringPiece lhs_domain, base::StringPiece rhs_domain) {
  if (lhs_domain.size() != rhs_domain.size())
    return lhs_domain.size() > rhs_domain.size() ? -1 : 1;
  return lhs_domain.compare(rhs_domain);
}

base::StringPiece ToStringPiece(const flatbuffers::String* string) {
  DCHECK(string);
  return base::StringPiece(string->c_str(), string->size());
}

int GetMaxRulePriority() {
  return kMaxPriority;
}

int GetRulePriority(const flat::RequestFilterRule& rule) {
  if (rule.options() & flat::OptionFlag_IS_ALLOW_RULE)
    return kAllowPriority;

  if (rule.redirect() && rule.redirect()->size())
    return kRedirectPriority;

  return kBlockPriority;
}

bool IsFullCSPAllowRule(const flat::RequestFilterRule& rule) {
  return (rule.options() & flat::OptionFlag_IS_ALLOW_RULE) != 0 &&
         (rule.options() & flat::OptionFlag_IS_CSP_RULE) != 0 && !rule.csp();
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
