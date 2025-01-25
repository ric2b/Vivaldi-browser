// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

// The matching logic distinguishes between the terms URL pattern and
// subpattern. A URL pattern usually stands for the full thing, e.g.
// "example.com^*path*par=val^", whereas subpattern denotes a maximal substring
// of a pattern not containing the wildcard '*' character. For the example above
// the subpatterns are: "example.com^", "path" and "par=val^".
//
// The separator placeholder '^' symbol is used in subpatterns to match any
// separator character, which is any ASCII symbol except letters, digits, and
// the following: '_', '-', '.', '%'. Note that the separator placeholder
// character '^' is itself a separator, as well as '\0'.

#include "components/request_filter/adblock_filter/adblock_rule_pattern_matcher.h"

#include <stddef.h>

#include <ostream>

#include "base/check_op.h"
#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/request_filter/adblock_filter/utils.h"
#include "components/url_pattern_index/fuzzy_pattern_matching.h"
#include "components/url_pattern_index/string_splitter.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"

namespace adblock_filter {

namespace {

constexpr char kWildcard = '*';

class IsWildcard {
 public:
  bool operator()(char c) const { return c == kWildcard; }
};

// Returns whether |position| within the |url| belongs to its |host| component
// and corresponds to the beginning of a (sub-)domain.
inline bool IsSubdomainAnchored(std::string_view url,
                                url::Component host,
                                size_t position) {
  DCHECK_LE(position, url.size());
  const size_t host_begin = static_cast<size_t>(host.begin);
  const size_t host_end = static_cast<size_t>(host.end());
  DCHECK_LE(host_end, url.size());

  return position == host_begin ||
         (position > host_begin && position <= host_end &&
          url[position - 1] == '.');
}

// Returns the position of the leftmost occurrence of a |subpattern| in the
// |text| starting no earlier than |from| the specified position. If the
// |subpattern| has separator placeholders, searches for a fuzzy occurrence.
size_t FindSubpattern(std::string_view text,
                      std::string_view subpattern,
                      size_t from = 0) {
  const bool is_fuzzy =
      (subpattern.find(url_pattern_index::kSeparatorPlaceholder) !=
       std::string_view::npos);
  return is_fuzzy ? url_pattern_index::FindFuzzy(text, subpattern, from)
                  : text.find(subpattern, from);
}

// Same as FindSubpattern(url, subpattern), but searches for an occurrence that
// starts at the beginning of a (sub-)domain within the url's |host| component.
size_t FindSubdomainAnchoredSubpattern(std::string_view url,
                                       url::Component host,
                                       std::string_view subpattern) {
  const bool is_fuzzy =
      (subpattern.find(url_pattern_index::kSeparatorPlaceholder) !=
       std::string_view::npos);

  // Any match found after the end of the host will be discarded, so just
  // avoid searching there for the subpattern to begin with.
  //
  // Check for overflow.
  size_t max_match_end = 0;
  if (!base::CheckAdd(host.end(), subpattern.length())
           .AssignIfValid(&max_match_end)) {
    return std::string_view::npos;
  }
  const std::string_view url_match_candidate = url.substr(0, max_match_end);
  const std::string_view url_host = url.substr(0, host.end());

  for (size_t position = static_cast<size_t>(host.begin);
       position <= static_cast<size_t>(host.end()); ++position) {
    // Enforce as a loop precondition that we are always anchored at a
    // sub-domain before calling find. This is to reduce the number of potential
    // searches for |subpattern|.
    DCHECK(IsSubdomainAnchored(url, host, position));

    position = is_fuzzy ? url_pattern_index::FindFuzzy(url_match_candidate,
                                                       subpattern, position)
                        : url_match_candidate.find(subpattern, position);
    if (position == std::string_view::npos ||
        IsSubdomainAnchored(url, host, position)) {
      return position;
    }

    // Enforce the loop precondition. This skips |position| to the next '.',
    // within the host, which the loop itself increments to the anchored
    // sub-domain.
    position = url_host.find('.', position);
    if (position == std::string_view::npos)
      break;
  }
  return std::string_view::npos;
}

// Helper for DoesTextMatchLastSubpattern. Treats kSeparatorPlaceholder as not
// matching the end of the text.
bool DoesTextMatchLastSubpatternInternal(uint8_t anchor_type,
                                         std::string_view text,
                                         url::Component url_host,
                                         std::string_view subpattern) {
  // Enumerate all possible combinations of |anchor_left| and |anchor_right|.
  if (anchor_type == flat::AnchorType_NONE) {
    return FindSubpattern(text, subpattern) != std::string_view::npos;
  }

  if (anchor_type == flat::AnchorType_END) {
    return url_pattern_index::EndsWithFuzzy(text, subpattern);
  }

  if (anchor_type == flat::AnchorType_START) {
    return url_pattern_index::StartsWithFuzzy(text, subpattern);
  }

  if (anchor_type == (flat::AnchorType_START | flat::AnchorType_END)) {
    return text.size() == subpattern.size() &&
           url_pattern_index::StartsWithFuzzy(text, subpattern);
  }

  if (anchor_type == flat::AnchorType_HOST) {
    return url_host.is_nonempty() &&
           FindSubdomainAnchoredSubpattern(text, url_host, subpattern) !=
               std::string_view::npos;
  }

  if (anchor_type == (flat::AnchorType_HOST | flat::AnchorType_END)) {
    return url_host.is_nonempty() && text.size() >= subpattern.size() &&
           IsSubdomainAnchored(text, url_host,
                               text.size() - subpattern.size()) &&
           url_pattern_index::EndsWithFuzzy(text, subpattern);
  }

  NOTREACHED();
}

// Matches the last |subpattern| against |text|. Special treatment is required
// for the last subpattern since |kSeparatorPlaceholder| can also match the end
// of the text.
bool DoesTextMatchLastSubpattern(uint8_t anchor_type,
                                 std::string_view text,
                                 url::Component url_host,
                                 std::string_view subpattern) {
  DCHECK(!subpattern.empty());

  if (DoesTextMatchLastSubpatternInternal(anchor_type, text, url_host,
                                          subpattern)) {
    return true;
  }

  // If the last |subpattern| ends with kSeparatorPlaceholder, then it can also
  // match the end of text.
  if (subpattern.back() == url_pattern_index::kSeparatorPlaceholder) {
    subpattern.remove_suffix(1);
    return DoesTextMatchLastSubpatternInternal(
        anchor_type | flat::AnchorType_END, text, url_host, subpattern);
  }

  return false;
}

// Returns whether the given |url_pattern| matches the given |url_spec|.
// Compares the pattern the the url in a case-sensitive manner.
bool IsCaseSensitiveMatch(std::string_view url_pattern,
                          uint8_t anchor_type,
                          std::string_view url_spec,
                          url::Component url_host) {
  DCHECK(!url_spec.empty());

  url_pattern_index::StringSplitter<IsWildcard> subpatterns(url_pattern);
  auto subpattern_it = subpatterns.begin();
  auto subpattern_end = subpatterns.end();

  // Empty patterns were handled earlier and patterns consisting entirely of
  // '*' were turned to empty at the parsing stage
  DCHECK(subpattern_it != subpattern_end);

  std::string_view subpattern = *subpattern_it;
  ++subpattern_it;

  // There is only one |subpattern|.
  if (subpattern_it == subpattern_end) {
    return DoesTextMatchLastSubpattern(anchor_type, url_spec, url_host,
                                       subpattern);
  }

  // Otherwise, the first |subpattern| does not have to be a suffix. But it
  // still can have a left anchor. Check and handle that.
  std::string_view text = url_spec;
  if ((anchor_type & flat::AnchorType_START) != 0) {
    if (!url_pattern_index::StartsWithFuzzy(url_spec, subpattern))
      return false;
    text.remove_prefix(subpattern.size());
  } else if ((anchor_type & flat::AnchorType_HOST) != 0) {
    if (url_host.is_empty())
      return false;
    const size_t match_begin =
        FindSubdomainAnchoredSubpattern(url_spec, url_host, subpattern);
    if (match_begin == std::string_view::npos)
      return false;
    text.remove_prefix(match_begin + subpattern.size());
  } else {
    // Get back to the initial |subpattern|, process it in the loop below.
    subpattern_it = subpatterns.begin();
  }

  DCHECK(subpattern_it != subpattern_end);
  subpattern = *subpattern_it;

  // Consecutively find all the remaining subpatterns in the |text|. Handle the
  // last subpattern outside the loop.
  while (++subpattern_it != subpattern_end) {
    DCHECK(!subpattern.empty());

    const size_t match_position = FindSubpattern(text, subpattern);
    if (match_position == std::string_view::npos)
      return false;
    text.remove_prefix(match_position + subpattern.size());

    subpattern = *subpattern_it;
  }

  return DoesTextMatchLastSubpattern(anchor_type & flat::AnchorType_END, text,
                                     url::Component(), subpattern);
}

}  // namespace

RulePatternMatcher::UrlInfo::UrlInfo(const GURL& url)
    : spec_(url.possibly_invalid_spec()),
      fold_case_spec_owner_(
          base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(spec_)))),
      fold_case_spec_(fold_case_spec_owner_),
      host_(url.parsed_for_possibly_invalid_spec().host) {
  DCHECK(url.is_valid());
}

