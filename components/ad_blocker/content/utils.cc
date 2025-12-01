// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/utils.h"

#include <map>
#include <set>

#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/ad_blocker/public/content/adblock_rule_service.h"
#include "components/ad_blocker/public/core/adblock_rule_manager.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "vivaldi/components/ad_blocker/content/flat/adblock_rules_list_generated.h"

namespace {
// Increment this whenever an incompatible change is made to
// adblock_rules_list.fbs or to the parser
constexpr int kRulesListFormatVersion = 15;

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

bool CanFilterUrl(const GURL& url) {
  return (url.SchemeIs(url::kFtpScheme) || url.SchemeIsHTTPOrHTTPS() ||
          url.SchemeIsWSOrWSS());
}

bool IsOriginWanted(RuleService* service, RuleGroup group, url::Origin origin) {
  return !service->GetRuleManager()->IsExemptOfFiltering(group, origin);
}

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

PartyMatcher GetPartyMatcher(const GURL& url, const url::Origin& origin) {
  enum Party {
    kFirst,
    kThird,
  };

  Party party =
      origin.opaque() ||
              !net::registry_controlled_domains::SameDomainOrHost(
                  url, origin,
                  net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)
          ? Party::kThird
          : Party::kFirst;

  Party strict_party =
      origin.IsSameOriginWith(url) ? Party::kFirst : Party::kThird;

  return base::BindRepeating(
      [](Party party, Party strict_party, flat::Party rule_party) {
        switch (rule_party) {
          case flat::Party_ALL:
            return true;
          case flat::Party_FIRST:
            return party == Party::kFirst;
          case flat::Party_THIRD:
            return party == Party::kThird;
          case flat::Party_STRICT_FIRST:
            return strict_party == Party::kFirst;
          case flat::Party_STRICT_THIRD:
            return strict_party == Party::kThird;
          case flat::Party_FIRST_AND_STRICT_THIRD:
            return party == Party::kFirst && strict_party == Party::kThird;
        }
      },
      party, strict_party);
}

bool ContentInjectionRuleBodyCompare::operator()(
    const flat::CosmeticRule* lhs,
    const flat::CosmeticRule* rhs) const {
  CHECK(lhs);
  CHECK(rhs);
  return FastCompareFlatString(lhs->selector(), rhs->selector()) < 0;
}

// The goal of this comparator is to provide some sort of order as fast as
// possible to make inserting into a map or set fast. We don't care about
// whether the order makes any logical sense.
bool ContentInjectionRuleBodyCompare::operator()(
    const flat::ScriptletInjectionRule* lhs,
    const flat::ScriptletInjectionRule* rhs) const {
  CHECK(lhs);
  CHECK(rhs);
  auto args_ordering =
      FastCompareFlatStringVector(lhs->arguments(), rhs->arguments());

  if (args_ordering < 0) {
    return true;
  }

  if (args_ordering > 0) {
    return false;
  }

  // If we get this far, all arguments match.
  // We compare the scriptlet name last, since rules will use only a few
  // different scriptlets, so we are guaranteed to have many matches.
  return FastCompareFlatString(lhs->scriptlet_name(), rhs->scriptlet_name()) <
         0;
}

std::weak_ordering FastCompareFlatString(const flatbuffers::String* lhs,
                                         const flatbuffers::String* rhs) {
  if (!rhs && !lhs) {
    return std::weak_ordering::equivalent;
  }

  if (!rhs) {
    return std::weak_ordering::less;
  }

  if (!lhs) {
    return std::weak_ordering::greater;
  }

  // Check sizes first to avoid a full comparison. We don't care if the ordering
  // makes sense as much as it being fast.
  if (lhs->size() < rhs->size()) {
    return std::weak_ordering::less;
  }

  if (lhs->size() > rhs->size()) {
    return std::weak_ordering::greater;
  }

  return std::lexicographical_compare_three_way(lhs->begin(), lhs->end(),
                                                rhs->begin(), rhs->end());
}

std::weak_ordering FastCompareFlatStringVector(
    const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>* lhs,
    const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>* rhs) {
  if (!rhs && !lhs) {
    return std::weak_ordering::equivalent;
  }

  if (!rhs) {
    return std::weak_ordering::less;
  }

  if (!lhs) {
    return std::weak_ordering::greater;
  }

  // Check sizes first to avoid a full comparison. We don't care if the ordering
  // makes sense as much as it being fast.
  if (lhs->size() < rhs->size()) {
    return std::weak_ordering::less;
  }

  if (lhs->size() > rhs->size()) {
    return std::weak_ordering::greater;
  }

  return std::lexicographical_compare_three_way(lhs->begin(), lhs->end(),
                                                rhs->begin(), rhs->end(),
                                                FastCompareFlatString);
}

std::optional<RequestFilterRule::ResourceType> ToCoreResourceType(
    flat::ResourceType resource_type) {
  switch (resource_type) {
    case flat::ResourceType_STYLESHEET:
      return RequestFilterRule::kStylesheet;
    case flat::ResourceType_IMAGE:
      return RequestFilterRule::kImage;
    case flat::ResourceType_OBJECT:
      return RequestFilterRule::kObject;
    case flat::ResourceType_SCRIPT:
      return RequestFilterRule::kScript;
    case flat::ResourceType_XMLHTTPREQUEST:
      return RequestFilterRule::kXmlHttpRequest;
    case flat::ResourceType_SUBDOCUMENT:
      return RequestFilterRule::kSubDocument;
    case flat::ResourceType_FONT:
      return RequestFilterRule::kFont;
    case flat::ResourceType_MEDIA:
      return RequestFilterRule::kMedia;
    case flat::ResourceType_WEBSOCKET:
      return RequestFilterRule::kWebSocket;
    case flat::ResourceType_WEBRTC:
      return RequestFilterRule::kWebRTC;
    case flat::ResourceType_PING:
      return RequestFilterRule::kPing;
    case flat::ResourceType_WEBTRANSPORT:
      return RequestFilterRule::kWebTransport;
    case flat::ResourceType_WEBBUNDLE:
      return RequestFilterRule::kWebBundle;
    case flat::ResourceType_OTHER:
      return RequestFilterRule::kOther;
    case flat::ResourceType_DOCUMENT:
    case flat::ResourceType_POPUP:
    case flat::ResourceType_POPUNDER:
    case flat::ResourceType_NONE:
    case flat::ResourceType_ANY:
      return std::nullopt;
  }
}

}  // namespace adblock_filter
