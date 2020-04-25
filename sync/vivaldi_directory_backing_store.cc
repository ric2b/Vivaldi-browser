// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "components/sync/syncable/directory_backing_store.h"

namespace syncer {
namespace syncable {

bool DirectoryBackingStore::SetVivaldiVersion(int version) {
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE, "UPDATE share_version SET vivaldi_version = ?"));
  s.BindInt(0, version);

  return s.Run();
}

int DirectoryBackingStore::GetVivaldiVersion() {
  if (!db_->DoesTableExist("share_version"))
    return 0;

  if (!db_->DoesColumnExist("share_version", "vivaldi_version"))
    return 0;

  sql::Statement statement(
      db_->GetUniqueStatement("SELECT vivaldi_version FROM share_version"));
  if (statement.Step()) {
    return statement.ColumnInt(0);
  } else {
    return 0;
  }
}

bool DirectoryBackingStore::MigrateVivaldiVersion0To1() {
  // Vivaldi Version 1 adds the vivalid_version to the share_version table.
  if (!db_->Execute("ALTER TABLE share_version ADD COLUMN vivaldi_version int"))
    return false;

  if (!db_->Execute("ALTER TABLE metas ADD COLUMN "
                    "unique_notes_tag VARCHAR")) {
    return false;
  }

  SetVivaldiVersion(1);
  needs_metas_column_refresh_ = true;
  return true;
}

}  // namespace syncable
}  // namespace syncer
