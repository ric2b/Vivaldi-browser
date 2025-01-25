// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_REQUEST_FILTER_RULE_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_REQUEST_FILTER_RULE_H_

#include <bitset>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace adblock_filter {

struct RequestFilterRule {
 public:
  enum ResourceType {
    kStylesheet = 0,
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
    kTypeCount
  };

  enum ExplicitResourceType { kDocument = 0, kPopup, kExplicitTypeCount };

  enum ActivationTypes {
    kWholeDocument = 0,
    kElementHide,
    kGenericHide,
    kGenericBlock,
    kAttributeAds,
    kActivationCount
  };

  enum Party { kFirstParty = 0, kThirdParty, kPartyCount };

  enum AnchorType {
    kAnchorStart = 0,
    kAnchorEnd,
    kAnchorHost,
    kAnchorTypeCount
  };

  enum PatternType {
    kPlain,
    kWildcarded,
    kRegex,
  };

  enum Decision { kModify, kPass, kModifyImportant };

  enum ModifierType { kNoModifier = -1, kRedirect, kCsp, kAdQueryTrigger };

  RequestFilterRule();
  ~RequestFilterRule();
  RequestFilterRule(RequestFilterRule&& request_filter_rule);
  RequestFilterRule& operator=(RequestFilterRule&& request_filter_rule);
  bool operator==(const RequestFilterRule& other) const;

  // Whether a match causes the request to be modified or passed as-is.
  Decision decision = kModify;
  // Whether the rule modifies the blocked state of the request.
  bool modify_block = true;
  // Other modification (redirect, CSP rules).
  ModifierType modifier = kNoModifier;
  std::set<std::string> modifier_values;
  // Affect whether some part of the filter run for given documents.
  std::bitset<kActivationCount> activation_types;

  std::set<std::string> ad_domains_and_query_triggers;

  bool is_case_sensitive = false;

  std::bitset<kTypeCount> resource_types;
  // These are handled like resource types, but do not get enabled if a
  // rule has no resource type associated. We keep them separate to easy
  // implementation.
  std::bitset<kExplicitTypeCount> explicit_types;
  std::bitset<kPartyCount> party;
  std::bitset<kAnchorTypeCount> anchor_type;
  PatternType pattern_type = kPlain;

  // Limit the rule to a specific host.
  std::optional<std::string> host;
  std::set<std::string> included_domains;
  std::set<std::string> excluded_domains;

  std::string pattern;
  // For regex patterns, this provides a string from which ngrams can be safely
  // extracted for indexing.
  std::optional<std::string> ngram_search_string;
};

using RequestFilterRules = std::vector<RequestFilterRule>;

// Used for unit tests.
std::ostream& operator<<(std::ostream& os, const RequestFilterRule& rule);
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_REQUEST_FILTER_RULE_H_
