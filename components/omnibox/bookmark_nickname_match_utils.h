// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BOOKMARK_NICKNAME_MATCH_UTILS_H_
#define COMPONENTS_OMNIBOX_BOOKMARK_NICKNAME_MATCH_UTILS_H_

#include <string>

#include "components/bookmarks/browser/titled_url_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"

class AutocompleteInput;
class AutocompleteProvider;
class AutocompleteSchemeClassifier;
struct AutocompleteMatch;

namespace bookmarks {

// Compose an AutocompleteMatch based on |match| that has the match's nickname
// title, type |type|, and relevance score |relevance|. |input| is used to
// compute the match's inline_autocompletion. |fixed_up_input_text| is used in
// that way as well; it's passed separately so this function doesn't have to
// compute it.
AutocompleteMatch NicknameMatchToAutocompleteMatch(
    const TitledUrlMatch& match,
    AutocompleteMatchType::Type type,
    int relevance,
    int bookmark_count,
    AutocompleteProvider* provider,
    const AutocompleteSchemeClassifier& scheme_classifier,
    const AutocompleteInput& input,
    const std::u16string& fixed_up_input_text);

}  // namespace bookmarks

#endif  // COMPONENTS_OMNIBOX_BOOKMARK_NICKNAME_MATCH_UTILS_H_
