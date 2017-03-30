// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_HISTORY_HISTORY_PRIVATE_API_H_
#define EXTENSIONS_API_HISTORY_HISTORY_PRIVATE_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/api/history/history_api.h"

namespace history {
class QueryResults;
}

namespace extensions {

class HistoryPrivateDbSearchFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("historyPrivate.dbSearch", HISTORYPRIVATE_DBSEARCH)

 protected:
  ~HistoryPrivateDbSearchFunction() override {}

  // HistoryFunctionWithCallback:
  bool RunAsyncImpl() override;

  // Callback for the history function to provide results.
  void SearchComplete(history::QueryResults* results);
};

class HistoryPrivateSearchFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("historyPrivate.search", HISTORYPRIVATE_SEARCH)

 protected:
  ~HistoryPrivateSearchFunction() override {}

  // HistoryFunctionWithCallback:
  bool RunAsyncImpl() override;

  // Callback for the history function to provide results.
  void SearchComplete(history::QueryResults* results);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_HISTORY_HISTORY_PRIVATE_API_H_
