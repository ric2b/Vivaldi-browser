// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RECENT_TYPED_HISTORY_PROVIDER_H_
#define RECENT_TYPED_HISTORY_PROVIDER_H_

#include <string>

#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "sql/statement.h"
#include "url/gurl.h"

class AutocompleteProviderClient;

struct RecentTypedHistoryItem {
  RecentTypedHistoryItem();
  RecentTypedHistoryItem& operator=(const RecentTypedHistoryItem&);
  RecentTypedHistoryItem(const RecentTypedHistoryItem&);

  GURL url;
  std::u16string contents;
};

class RecentTypedHistoryProvider : public AutocompleteProvider {
 public:
  explicit RecentTypedHistoryProvider(AutocompleteProviderClient* client);

  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~RecentTypedHistoryProvider() override;
  void QueryRecentTypedHistory(const AutocompleteInput& input);
  void QueryRecentTypedSearch(const AutocompleteInput& input);
  void OnGetRecentTypedHistoryOrSearchDone(sql::Statement&& statement);

  const raw_ptr<AutocompleteProviderClient> client_;

  // The maximum number of matches to return.
  const size_t max_matches_;
};

#endif //RECENT_TYPED_HISTORY_PROVIDER_H_
