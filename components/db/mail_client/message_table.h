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
  bool CreateMessages(std::vector<mail_client::MessageRow> messages);
  bool SearchMessages(std::u16string search, SearchListIdRows* out_emails);
  bool MatchMessage(const SearchListID& search_list_id, std::u16string search);
  bool AddMessageBody(const SearchListID& sld, std::u16string body);
  bool DeleteMessages(std::vector<SearchListID> search_list_ids);
  bool RebuildDatabase();
  bool UpdateToVersion2();

 protected:
  virtual sql::Database& GetDB() = 0;
};

static const char MESSAGES_TRIGGER_AFTER_DELETE[] =
    " CREATE TRIGGER messages_ad AFTER DELETE ON messages "
    " BEGIN "
    " INSERT INTO messages_fts(messages_fts, rowid, "
    "   toAddress, fromAddress, cc, replyTo, subject, body) "
    " VALUES('delete', old.searchListId, "
    "   old.toAddress, old.fromAddress, old.cc, old.replyTo, old.subject, "
    "   old.body); "
    " END;";

static const char MESSAGES_TRIGGER_AFTER_UPDATE[] =
    " CREATE TRIGGER messages_au AFTER UPDATE ON messages "
    " BEGIN "
    " INSERT INTO messages_fts(messages_fts, rowid,  "
    "   toAddress, fromAddress, cc, replyTo, subject, body) "
    " VALUES('delete', old.searchListId, "
    "   old.toAddress, old.fromAddress, old.cc, old.replyTo, old.subject, "
    "   old.body); "
    " INSERT INTO messages_fts(rowid, toAddress, fromAddress, cc, replyTo, "
    "   subject, body) "
    " VALUES(new.searchListId, new.toAddress, new.fromAddress, new.cc, "
    "   new.replyTo, new.subject, new.body); "
    " END; ";

}  // namespace mail_client

#endif  // COMPONENTS_DB_MAIL_CLIENT_MESSAGE_TABLE_H_
