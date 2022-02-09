// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/history/core/browser/visit_database.h"

#include "sql/statement.h"
#include "sql/transaction.h"

namespace history {

bool VisitDatabase::DropHistoryTables() {
  // This will also drop the indices over the table.
  return GetDB().Execute("DROP TABLE visits") &&
         GetDB().Execute("DROP TABLE urls");
}

}  // namespace history