RulePatternMatcher::UrlInfo::~UrlInfo() = default;

RulePatternMatcher::RulePatternMatcher(const flat::RequestFilterRule& rule)
    : rule_(rule), pattern_(ToStringPiece(rule_->pattern())) {
  DCHECK(rule_->pattern_type() == flat::PatternType_PLAIN ||
         rule_->pattern_type() == flat::PatternType_WILDCARDED);
  DCHECK((rule_->anchor_type() & flat::AnchorType_START) == 0 ||
         (rule_->anchor_type() & flat::AnchorType_HOST) == 0);
  DCHECK((rule_->anchor_type() &
          (flat::AnchorType_START | flat::AnchorType_HOST)) == 0 ||
         !base::StartsWith(pattern_, "*"));
  DCHECK((rule_->anchor_type() & (flat::AnchorType_END)) == 0 ||
         !base::EndsWith(pattern_, "*"));
}

RulePatternMatcher::~RulePatternMatcher() = default;

bool RulePatternMatcher::MatchesUrl(const UrlInfo& url) const {
  if (pattern_.empty())
    return true;

  if ((rule_->options() & flat::OptionFlag_IS_CASE_SENSITIVE) != 0) {
    return IsCaseSensitiveMatch(pattern_, rule_->anchor_type(), url.spec(),
                                url.host());
  } else {
    return IsCaseSensitiveMatch(pattern_, rule_->anchor_type(),
                                url.fold_case_spec(), url.host());
  }
}

}  // namespace adblock_filter
