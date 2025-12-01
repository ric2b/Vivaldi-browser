// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_PUBLIC_CORE_ADBLOCK_REQUEST_FILTER_RULE_TYPES_H_
#define COMPONENTS_AD_BLOCKER_PUBLIC_CORE_ADBLOCK_REQUEST_FILTER_RULE_TYPES_H_

#include "base/containers/enum_array.h"
#include "base/containers/enum_set.h"

namespace adblock_filter {

enum class ResourceType {
  kMin = 0,
  kMinRegular = kMin,
  kStylesheet = kMinRegular,
  kImage,
  kObject,
  kScript,
  kXmlHttpRequest,
  kSubDocument,
  kFont,
  kMedia,
  kWebSocket,
  kWebRTC,
  kPing,
  kWebTransport,
  kWebBundle,
  kOther,
  kMaxRegular = kOther,
  kMinExplicit,
  kDocument = kMinExplicit,
  kPopup,
  kPopunder,
  kMaxExplicit = kPopunder,
  kMax = kMaxExplicit,
};
using ResourceTypes =
    base::EnumSet<ResourceType, ResourceType::kMin, ResourceType::kMax>;
using RegularResourceTypes = base::
    EnumSet<ResourceType, ResourceType::kMinRegular, ResourceType::kMaxRegular>;
using ExplicitResourceTypes = base::EnumSet<ResourceType,
                                            ResourceType::kMinExplicit,
                                            ResourceType::kMaxExplicit>;

enum class ActivationType {
  kMin = 0,
  kWholeDocument = kMin,
  kSpecificHide,
  kGenericHide,
  kGenericBlock,
  kAttributeAds,
  kActivationCount,
  kMax = kActivationCount,
};
using ActivationTypes =
    base::EnumSet<ActivationType, ActivationType::kMin, ActivationType::kMax>;

enum class RequestMethod {
  kMin = 0,
  KConnect = kMin,
  kDelete,
  kGet,
  kHead,
  kOptions,
  kPatch,
  kPost,
  kPut,
  kOther,
  kMax = kOther,
};

using RequestMethods =
    base::EnumSet<RequestMethod, RequestMethod::kMin, RequestMethod::kMax>;

enum class Party {
  kFirst = 0,
  kThird,
  kStrictFirst,
  kStrictThird,
  kFirstAndStrictThird
};

enum class RuleDecision { kModify, kPass, kModifyImportant };

enum class ModifierType {
  kNoModifier = -1,
  kFirst,
  kRedirect = kFirst,
  kCsp,
  kAdQueryTrigger,
  kLast = kAdQueryTrigger
};

struct RequestFilterRuleStub {
  // Whether a match causes the request to be modified or passed as-is.
  RuleDecision decision = RuleDecision::kModify;

  // Whether the rule modifies the blocked state of the request.
  bool modify_block = false;

  // Whether this rule is used by ad attribution.
  bool is_attribution_allow_rule = false;

  // The text of the rule as it was before parsing. Used for logging.
  std::string original_rule_text;

  // The calculated rule priority
  int priority = -1;

  // The rule source which had the rule from which this stub was generated.
  uint32_t rule_source_id;
};

struct ActivationResult {
  bool from_parent = false;
  std::optional<RequestFilterRuleStub> rule_stub;

  bool IsDecision(RuleDecision match_decision) const;
};

struct ActivationResults {
  bool document_exception = false;
  base::EnumArray<ActivationResult,
                  ActivationType,
                  ActivationType::kMin,
                  ActivationType::kMax>
      by_type;

  bool IsDocumentDecision(RuleDecision decision) const;
};
inline constexpr ActivationResults kEmptyActivationResults;

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_PUBLIC_CORE_ADBLOCK_REQUEST_FILTER_RULE_TYPES_H_
