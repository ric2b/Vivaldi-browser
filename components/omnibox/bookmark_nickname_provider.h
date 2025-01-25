// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BOOKMARK_NICKNAME_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BOOKMARK_NICKNAME_PROVIDER_H_

#include <stddef.h>

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;

namespace bookmarks {
class BookmarkModel;
struct TitledUrlMatch;
}  // namespace bookmarks

// This class is an autocomplete provider which quickly (and synchronously)
// provides autocomplete suggestions based on bookmark nickname.
//

class BookmarkNicknameProvider : public AutocompleteProvider {
 public:
  explicit BookmarkNicknameProvider(AutocompleteProviderClient* client);

  BookmarkNicknameProvider(const BookmarkNicknameProvider&) = delete;
  BookmarkNicknameProvider& operator=(const BookmarkNicknameProvider&) = delete;

  // When |minimal_changes| is true short circuit any additional searching and
  // leave the previous matches for this provider unchanged, otherwise perform
  // a complete search for |input| across all bookmark nicknames.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~BookmarkNicknameProvider() override;

  // Performs the actual matching of |input| over the bookmarks and fills in
  // |matches_|.
  void DoAutocomplete(const AutocompleteInput& input);

  // Calculates the relevance score for |match|.
  // Also returns the number of bookmarks containing the destination URL.
  std::pair<int, int> CalculateBookmarkMatchRelevance(
      const bookmarks::TitledUrlMatch& match) const;

  const raw_ptr<AutocompleteProviderClient> client_;
  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

#endif  // COMPONENTS_OMNIBOX_BOOKMARK_NICKNAME_PROVIDER_H_
