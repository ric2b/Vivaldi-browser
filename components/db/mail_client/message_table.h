// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DB_MAIL_CLIENT_MESSAGE_TABLE_H_
#define COMPONENTS_DB_MAIL_CLIENT_MESSAGE_TABLE_H_

#include <stddef.h>

#include "message_type.h"
#include "sql/statement.h"

namespace sql {
class Database;
}

namespace mail_client {

// Encapsulates an SQL table that holds Messages info.
//
class MessageTable {
 public:
  // Must call CreateMessageTable() before to make sure the database is
  // initialized.
  MessageTable();
  MessageTable(const MessageTable&) = delete;
  MessageTable& operator=(const MessageTable&) = delete;

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~MessageTable();

  bool CreateMessageTable();
  bool CreateMessages(std::vector<MessageRow> messages);
  bool SearchMessages(std::u16string search, SearchListIDs* out_ids);
  bool MatchMessage(const SearchListID& search_list_id, std::u16string search);
  bool UpdateMessage(MessageRow row);
  bool DeleteMessages(SearchListIDs ids);
  int CountRows(std::string table);
  bool UpdateToVersion2();
  bool UpdateToVersion3();
  bool CreateMigrationTable();
  bool InsertIntoMigrationTable(int limit, int offset);
  int SelectMaxOffsetFromMigration();
  bool DoesTableExist(std::string name);
  bool CopyMessagesToContentless(int limit, int offs);
  bool AttachDBForMigrate(base::FilePath db_dir);
  bool DetachDBAfterMigrate();
  bool DoesAttachedMessageTableExists();

 protected:
  virtual sql::Database& GetDB() = 0;
};

}  // namespace mail_client

#endif  // COMPONENTS_DB_MAIL_CLIENT_MESSAGE_TABLE_H_
