// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "message_table.h"

#include <string>
#include <string_view>
#include <vector>

#include "app/vivaldi_resources.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "message_type.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/base/l10n/l10n_util.h"

namespace mail_client {

MessageTable::MessageTable() {}

MessageTable::~MessageTable() {}

bool MessageTable::CreateMessageTable() {
  const char* name = "messages_search_fts";

  if (GetDB().DoesTableExist(name))
    return true;

  return GetDB().Execute(
      "CREATE VIRTUAL TABLE messages_search_fts USING fts5(searchListId, "
      "toAddress, "
      " fromAddress, cc, replyTo, subject, body, content='', "
      " tokenize = trigram, contentless_delete=1);");
}

bool MessageTable::CreateMessages(
    std::vector<mail_client::MessageRow> messages) {
  sql::Transaction transaction(&GetDB());
  if (!transaction.Begin())
    return false;

  const char kCreateMessage[] =
      "INSERT INTO messages_search_fts "
      "(rowid, toAddress, fromAddress, cc, replyTo, "
      "subject, body) "
      "VALUES (?, ?, ?, ?, ?, ?, ?)";
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE, kCreateMessage));
  for (const auto& row : messages) {
    int column_index = 0;
    statement.BindInt64(column_index++, row.searchListId);
    statement.BindString16(column_index++, row.to);
    statement.BindString16(column_index++, row.from);
    statement.BindString16(column_index++, row.cc);
    statement.BindString16(column_index++, row.replyTo);
    statement.BindString16(column_index++, row.subject);
    statement.BindString16(column_index++, row.body);

    if (!statement.Run())
      return false;

    statement.Reset(true);
  }

  return transaction.Commit();
}

bool MessageTable::SearchMessages(std::u16string search,
                                  SearchListIDs* out_ids) {
  sql::Statement statement(GetDB().GetUniqueStatement(
      "SELECT rowid FROM "
      "messages_search_fts where messages_search_fts MATCH ?"));

  statement.BindString16(0, search);

  while (statement.Step()) {
    SearchListID searchListId = statement.ColumnInt64(0);
    out_ids->push_back(searchListId);
  }
  return true;
}

bool MessageTable::MatchMessage(const SearchListID& search_list_id,
                                std::u16string search) {
  sql::Statement statement(GetDB().GetUniqueStatement(
      "SELECT count(*) FROM messages_search_fts where "
      "messages_search_fts MATCH ? and rowid=?"));

  statement.BindString16(0, search);
  statement.BindInt64(1, search_list_id);

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) == 1;
}

bool MessageTable::UpdateMessage(mail_client::MessageRow row) {
  const char kUpdateMessage[] =
      "UPDATE messages_search_fts SET "
      "searchListId = ?, toAddress = ?, fromAddress = ?, cc = ?, replyTo = ?, "
      "subject = ?, body = ? where rowid = ?";
  sql::Statement statement(GetDB().GetUniqueStatement(kUpdateMessage));

  int column_index = 0;
  statement.BindInt64(column_index++, row.searchListId);
  statement.BindString16(column_index++, row.to);
  statement.BindString16(column_index++, row.from);
  statement.BindString16(column_index++, row.cc);
  statement.BindString16(column_index++, row.replyTo);
  statement.BindString16(column_index++, row.subject);
  statement.BindString16(column_index++, row.body);
  statement.BindInt64(column_index++, row.searchListId);

  return statement.Run();
}

bool MessageTable::DeleteMessages(SearchListIDs search_list_ids) {
  std::string sql = "DELETE from messages_search_fts WHERE rowid IN (";
  for (const auto& id : search_list_ids) {
    sql.append(base::NumberToString(id));
    if (&id != &search_list_ids.back()) {
      sql.push_back(',');
    }
  }
  sql.append(")");
  sql::Statement statement(GetDB().GetUniqueStatement(sql));
  return statement.Run();
}

bool MessageTable::UpdateToVersion2() {
  if (!GetDB().DoesTableExist("messages")) {
    NOTREACHED() << "messages table should exist before migration";
    //return false;
  }

  if (!GetDB().Execute("DROP TRIGGER messages_au"))
    return false;

  if (!GetDB().Execute("DROP TRIGGER messages_ad"))
    return false;

  return true;
}

bool MessageTable::UpdateToVersion3() {
  return CreateMessageTable();
}

// Note. DB must have been attached to the "old" MailDB using the logical
// database name: old
bool MessageTable::CopyMessagesToContentless(int limit, int offs) {
  sql::Transaction transaction(&GetDB());
  if (!transaction.Begin())
    return false;

  sql::Statement statement(GetDB().GetUniqueStatement(
      "INSERT INTO messages_search_fts(rowid, toAddress, fromAddress, "
      " cc, replyTo,  subject, body) "
      " select searchListId, toAddress, fromAddress, "
      " cc, replyTo, subject, body from old.messages  limit ? offset ? "));
  statement.BindInt(0, limit);
  statement.BindInt(1, offs);

  bool insert = statement.Run();

  if (!insert) {
    return false;
  }

  InsertIntoMigrationTable(limit, offs);

  return transaction.Commit();
}

bool MessageTable::DoesAttachedMessageTableExists() {
  std::string sql_str =
      "SELECT 1 FROM old.sqlite_master WHERE type='table' AND name= 'messages'";

  sql::Statement statement(GetDB().GetUniqueStatement(sql_str));

  return statement.Step();
}

int MessageTable::CountRows(std::string table) {
  std::string sql_str = "SELECT COUNT(rowid) FROM " + table;

  sql::Statement statement(GetDB().GetUniqueStatement(sql_str));

  if (!statement.Step())
    return -1;
  return statement.ColumnInt(0);
}

bool MessageTable::DoesTableExist(std::string name) {
  return GetDB().DoesTableExist(name);
}

bool MessageTable::CreateMigrationTable() {
  return GetDB().Execute(
      "CREATE TABLE IF NOT EXISTS migrationLogger (lim INTEGER, offs "
      "INTEGER);");
}

bool MessageTable::InsertIntoMigrationTable(int limit, int offset) {
  sql::Statement statement(
      GetDB().GetUniqueStatement("INSERT INTO migrationLogger(lim, offs) "
                                 " values(?, ?)"));
  statement.BindInt(0, limit);
  statement.BindInt(1, offset);
  return statement.Run();
}

int MessageTable::SelectMaxOffsetFromMigration() {
  std::string sql_str = "SELECT max(offs) FROM migrationLogger;";

  sql::Statement statement(GetDB().GetUniqueStatement(sql_str));

  if (!statement.Step())
    return 0;
  int max_offset = statement.ColumnInt(0);

  if (!max_offset) {
    return 0;
  }

  return max_offset;
}

bool MessageTable::DetachDBAfterMigrate() {
  sql::Statement detach_statement(GetDB().GetUniqueStatement("DETACH old"));
  return detach_statement.Run();
}

bool MessageTable::AttachDBForMigrate(base::FilePath db_dir) {
  base::FilePath old_db = db_dir.Append(FILE_PATH_LITERAL("MailDB"));

  sql::Statement statement(GetDB().GetUniqueStatement("ATTACH ? AS old"));
#if BUILDFLAG(IS_WIN)
  statement.BindString16(0, old_db.AsUTF16Unsafe());
#else
  statement.BindString(0, old_db.value());
#endif

  return statement.Run();
}
}  // namespace mail_client
