// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_INDEX_INDEX_UTILS_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_INDEX_INDEX_UTILS_H_

#include <compare>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"
#include "vivaldi/components/ad_blocker/content/index/flat/adblock_rules_list_generated.h"

class GURL;

namespace adblock_filter {
class RuleService;

using FlatStringOffset = flatbuffers::Offset<flatbuffers::String>;

std::string GetIndexVersionHeader();
std::string GetRulesListVersionHeader();

std::string CalculateBufferChecksum(base::span<const uint8_t> data);

int SizePrioritizedStringCompare(std::string_view lhs, std::string_view rhs);
std::string_view ToStringPiece(const flatbuffers::String* string);
int GetRulePriority(const flat::RequestFilterRule& rule);
int GetMaxRulePriority();
bool IsFullModifierPassRule(const flat::RequestFilterRule& rule);

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

constexpr flat::ResourceType ToFlatResourceType(ResourceType resource_type) {
  switch (resource_type) {
    case ResourceType::kStylesheet:
      return flat::ResourceType_STYLESHEET;
    case ResourceType::kImage:
      return flat::ResourceType_IMAGE;
    case ResourceType::kObject:
      return flat::ResourceType_OBJECT;
    case ResourceType::kScript:
      return flat::ResourceType_SCRIPT;
    case ResourceType::kXmlHttpRequest:
      return flat::ResourceType_XMLHTTPREQUEST;
    case ResourceType::kSubDocument:
      return flat::ResourceType_SUBDOCUMENT;
    case ResourceType::kFont:
      return flat::ResourceType_FONT;
    case ResourceType::kMedia:
      return flat::ResourceType_MEDIA;
    case ResourceType::kWebSocket:
      return flat::ResourceType_WEBSOCKET;
    case ResourceType::kWebBundle:
      return flat::ResourceType_WEBBUNDLE;
    case ResourceType::kWebRTC:
      return flat::ResourceType_WEBRTC;
    case ResourceType::kPing:
      return flat::ResourceType_PING;
    case ResourceType::kWebTransport:
      return flat::ResourceType_WEBTRANSPORT;
    case ResourceType::kOther:
      return flat::ResourceType_OTHER;
    case ResourceType::kDocument:
      return flat::ResourceType_DOCUMENT;
    case ResourceType::kPopup:
      return flat::ResourceType_POPUP;
    case ResourceType::kPopunder:
      return flat::ResourceType_POPUNDER;
  }
}
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_INDEX_INDEX_UTILS_H_
