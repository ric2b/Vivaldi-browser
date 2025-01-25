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
#include "components/omnibox/bookmark_nickname_match_utils.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/scoring_functor.h"
#include "components/omnibox/browser/titled_url_match_utils.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/url_constants.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

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

  PrefService* prefs = client_->GetPrefs();
  if (!prefs->GetBoolean(vivaldiprefs::kAddressBarOmniboxShowNicknames)) {
    return;
  }

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
  std::vector<TitledUrlMatch> matches =
      bookmark_model_->GetNicknameMatching(input.text(), kMaxBookmarkMatches);

  if (matches.empty())
    return;  // There were no matches.

  std::u16string input_text(input.text());

  for (auto& bookmark_match : matches) {
    // Score the TitledUrlMatch. If its score is greater than 0 then the
    // AutocompleteMatch is created and added to matches_.
    auto [relevance, bookmark_count] =
        CalculateBookmarkMatchRelevance(bookmark_match);

    if (relevance > 0) {
      AutocompleteMatch match = NicknameMatchToAutocompleteMatch(
          bookmark_match, AutocompleteMatchType::BOOKMARK_NICKNAME, relevance,
          bookmark_count, this, client_->GetSchemeClassifier(), input,
          input_text);

      if (match.inline_autocompletion.empty() &&
          input_text.length() !=
              bookmark_match.node->GetTitledUrlNodeNickName().length()) {
        match.relevance = 0;
      }

      match.allowed_to_be_default_match =
          !input.prevent_inline_autocomplete() ||
          match.inline_autocompletion.empty();

      match.nickname = bookmark_match.node->GetTitledUrlNodeNickName();

      if (!input.prevent_inline_autocomplete() && match.relevance > 0) {
        matches_.push_back(match);
      }
    }
  }
}

std::pair<int, int> BookmarkNicknameProvider::CalculateBookmarkMatchRelevance(
    const TitledUrlMatch& bookmark_match) const {
  size_t nickname_length =
      bookmark_match.node->GetTitledUrlNodeNickName().length();

  ScoringFunctor nickname_position_functor =
      for_each(bookmark_match.nickname_match_positions.begin(),
               bookmark_match.nickname_match_positions.end(),
               ScoringFunctor(nickname_length));
  const double normalized_sum = std::min(
      nickname_position_functor.ScoringFactor() / (nickname_length + 10), 1.0);

  const GURL& url(bookmark_match.node->GetTitledUrlNodeUrl());

  const int kBaseBookmarkNicknameScore = 1460;
  const int kMaxBookmarkScore = 1599;
  const double kBookmarkScoreRange =
      static_cast<double>(kMaxBookmarkScore - kBaseBookmarkNicknameScore);
  int relevance = static_cast<int>(normalized_sum * kBookmarkScoreRange) +
                  kBaseBookmarkNicknameScore;

  const size_t url_node_count = bookmark_model_->GetNodesByURL(url).size();

  relevance = std::min(kMaxBookmarkScore, relevance);
  return {relevance, url_node_count};
}
