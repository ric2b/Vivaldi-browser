// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_PATTERN_MATCHER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_PATTERN_MATCHER_H_

#include <string>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "url/third_party/mozilla/url_parse.h"

class GURL;

namespace adblock_filter {

namespace flat {
struct RequestFilterRule;  // The FlatBuffers version of UrlRule.
}

// The structure used to match the pattern of a RequestFilterRule against URLs.
class RulePatternMatcher {
 public:
  // A wrapper over a GURL to reduce redundant computation.
  class UrlInfo {
   public:
    // The |url| must outlive this instance.
    explicit UrlInfo(const GURL& url);
    ~UrlInfo();
    UrlInfo(const UrlInfo&) = delete;
    UrlInfo& operator=(const UrlInfo&) = delete;

    std::string_view spec() const { return spec_; }
    std::string_view fold_case_spec() const { return fold_case_spec_; }
    url::Component host() const { return host_; }

   private:
    // The url spec.
    const std::string_view spec_;
    // String to hold the case-folded spec.
    const std::string fold_case_spec_owner_;
    // Reference to the case-folded spec.
    std::string_view fold_case_spec_;

    // The url host component.
    const url::Component host_;
  };

  // The passed in |rule| must outlive the created instance.
  explicit RulePatternMatcher(const flat::RequestFilterRule& rule);
  ~RulePatternMatcher();
  RulePatternMatcher(const RulePatternMatcher&) = delete;
  RulePatternMatcher& operator=(const RulePatternMatcher&) = delete;

  // Returns whether the |url| matches the URL |pattern|. Requires the type of
  // |this| pattern to be either SUBSTRING or WILDCARDED.
  //
  // Splits the pattern into subpatterns separated by '*' wildcards, and
  // greedily finds each of them in the spec of the |url|. Respects anchors at
  // either end of the pattern, and '^' separator placeholders when comparing a
  // subpattern to a subtring of the spec.
  bool MatchesUrl(const UrlInfo& url) const;

 private:
  const raw_ref<const flat::RequestFilterRule> rule_;
  std::string_view pattern_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_PATTERN_MATCHER_H_
