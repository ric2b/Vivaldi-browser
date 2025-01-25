// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "components/search_engines/keyword_table.h"

#include "components/webdata/common/web_database.h"

bool KeywordTable::MigrateToVivaldiVersion(int version) {
  switch (version) {
    case 1:
      return MigrateToVivaldiVersion1AddPositionColumn();
  }

  return true;
}

bool KeywordTable::MigrateToVivaldiVersion1AddPositionColumn () {
  if (!db()->DoesTableExist("keywords"))
    return true;
  return db()->Execute(
      "ALTER TABLE keywords ADD COLUMN position VARCHAR");
}