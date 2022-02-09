// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef DB_VIVALDI_HISTORY_DATABASE_H_
#define DB_VIVALDI_HISTORY_DATABASE_H_

#include "components/history/core/browser/history_types.h"
#include "db/vivaldi_history_types.h"

namespace sql {
class Database;
}

namespace history {

class VivaldiHistoryDatabase {
 public:
  VivaldiHistoryDatabase();
  virtual ~VivaldiHistoryDatabase();
  VivaldiHistoryDatabase(const VivaldiHistoryDatabase&) = delete;
  VivaldiHistoryDatabase& operator=(const VivaldiHistoryDatabase&) = delete;

  UrlVisitCount::TopUrlsPerDayList TopUrlsPerDay(size_t num_hosts);

  Visit::VisitsList VisitSearch(const QueryOptions& options);

 private:
  // Returns the database for the functions in this interface. The decendent of
  // this class implements these functions to return its objects.
  virtual sql::Database& GetDB() = 0;
};

}  // namespace history

#endif  // DB_VIVALDI_HISTORY_DATABASE_H_
