// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_ADBLOCK_REQUEST_FILTER_RULE_H_
#define COMPONENTS_AD_BLOCKER_ADBLOCK_REQUEST_FILTER_RULE_H_

#include <bitset>
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

  RequestFilterRule();
  ~RequestFilterRule();
  RequestFilterRule(RequestFilterRule&& request_filter_rule);
  RequestFilterRule& operator=(RequestFilterRule&& request_filter_rule);
  bool operator==(const RequestFilterRule& other) const;

  bool is_allow_rule = false;
  bool is_case_sensitive = false;

  // Tells us whether this rule is supposed to modify the CSP
  // Needed because allow CSP rules can use an empty CSP.
  bool is_csp_rule = false;

  std::bitset<kTypeCount> resource_types;
  std::bitset<kActivationCount> activation_types;
  std::bitset<kPartyCount> party;
  std::bitset<kAnchorTypeCount> anchor_type;
  PatternType pattern_type = kPlain;

  // Limit the rule to a specific host.
  std::string host;
  std::vector<std::string> included_domains;
  std::vector<std::string> excluded_domains;

  std::string redirect;
  std::string csp;

  std::string pattern;
  // For regex patterns, this provides a string from which ngrams can be safely
  // extracted for indexing.
  std::string ngram_search_string;
};

using RequestFilterRules = std::vector<RequestFilterRule>;

// Used for unit tests.
std::ostream& operator<<(std::ostream& os, const RequestFilterRule& rule);
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_ADBLOCK_REQUEST_FILTER_RULE_H_
