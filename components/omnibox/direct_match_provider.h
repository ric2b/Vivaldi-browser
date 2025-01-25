// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_DIRECT_MATCH_PROVIDER_H_
#define COMPONENTS_OMNIBOX_DIRECT_MATCH_PROVIDER_H_

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/direct_match/direct_match_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;

namespace direct_match {
class DirectMatchService;
}

// This class is an autocomplete provider which quickly (and synchronously)
// provides autocomplete suggestions based on direct match.
//

class DirectMatchProvider : public AutocompleteProvider {
 public:
  explicit DirectMatchProvider(AutocompleteProviderClient* client);

  DirectMatchProvider(const DirectMatchProvider&) = delete;
  DirectMatchProvider& operator=(const DirectMatchProvider&) = delete;

  // When |minimal_changes| is true short circuit any additional searching and
  // leave the previous matches for this provider unchanged, otherwise perform
  // a complete search for |input| across all direct match
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~DirectMatchProvider() override;

  // Performs the actual matching of |input| over the direct match and fills in
  // |matches_|.
  void StartDirectMatchSearch(const AutocompleteInput& input);

  // Calculates the relevance score for |match|.
  int CalculateDirectMatchRelevance(
      const direct_match::DirectMatchService::DirectMatchUnit& match,
      query_parser::Snippet::MatchPositions* match_positions,
      bool dm_boost) const;

  const raw_ptr<AutocompleteProviderClient> client_;
  const raw_ptr<direct_match::DirectMatchService> direct_match_service_;
};

#endif  // COMPONENTS_OMNIBOX_DIRECT_MATCH_PROVIDER_H_
