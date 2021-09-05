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
constexpr int kRulesListFormatVersion = 2;

// Increment this whenever an incompatible change is made to
// adblock_rules_index.fbs
constexpr int kIndexFormatVersion = 2;

const base::FilePath::CharType kParsedRulesFolder[] =
    FILE_PATH_LITERAL("AdBlockRules");

const base::FilePath::CharType kTrackingRulesFolder[] =
    FILE_PATH_LITERAL("TrackerBlocking");

const base::FilePath::CharType kAdBlockingRulesFolder[] =
    FILE_PATH_LITERAL("AdBlocking");

enum RulePriorities {
  kBlockPriority = 0,
  kRedirectPriority,
  kAllowPriority,
  kMaxPriority = kAllowPriority
};
}  // namespace

namespace adblock_filter {

std::string GetIndexVersionHeader() {
  return base::StringPrintf("---------Version=%d", kRulesListFormatVersion);
}

std::string GetRulesListVersionHeader() {
  return base::StringPrintf("---------Version=%d", kIndexFormatVersion);
}

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

int GetRulePriority(const flat::FilterRule& rule) {
  if (rule.options() & flat::OptionFlag_IS_ALLOW_RULE)
    return kAllowPriority;

  if (rule.redirect() != flat::RedirectResource_NO_REDIRECT)
    return kRedirectPriority;

  return kBlockPriority;
}

bool IsFullCSPAllowRule(const flat::FilterRule& rule) {
  return (rule.options() & flat::OptionFlag_IS_ALLOW_RULE) != 0 &&
         (rule.options() & flat::OptionFlag_IS_CSP_RULE) != 0 && !rule.csp();
}

bool IsThirdParty(const GURL& url, const url::Origin& origin) {
  return origin.opaque() ||
         !net::registry_controlled_domains::SameDomainOrHost(
             url, origin,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace adblock_filter
