// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "message_table.h"

#include <string>
#include <vector>
#include "app/vivaldi_resources.h"
#include "base/strings/utf_string_conversions.h"
#include "message_type.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/base/l10n/l10n_util.h"

namespace mail_client {

MessageTable::MessageTable() {}

MessageTable::~MessageTable() {}

bool MessageTable::CreateMessageTable() {
  const char* name = "messages";

  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "searchListId INTEGER PRIMARY KEY,"
      "toAddress TEXT NOT NULL,"
      "fromAddress TEXT,"
      "cc TEXT,"
      "replyTo TEXT,"
      "subject TEXT,"
      "body TEXT)");

  bool emailTable = GetDB().Execute(sql.c_str());

  bool indexTable = GetDB().Execute(
      "CREATE VIRTUAL TABLE messages_fts USING fts5(searchListId, toAddress, "
      " fromAddress, cc, replyTo, subject, body, content = messages, "
      " content_rowid = searchListId, tokenize = trigram);");

  bool insertTrigger = GetDB().Execute(
      "CREATE TRIGGER messages_ai AFTER INSERT ON messages "
      " BEGIN "
      " INSERT INTO messages_fts(rowid, toAddress, fromAddress, cc, replyTo, "
      "subject, body) "
      " VALUES(new.searchListId, new.toAddress, new.fromAddress, new.cc, "
      " new.replyTo, new.subject, new.body); "
      " END;");

  bool deleteTrigger = GetDB().Execute(MESSAGES_TRIGGER_AFTER_DELETE);

  bool updateTrigger = GetDB().Execute(MESSAGES_TRIGGER_AFTER_UPDATE);

  return emailTable && indexTable && insertTrigger && deleteTrigger &&
         updateTrigger;
}

bool MessageTable::CreateMessages(
    std::vector<mail_client::MessageRow> messages) {
  sql::Transaction transaction(&GetDB());
  if (!transaction.Begin())
    return false;

  const char kCreateMessage[] =
      "INSERT OR REPLACE INTO messages "
      "(searchListId, toAddress, fromAddress, cc, replyTo, "
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
                                  SearchListIdRows* out_rows) {
  sql::Statement statement(
      GetDB().GetUniqueStatement("SELECT searchListId FROM "
                                 "messages_fts where messages_fts MATCH ?"));

  statement.BindString16(0, search);

  while (statement.Step()) {
    SearchListID searchListId = statement.ColumnInt64(0);
    out_rows->push_back(searchListId);
  }
  return true;
}

bool MessageTable::MatchMessage(const SearchListID& search_list_id,
                                std::u16string search) {
  sql::Statement statement(
      GetDB().GetUniqueStatement("SELECT count(*) FROM messages_fts where "
                                 "messages_fts MATCH ? and searchListId=?"));

  statement.BindString16(0, search);
  statement.BindInt64(1, search_list_id);

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) == 1;
}

bool MessageTable::AddMessageBody(const SearchListID& search_list_id,
                                  std::u16string body) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "UPDATE messages SET body=? WHERE searchListId=?"));

  statement.BindString16(0, body);
  statement.BindInt64(1, search_list_id);
  return statement.Run();
}

bool MessageTable::DeleteMessages(std::vector<SearchListID> search_list_ids) {
  std::string sql;
  sql.append("DELETE FROM messages ");
  std::ostringstream slids;
  bool has_id = false;
  for (std::vector<SearchListID>::const_iterator i = search_list_ids.begin();
       i != search_list_ids.end(); ++i) {
    if (has_id)
      slids << ", ";
    else
      has_id = true;
    slids << *i;
  }

  if (has_id) {
    sql.append(" WHERE searchListId in ( ");
    sql.append(slids.str());
    sql.append(" )");
  }

  if (!GetDB().Execute(sql.c_str())) {
    LOG(ERROR) << GetDB().GetErrorMessage();
    return false;
  }
  return true;
}

bool MessageTable::RebuildDatabase() {
  return GetDB().Execute(
      "INSERT INTO messages_fts(messages_fts) VALUES('rebuild');");
}

bool MessageTable::UpdateToVersion2() {
  if (!GetDB().DoesTableExist("messages")) {
    NOTREACHED() << "messages table should exist before migration";
    return false;
  }

  if (!GetDB().Execute("DROP TRIGGER messages_au"))
    return false;

  bool updateTrigger = GetDB().Execute(MESSAGES_TRIGGER_AFTER_UPDATE);

  if (!GetDB().Execute("DROP TRIGGER messages_ad"))
    return false;

  bool deleteTrigger = GetDB().Execute(MESSAGES_TRIGGER_AFTER_DELETE);

  if (!updateTrigger || !deleteTrigger)
    return false;

  return RebuildDatabase();
}

}  // namespace mail_client
