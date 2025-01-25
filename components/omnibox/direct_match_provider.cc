// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/direct_match_provider.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/trace_event.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "components/direct_match/direct_match_service.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/scoring_functor.h"
#include "components/omnibox/browser/titled_url_match_utils.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

DirectMatchProvider::DirectMatchProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_DIRECT_MATCH),
      client_(client),
      direct_match_service_(client ? client_->GetDirectMatchService()
                                   : nullptr) {}

void DirectMatchProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes) {
  matches_.clear();
  PrefService* prefs = client_->GetPrefs();
  if (!prefs->GetBoolean(vivaldiprefs::kAddressBarSearchDirectMatchEnabled)) {
    return;
  }

  if (input.IsZeroSuggest() || input.text().empty()) {
    return;
  }

  StartDirectMatchSearch(input);
}

DirectMatchProvider::~DirectMatchProvider() = default;

AutocompleteMatch DirectMatchToAutocompleteMatch(
    const direct_match::DirectMatchService::DirectMatchUnit& direct_match,
    AutocompleteMatchType::Type type,
    int relevance,

    AutocompleteProvider* provider,
    const AutocompleteSchemeClassifier& scheme_classifier,
    const AutocompleteInput& input) {
  AutocompleteMatch match(provider, relevance, false, type);

  const std::u16string fixed_up_input_text = input.text();

  const std::u16string title = base::UTF8ToUTF16(direct_match.title);
  const std::u16string name = base::UTF8ToUTF16(direct_match.name);
  const std::u16string local_favicon_path =
      base::UTF8ToUTF16(direct_match.image_path);
  const GURL& redirect_url = GURL(direct_match.redirect_url);

  match.destination_url = redirect_url;
  match.RecordAdditionalInfo("Title", title);
  match.RecordAdditionalInfo("URL", redirect_url.spec());
  match.RecordAdditionalInfo("Name", name);
  match.RecordAdditionalInfo("Icon", local_favicon_path);

  match.contents = name;
  match.description = title;
  match.local_favicon_path = local_favicon_path;

  auto contents_terms = FindTermMatches(input.text(), match.contents);
  match.contents_class = ClassifyTermMatches(
      contents_terms, match.contents.length(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  base::TrimWhitespace(match.description, base::TRIM_LEADING,
                       &match.description);
  auto description_terms = FindTermMatches(input.text(), match.description);
  match.description_class = ClassifyTermMatches(
      description_terms, match.description.length(),
      ACMatchClassification::MATCH, ACMatchClassification::NONE);

  // The inline_autocomplete_offset should be adjusted based on the formatting
  // applied to |fill_into_edit|.
  size_t inline_autocomplete_offset = URLPrefix::GetInlineAutocompleteOffset(
      input.text(), fixed_up_input_text, false, title);

  match.fill_into_edit = title;
  if (match.TryRichAutocompletion(match.contents, match.description, input)) {
    // If rich autocompletion applies, we skip trying the alternatives below.
  } else if (inline_autocomplete_offset != std::u16string::npos) {
    match.inline_autocompletion =
        match.fill_into_edit.substr(inline_autocomplete_offset);
    match.SetAllowedToBeDefault(input);
  }

  return match;
}

void DirectMatchProvider::StartDirectMatchSearch(
    const AutocompleteInput& input) {
  if (!direct_match_service_ || input.prevent_inline_autocomplete()) {
    return;
  }

  const std::u16string input_text = input.text();
  const std::string input_text_utf8 = base::UTF16ToUTF8(input_text);

  auto [ unit_found, allowed_to_be_default_match ] =
      direct_match_service_->GetDirectMatch(input_text_utf8);
  if (!unit_found) {
    return;
  }
  query_parser::QueryWordVector dm_words;
  query_parser::QueryParser::ExtractQueryWords(input_text, &dm_words);

  query_parser::QueryNodeVector query_nodes;
  query_parser::QueryParser::ParseQueryNodes(
      input_text, query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH,
      &query_nodes);

  query_parser::Snippet::MatchPositions dm_match_pos;
  for (const auto& query_node : query_nodes) {
    query_node->HasMatchIn(dm_words, &dm_match_pos);
  }
  PrefService* prefs = client_->GetPrefs();
  bool dm_boost =
      prefs->GetBoolean(vivaldiprefs::kAddressBarSearchDirectMatchBoosted);

  auto relevance =
      CalculateDirectMatchRelevance(*unit_found, &dm_match_pos, dm_boost);

  AutocompleteMatch match = DirectMatchToAutocompleteMatch(
      *unit_found, AutocompleteMatchType::DIRECT_MATCH, relevance, this,
      client_->GetSchemeClassifier(), input);
  if (match.relevance > 0) {
    match.boosted = dm_boost;
    match.allowed_to_be_default_match = allowed_to_be_default_match;
    match.RecordAdditionalInfo("Title", unit_found->name);
    match.RecordAdditionalInfo("URL", unit_found->redirect_url);
    match.RecordAdditionalInfo("Path", unit_found->title);
    matches_.push_back(match);
  }
}

int DirectMatchProvider::CalculateDirectMatchRelevance(
    const direct_match::DirectMatchService::DirectMatchUnit& direct_match,
    query_parser::Snippet::MatchPositions* match_positions,
    bool dm_boost) const {
  size_t direct_match_name_length = direct_match.name.length();

  ScoringFunctor dm_position_functor =
      for_each(match_positions->begin(), match_positions->end(),
               ScoringFunctor(direct_match_name_length));

  const double normalized_sum = std::min(
      dm_position_functor.ScoringFactor() / (direct_match_name_length + 10),

      1.0);
  const int kBaseDirectMatchScore = 1450;
  const int kMaxDirectMatchScore = 1589;
  const double kDMScoreRange =
      static_cast<double>(kMaxDirectMatchScore - kBaseDirectMatchScore);
  int relevance =
      static_cast<int>(normalized_sum * kDMScoreRange) + kBaseDirectMatchScore;

  return std::min(kMaxDirectMatchScore, relevance + (dm_boost ? 0 : -450));
}
