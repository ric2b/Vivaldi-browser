// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_HISTORY_HISTORY_SEARCH_API_H
#define EXTENSIONS_HISTORY_HISTORY_SEARCH_API_H

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/api/history/history_api.h"

namespace history {
class QueryResults;
}

namespace extensions {

class HistorySearchDbSearchFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("historySearch.dbSearch", HISTORY_DBSEARCH)

 protected:
  ~HistorySearchDbSearchFunction() override {}

  // HistoryFunctionWithCallback:
  bool RunAsyncImpl() override;

  // Callback for the history function to provide results.
  void SearchComplete(history::QueryResults* results);
};

} // namespace extensions

#endif // EXTENSIONS_HISTORY_HISTORY_SEARCH_API_H
