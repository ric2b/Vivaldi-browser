// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/recent_typed_history_provider.h"

#include "base/functional/bind.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/search_engines/template_url_service.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

using metrics::OmniboxInputType;

// RecentTypedHistoryItem ------------------------------------------------------
RecentTypedHistoryItem::RecentTypedHistoryItem() {}
RecentTypedHistoryItem& RecentTypedHistoryItem::operator=(
    const RecentTypedHistoryItem&) = default;
RecentTypedHistoryItem::RecentTypedHistoryItem(
    const RecentTypedHistoryItem&) = default;

// RecentTypedHistoryProvider --------------------------------------------------
AutocompleteMatch RecentTypedHistoryToAutocompleteMatch(
    AutocompleteProvider* provider,
    RecentTypedHistoryItem item,
    int relevance) {
  std::u16string empty_string;
  std::u16string url_to_contents = url_formatter::FormatUrl(
      item.url,
      AutocompleteMatch::GetFormatTypes(!item.url.SchemeIsHTTPOrHTTPS(), true),
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);

  AutocompleteMatch match(provider, relevance, true,
                          AutocompleteMatchType::RECENT_TYPED_HISTORY);
  match.destination_url = item.url;
  match.contents = item.contents.empty() ? url_to_contents : item.contents;
  match.transition = ui::PAGE_TRANSITION_TYPED;
  auto contents_terms = FindTermMatches(empty_string, match.contents);
  match.contents_class = ClassifyTermMatches(
      contents_terms, match.contents.length(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);
  match.fill_into_edit = match.contents;
  match.inline_autocompletion = empty_string;
  match.allowed_to_be_default_match = true;
  match.suggestion_group_id = omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST;
  return match;
}

RecentTypedHistoryProvider::RecentTypedHistoryProvider(
    AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_RECENT_TYPED_HISTORY),
      client_(client) {}

RecentTypedHistoryProvider::~RecentTypedHistoryProvider() = default;

void RecentTypedHistoryProvider::Start(const AutocompleteInput& input,
                                       bool minimal_changes) {
  TRACE_EVENT0("omnibox", "RecentTypedHistoryProvider::Start");
  Stop(true, false);

  if (input.focus_type() != metrics::OmniboxFocusType::INTERACTION_FOCUS ||
         input.type() != OmniboxInputType::EMPTY) {
    return;
  }
  if (input.from_search_field) {
    QueryRecentTypedSearch(input);
  } else {
    QueryRecentTypedHistory(input);
  }
}

void RecentTypedHistoryProvider::DeleteMatch(const AutocompleteMatch& match) {
  GURL url = match.destination_url;
  history::HistoryService* const history_service = client_->GetHistoryService();
  history::WebHistoryService* web_history = client_->GetWebHistoryService();
  history_service->DeleteLocalAndRemoteUrl(web_history, url);

  // Immediately update the list of matches to reflect the match was deleted.
  std::erase_if(matches_, [&](const auto& item) {
    return match.destination_url == item.destination_url;
  });
}

void RecentTypedHistoryProvider::QueryRecentTypedHistory(
      const AutocompleteInput& input) {
  done_ = true;
  matches_.clear();

  history::HistoryService* const history_service = client_->GetHistoryService();
  if (!history_service)
    return;
  // Fail if the in-memory URL database is not available.
  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (!url_db)
    return;

  const bool show_search_queries = client_->GetPrefs()->GetBoolean(
    vivaldiprefs::kAddressBarOmniboxShowSearchHistory);

  if (show_search_queries) {
    url_db->GetRecentTypedHistoryItems(
        base::BindOnce(
            &RecentTypedHistoryProvider::OnGetRecentTypedHistoryOrSearchDone,
            base::Unretained(this)),
        provider_max_matches_);
  } else {
    url_db->GetRecentTypedUrlItems(
        base::BindOnce(
            &RecentTypedHistoryProvider::OnGetRecentTypedHistoryOrSearchDone,
            base::Unretained(this)),
        provider_max_matches_);
  }
}

void RecentTypedHistoryProvider::QueryRecentTypedSearch(
      const AutocompleteInput& input) {
  done_ = true;
  matches_.clear();

  history::HistoryService* const history_service = client_->GetHistoryService();
  if (!history_service)
    return;
  // Fail if the in-memory URL database is not available.
  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (!url_db)
    return;

  // Fail if we can't set the clickthrough URL for query suggestions.
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  if (!template_url_service ||
      !template_url_service->GetDefaultSearchProvider()) {
    return;
  }

  url_db->GetRecentTypedSearchItems(
      base::BindOnce(
          &RecentTypedHistoryProvider::OnGetRecentTypedHistoryOrSearchDone,
          base::Unretained(this)),
      provider_max_matches_,
      template_url_service->GetTemplateURLForGUID(input.search_engine_guid)
          ->id());
}

void RecentTypedHistoryProvider::OnGetRecentTypedHistoryOrSearchDone(
    sql::Statement&& statement) {
  std::vector<RecentTypedHistoryItem> items;
  if (!statement.is_valid())
    return;
  while (statement.Step()) {
    RecentTypedHistoryItem item;
    item.contents = statement.ColumnString16(0);
    item.url = GURL(statement.ColumnString(1));
    items.push_back(item);
  }
  DCHECK_LE(items.size(), provider_max_matches_);
  int relevance = 100;
  for (const auto& item : items) {
    matches_.push_back(
        RecentTypedHistoryToAutocompleteMatch(this, item, relevance--));
  }
}
