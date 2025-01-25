// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/invite_table.h"

#include <string>
#include <vector>

#include "app/vivaldi_resources.h"
#include "base/strings/utf_string_conversions.h"
#include "calendar/invite_type.h"
#include "sql/statement.h"
#include "ui/base/l10n/l10n_util.h"

namespace calendar {

InviteTable::InviteTable() {}

InviteTable::~InviteTable() {}

bool InviteTable::CreateInviteTable() {
  const char* name = "invite";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "event_id INTEGER NOT NULL,"
      "name LONGVARCHAR,"
      "address LONGVARCHAR,"
      "sent INTEGER DEFAULT 0,"
      "partstat LONGVARCHAR,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  bool res = GetDB().Execute(sql);

  return res;
}

InviteID InviteTable::CreateInvite(InviteRow row) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO invite "
                                 "(event_id, name, address, sent, partstat, "
                                 "created, last_modified) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, row.event_id);
  statement.BindString16(1, row.name);
  statement.BindString16(2, row.address);
  statement.BindInt(3, row.sent ? 1 : 0);
  statement.BindString(4, row.partstat);
  statement.BindInt64(5, base::Time().Now().ToInternalValue());
  statement.BindInt64(6, base::Time().Now().ToInternalValue());

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool InviteTable::GetInvitesForEvent(EventID event_id, InviteRows* invites) {
  invites->clear();
  sql::Statement s(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                              "SELECT" INVITE_ROW_FIELDS
                                              " FROM invite WHERE event_id=?"));

  s.BindInt64(0, event_id);

  while (s.Step()) {
    InviteRow invite;
    FillInviteRow(s, &invite);
    invites->push_back(invite);
  }

  return true;
}

bool InviteTable::UpdateInvite(const InviteRow& invite) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                                      "UPDATE invite SET \
        event_id=?, name=?, address=?, sent=?, \
        partstat=? \
        WHERE id=?"));
  statement.BindInt64(0, invite.event_id);
  statement.BindString16(1, invite.name);
  statement.BindString16(2, invite.address);
  statement.BindInt(3, invite.sent ? 1 : 0);
  statement.BindString(4, invite.partstat);
  statement.BindInt64(5, invite.id);

  return statement.Run();
}

bool InviteTable::DeleteInvite(InviteID invite_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from invite WHERE id=?"));
  statement.BindInt64(0, invite_id);

  return statement.Run();
}

bool InviteTable::DeleteInvitesForCalendar(CalendarID calendar_id) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                                      "DELETE from invite \
        WHERE event_id IN( \
          select i.event_id from invite i \
            inner join events e on(e.id = i.event_id) \
            where e.calendar_id = ?)"));

  statement.BindInt64(0, calendar_id);

  return statement.Run();
}

bool InviteTable::GetInviteRow(InviteID invite_id, InviteRow* out_invite) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" INVITE_ROW_FIELDS "FROM invite WHERE id=?"));
  statement.BindInt64(0, invite_id);

  if (!statement.Step())
    return false;

  FillInviteRow(statement, out_invite);

  return true;
}

void InviteTable::FillInviteRow(sql::Statement& statement, InviteRow* invite) {
  InviteID invite_id = statement.ColumnInt64(0);
  EventID event_id = statement.ColumnInt64(1);
  std::u16string name = statement.ColumnString16(2);
  std::u16string address = statement.ColumnString16(3);
  bool sent = statement.ColumnInt(4) != 0;
  std::string partstat = statement.ColumnString(5);

  invite->id = invite_id;
  invite->event_id = event_id;
  invite->name = name;
  invite->address = address;
  invite->sent = sent;
  invite->partstat = partstat;
}

bool InviteTable::DoesInviteIdExist(InviteID invite_id) {
  sql::Statement statement(
      GetDB().GetUniqueStatement("select count(*) as count from invite \
        WHERE id=?"));
  statement.BindInt64(0, invite_id);

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) == 1;
}

// Updates to version 5. Adds columns partstat to events
bool InviteTable::MigrateCalendarToVersion5() {
  if (!GetDB().DoesTableExist("events") && !GetDB().DoesTableExist("invite")) {
    NOTREACHED() << "invite table should exist before migration";
    //return false;
  }

  if (!GetDB().DoesColumnExist("events", "organizer")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN organizer LONGVARCHAR"))
      return false;
  }

  if (!GetDB().DoesColumnExist("invite", "partstat")) {
    if (!GetDB().Execute("ALTER TABLE invite "
                         "ADD COLUMN partstat LONGVARCHAR"))
      return false;
  }
  return true;
}

}  // namespace calendar
