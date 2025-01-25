// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/bookmark_nickname_match_utils.h"

#include <numeric>
#include <string_view>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/titled_url_node.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/query_parser/snippet.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace bookmarks {

AutocompleteMatch NicknameMatchToAutocompleteMatch(
    const TitledUrlMatch& titled_url_match,
    AutocompleteMatchType::Type type,
    int relevance,
    int bookmark_count,
    AutocompleteProvider* provider,
    const AutocompleteSchemeClassifier& scheme_classifier,
    const AutocompleteInput& input,
    const std::u16string& fixed_up_input_text) {
  const std::u16string title = titled_url_match.node->GetTitledUrlNodeTitle();
  const std::u16string nickname =
      titled_url_match.node->GetTitledUrlNodeNickName();
  const GURL& url = titled_url_match.node->GetTitledUrlNodeUrl();

  // The AutocompleteMatch we construct is non-deletable because the only way to
  // support this would be to delete the underlying object that created the
  // titled_url_match. E.g., for the bookmark provider this would mean deleting
  // the underlying bookmark, which is unlikely to be what the user intends.
  AutocompleteMatch match(provider, relevance, false, type);
  match.destination_url = url;
  match.RecordAdditionalInfo("Title", title);
  match.RecordAdditionalInfo("URL", url.spec());
  match.RecordAdditionalInfo("Nickname", nickname);

  bool match_in_scheme = false;
  bool match_in_subdomain = false;
  AutocompleteMatch::GetMatchComponents(
      url, titled_url_match.nickname_match_positions, &match_in_scheme,
      &match_in_subdomain);
  auto format_types = AutocompleteMatch::GetFormatTypes(
      input.parts().scheme.is_nonempty() || match_in_scheme,
      match_in_subdomain);
  const std::u16string formatted_url = url_formatter::FormatUrl(
      url, format_types, base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);

  // Display the nickname.
  match.contents = formatted_url;

  // Bookmark classification diverges from relevance scoring. Specifically,
  // 1) All occurrences of the input contribute to relevance; e.g. for the input
  // 'pre', the bookmark 'pre prefix' will be scored higher than 'pre suffix'.
  // For classification though, if the input is a prefix of the suggestion text,
  // only the prefix will be bolded; e.g. the 1st bookmark will display '[pre]
  // prefix' as opposed to '[pre] [pre]fix'. This divergence allows consistency
  // with other providers' and google.com's bolding.
  // 2) Non-complete-word matches less than 3 characters long do not contribute
  // to relevance; e.g. for the input 'a pr', the bookmark 'a pr prefix' will be
  // scored the same as 'a pr suffix'. For classification though, both
  // occurrences will be bolded, 'a [pr] [pr]efix'.
  auto contents_terms = FindTermMatches(input.text(), match.contents);
  match.contents_class = ClassifyTermMatches(
      contents_terms, match.contents.length(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  match.description = nickname;

  base::TrimWhitespace(match.description, base::TRIM_LEADING,
                       &match.description);
  auto description_terms = FindTermMatches(input.text(), match.description);
  match.description_class = ClassifyTermMatches(
      description_terms, match.description.length(),
      ACMatchClassification::MATCH, ACMatchClassification::NONE);

  size_t inline_autocomplete_offset = URLPrefix::GetInlineAutocompleteOffset(
      input.text(), fixed_up_input_text, false, nickname);
  match.fill_into_edit = nickname;

  if (match.TryRichAutocompletion(match.contents, match.description, input)) {
    // If rich autocompletion applies, we skip trying the alternatives below.
  } else if (inline_autocomplete_offset != std::u16string::npos) {
    match.inline_autocompletion =
        match.fill_into_edit.substr(inline_autocomplete_offset);
    match.SetAllowedToBeDefault(input);
  }

  return match;
}

}  // namespace bookmarks
