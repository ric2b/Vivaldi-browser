// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/titled_url_index.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/titled_url_node.h"
#include "components/query_parser/query_parser.h"

namespace bookmarks {

std::vector<TitledUrlMatch> TitledUrlIndex::MatchNicknameNodesWithQuery(
    const TitledUrlNodes& nodes,
    const query_parser::QueryNodeVector& query_nodes,
    const std::vector<std::u16string>& query_terms,
    size_t max_count) {
  // The highest typed counts should be at the beginning of the `matches` vector
  // so that the best matches will always be included in the results. The loop
  // that calculates match relevance in
  // `HistoryContentsProvider::ConvertResults()` will run backwards to assure
  // higher relevance will be attributed to the best matches.
  std::vector<TitledUrlMatch> matches;
  for (TitledUrlNodes::const_iterator i = nodes.begin();
       i != nodes.end() && matches.size() < max_count; ++i) {
    std::optional<TitledUrlMatch> match =
        MatchNicknameNodeWithQuery(*i, query_nodes, query_terms);
    if (match)
      matches.emplace_back(std::move(match).value());
  }

  return matches;
}

std::vector<TitledUrlMatch> TitledUrlIndex::GetResultsNicknameMatching(
    const std::u16string& input_query,
    size_t max_count) {
  const std::u16string query = Normalize(input_query);
  std::vector<std::u16string> terms = ExtractQueryWords(query);
  if (terms.empty())
    return {};

  // `matches` shouldn't exclude nodes that don't match every query term, as the
  // query terms may match in the ancestors. `MatchTitledUrlNodeWithQuery()`
  // below will filter out nodes that neither match nor ancestor-match every
  // query term.

  TitledUrlNodeSet matches = RetrieveNicknameNodesMatchingAnyTerms(terms);

  if (matches.empty())
    return {};

  TitledUrlNodes sorted_nodes;
  SortMatches(matches, &sorted_nodes);

  // We use a QueryParser to fill in match positions for us. It's not the most
  // efficient way to go about this, but by the time we get here we know what
  // matches and so this shouldn't be performance critical.
  query_parser::QueryNodeVector query_nodes;

  query_parser::QueryParser::ParseQueryNodes(
      query, query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH,
      &query_nodes);

  std::vector<TitledUrlMatch> ret =
      MatchNicknameNodesWithQuery(sorted_nodes, query_nodes, terms, max_count);
  return ret;
}

std::optional<TitledUrlMatch> TitledUrlIndex::MatchNicknameNodeWithQuery(
    const TitledUrlNode* node,
    const query_parser::QueryNodeVector& query_nodes,
    const std::vector<std::u16string>& query_terms) {
  if (!node) {
    return std::nullopt;
  }

  // Check that the result matches the query.  The previous search
  // was a simple per-word search, while the more complex matching
  // of QueryParser may filter it out.  For example, the query
  // ["thi"] will match the title [Thinking], but since
  // ["thi"] is quoted we don't want to do a prefix match.

  // Clean up the title, URL, and ancestor titles in preparation for string
  // comparisons.
  base::OffsetAdjuster::Adjustments adjustments;
  const std::u16string clean_url =
      CleanUpUrlForMatching(node->GetTitledUrlNodeUrl(), &adjustments);

  const std::u16string nickname =
      base::i18n::ToLower(Normalize(node->GetTitledUrlNodeNickName()));

  std::vector<std::u16string> lower_ancestor_titles;
  base::ranges::transform(
      node->GetTitledUrlNodeAncestorTitles(),
      std::back_inserter(lower_ancestor_titles),
      [](const auto& ancestor_title) {
        return base::i18n::ToLower(Normalize(std::u16string(ancestor_title)));
      });

  // Check if the input approximately matches the node. This is less strict than
  // the following check; it will return false positives. But it's also much
  // faster, so if it returns false, early exit and avoid the expensive
  // `ExtractQueryWords()` calls.
  bool approximate_match =
      base::ranges::all_of(query_terms, [&](const auto& word) {
        if (nickname.find(word) != std::u16string::npos)
          return true;

        return false;
      });
  if (!approximate_match)
    return std::nullopt;

  // If `node` passed the approximate check above, to the more accurate check.
  query_parser::QueryWordVector title_words, url_words, ancestor_words;
  query_parser::QueryParser::ExtractQueryWords(clean_url, &url_words);
  for (const auto& ancestor_title : lower_ancestor_titles) {
    query_parser::QueryParser::ExtractQueryWords(ancestor_title,
                                                 &ancestor_words);
  }

  query_parser::QueryWordVector description_words, nickname_words;
  const std::u16string lower_description =
      base::i18n::ToLower(Normalize(node->GetTitledUrlNodeDescription()));
  query_parser::QueryParser::ExtractQueryWords(lower_description,
                                               &description_words);
  const std::u16string lower_nickname =
      base::i18n::ToLower(Normalize(node->GetTitledUrlNodeNickName()));
  query_parser::QueryParser::ExtractQueryWords(lower_nickname, &nickname_words);

  query_parser::Snippet::MatchPositions description_matches, nickname_matches;

  bool query_has_ancestor_matches = false;
  for (const auto& query_node : query_nodes) {
    const bool has_nickname_matches =
        query_node->HasMatchIn(nickname_words, &nickname_matches);
    if (!has_nickname_matches) {
      return std::nullopt;
    }
    query_parser::QueryParser::SortAndCoalesceMatchPositions(&nickname_matches);
  }

  TitledUrlMatch match;
  std::vector<size_t> offsets =
      TitledUrlMatch::OffsetsFromMatchPositions(nickname_matches);
  base::OffsetAdjuster::UnadjustOffsets(adjustments, &offsets);
  nickname_matches =
      TitledUrlMatch::ReplaceOffsetsInMatchPositions(nickname_matches, offsets);
  match.nickname_match_positions.swap(nickname_matches);
  match.has_ancestor_match = query_has_ancestor_matches;
  match.node = node;
  return match;
}
}  // namespace bookmarks
