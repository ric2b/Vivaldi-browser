// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_REQUEST_FILTER_RULE_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_REQUEST_FILTER_RULE_H_

#include <bitset>
#include <optional>
#include <ostream>
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

  enum ActivationTypes {
    kPopup = 0,
    kDocument,
    kElementHide,
    kGenericHide,
    kGenericBlock,
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

  enum ModifierType { kNoModifier = -1, kRedirect, kCsp };

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
  std::optional<std::string> modifier_value;
  // Affect whether some part of the filter run for given documents.
  std::bitset<kActivationCount> activation_types;

  bool is_case_sensitive = false;

  std::bitset<kTypeCount> resource_types;
  std::bitset<kPartyCount> party;
  std::bitset<kAnchorTypeCount> anchor_type;
  PatternType pattern_type = kPlain;

  // Limit the rule to a specific host.
  std::optional<std::string> host;
  std::vector<std::string> included_domains;
  std::vector<std::string> excluded_domains;

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
