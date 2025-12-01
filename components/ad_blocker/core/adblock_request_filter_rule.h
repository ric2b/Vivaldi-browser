// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CORE_ADBLOCK_REQUEST_FILTER_RULE_H_
#define COMPONENTS_AD_BLOCKER_CORE_ADBLOCK_REQUEST_FILTER_RULE_H_

#include <bitset>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"

namespace adblock_filter {

struct RequestFilterRule {
 public:
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

  // Whether this rule should be used to fully eliminate an existing, equivalent
  // rule.
  bool bad_filter = false;

  // Whether a match causes the request to be modified or passed as-is.
  RuleDecision decision = RuleDecision::kModify;
  // Whether the rule modifies the blocked state of the request.
  bool modify_block = true;
  // Other modification (redirect, CSP rules).
  ModifierType modifier = ModifierType::kNoModifier;
  std::set<std::string> modifier_values;
  // Affect whether some part of the filter run for given documents.
  ActivationTypes activation_types;

  // For which domain and query-triggers should this ad-attribution allow rule
  // apply.
  std::set<std::string> ad_domains_and_query_triggers;

  bool is_case_sensitive = false;

  RegularResourceTypes resource_types;
  // These are handled like resource types, but do not get enabled if a
  // rule has no resource type associated. We keep them separate to easy
  // implementation.
  ExplicitResourceTypes explicit_types;
  RequestMethods request_methods = RequestMethods::All();
  std::optional<Party> party;
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

  // The text of the rule as it was before parsing. Used for logging.
  std::string original_rule_text;
};

using RequestFilterRules = std::vector<RequestFilterRule>;

// Used for unit tests.
std::ostream& operator<<(std::ostream& os, const RequestFilterRule& rule);
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CORE_ADBLOCK_REQUEST_FILTER_RULE_H_
