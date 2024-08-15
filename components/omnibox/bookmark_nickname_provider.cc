// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/bookmark_nickname_provider.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/trace_event.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/scoring_functor.h"
#include "components/omnibox/browser/titled_url_match_utils.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

using bookmarks::BookmarkNode;
using bookmarks::TitledUrlMatch;

BookmarkNicknameProvider::BookmarkNicknameProvider(
    AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_BOOKMARK_NICKNAME),
      client_(client),
      bookmark_model_(client ? client_->GetBookmarkModel() : nullptr) {}

void BookmarkNicknameProvider::Start(const AutocompleteInput& input,
                                     bool minimal_changes) {
  TRACE_EVENT0("omnibox", "BookmarkNicknameProvider::Start");
  matches_.clear();

  if (input.IsZeroSuggest() || input.text().empty()) {
    return;
  }

  DoAutocomplete(input);
}

BookmarkNicknameProvider::~BookmarkNicknameProvider() = default;

void BookmarkNicknameProvider::DoAutocomplete(const AutocompleteInput& input) {
  // We may not have a bookmark model for some unit tests.
  if (!bookmark_model_) {
    return;
  }
  // Retrieve enough bookmarks so that we have a reasonable probability of
  // suggesting the one that the user desires.
  const size_t kMaxBookmarkMatches = 50;

  // Remove the keyword from input if we're in keyword mode for a starter pack
  // engine.
  const auto [adjusted_input, starter_pack_engine] =
      KeywordProvider::AdjustInputForStarterPackEngines(
          input, client_->GetTemplateURLService());

  // GetNicknameMatching returns bookmarks matching the user's
  // search terms using the following rules:
  //  - The search text is broken up into search terms. Each term is searched
  //    for separately.
  //  - Term matches are always performed against the start of a word. 'def'
  //    will match against 'define' but not against 'indefinite'.
  //  - Terms perform partial word matches only if the the total search text
  //    length is at least 3 characters.
  //  - A search containing multiple terms will return results with those words
  //    occurring in any order.
  //  - Terms enclosed in quotes comprises a phrase that must match exactly.
  //  - Multiple terms enclosed in quotes will require those exact words in that
  //    exact order to match.
  //
  std::vector<TitledUrlMatch> matches = bookmark_model_->GetNicknameMatching(
      adjusted_input.text(), kMaxBookmarkMatches);

  if (matches.empty())
    return;  // There were no matches.

  const std::u16string fixed_up_input(FixupUserInput(adjusted_input).second);
  for (auto& bookmark_match : matches) {
    // Score the TitledUrlMatch. If its score is greater than 0 then the
    // AutocompleteMatch is created and added to matches_.
    auto [relevance, bookmark_count] =
        CalculateBookmarkMatchRelevance(bookmark_match);

    relevance = relevance * 10;
    if (relevance > 0) {
      AutocompleteMatch match = TitledUrlMatchToAutocompleteMatch(
          bookmark_match, AutocompleteMatchType::BOOKMARK_NICKNAME,
          relevance, bookmark_count, this, client_->GetSchemeClassifier(),
          adjusted_input, fixed_up_input);
      // If the input was in a starter pack keyword scope, set the `keyword` and
      // `transition` appropriately to avoid popping the user out of keyword
      // mode.
      if (starter_pack_engine) {
        match.keyword = starter_pack_engine->keyword();
        match.transition = ui::PAGE_TRANSITION_KEYWORD;
      }

      match.allowed_to_be_default_match = true;

      matches_.push_back(match);
    }
  }

  // In keyword mode, it's possible we only provide results from one or two
  // autocomplete provider(s), so it's sometimes necessary to show more results
  // than provider_max_matches_.
  size_t max_matches = adjusted_input.InKeywordMode()
                           ? provider_max_matches_in_keyword_mode_
                           : provider_max_matches_;

  // Sort and clip the resulting matches.
  size_t num_matches = std::min(matches_.size(), max_matches);
  std::partial_sort(matches_.begin(), matches_.begin() + num_matches,
                    matches_.end(), AutocompleteMatch::MoreRelevant);
  ResizeMatches(
      num_matches,
      OmniboxFieldTrial::IsMlUrlScoringUnlimitedNumCandidatesEnabled());
}

std::pair<int, int> BookmarkNicknameProvider::CalculateBookmarkMatchRelevance(
    const TitledUrlMatch& bookmark_match) const {


  size_t title_length = bookmark_match.node->GetTitledUrlNodeTitle().length();
  size_t url_length =
      bookmark_match.node->GetTitledUrlNodeUrl().spec().length();
  size_t length = title_length > 0 ? title_length : url_length;

  ScoringFunctor title_position_functor = for_each(
      bookmark_match.title_match_positions.begin(),
      bookmark_match.title_match_positions.end(), ScoringFunctor(title_length));
  ScoringFunctor url_position_functor = for_each(
      bookmark_match.url_match_positions.begin(),
      bookmark_match.url_match_positions.end(), ScoringFunctor(url_length));
  const double title_match_strength = title_position_functor.ScoringFactor();
  const double summed_factors =
      title_match_strength + url_position_functor.ScoringFactor();
  const double normalized_sum = std::min(summed_factors / (length + 10), 1.0);

  // Bookmarks with javascript scheme ("bookmarklets") that do not have title
  // matches get a lower base and lower maximum score because returning them
  // for matches in their (often very long) URL looks stupid and is often not
  // intended by the user.
  const GURL& url(bookmark_match.node->GetTitledUrlNodeUrl());
  const bool bookmarklet_without_title_match =
      url.SchemeIs(url::kJavaScriptScheme) && title_match_strength == 0.0;
  const int kBaseBookmarkScore = bookmarklet_without_title_match ? 400 : 900;
  const int kMaxBookmarkScore = bookmarklet_without_title_match ? 799 : 1199;
  const double kBookmarkScoreRange =
      static_cast<double>(kMaxBookmarkScore - kBaseBookmarkScore);
  int relevance = static_cast<int>(normalized_sum * kBookmarkScoreRange) +
                  kBaseBookmarkScore;

  // If scoring signal logging and ML scoring is disabled, skip counting
  // bookmarks if relevance is above max score. Don't waste any time searching
  // for additional referenced URLs if we already have a perfect title match.
  // Returns a pair of the relevance score and -1 as a dummy bookmark count.
  if (!OmniboxFieldTrial::IsPopulatingUrlScoringSignalsEnabled() &&
      relevance >= kMaxBookmarkScore) {
    return {relevance, /*bookmark_count=*/-1};
  }

  // Boost the score if the bookmark's URL is referenced by other bookmarks.
  const int kURLCountBoost[4] = {0, 75, 125, 150};

  const size_t url_node_count = bookmark_model_->GetNodeCountByURL(url);
  DCHECK_GE(std::min(std::size(kURLCountBoost), url_node_count), 1U);
  relevance +=
      kURLCountBoost[std::min(std::size(kURLCountBoost), url_node_count) - 1];
  relevance = std::min(kMaxBookmarkScore, relevance);
  return {relevance, url_node_count};
}
