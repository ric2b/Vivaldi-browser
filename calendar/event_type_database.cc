// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/event_type_database.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "calendar/calendar_type.h"
#include "calendar/event_type.h"
#include "sql/statement.h"

namespace calendar {

bool EventTypeDatabase::CreateEventTypeTable() {
  const char* name = "event_type";
  if (GetDB().DoesTableExist(name))
    return true;

  // Note: revise implementation for InsertOrUpdateURLRowByID() if you add any
  // new constraints to the schema.
  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      // Using AUTOINCREMENT is for sync propose. Sync uses this |id| as an
      // unique key to identify the Events. If here did not use AUTOINCREMENT,
      // and Sync was not working somehow, a ROWID could be deleted and re-used
      // during this period. Once Sync come back, Sync would use ROWIDs and
      // timestamps to see if there are any updates need to be synced. And sync
      //  will only see the new Event, but missed the deleted Event.
      "name LONGVARCHAR,"
      "color LONGVARCHAR,"
      "iconindex INTEGER,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  return GetDB().Execute(sql);
}

EventTypeID EventTypeDatabase::CreateEventType(calendar::EventTypeRow row) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO event_type "
      "(name, color, iconindex, created, last_modified) "
      "VALUES (?, ?, ?, ?, ?)"));

  statement.BindString16(0, row.name());
  statement.BindString(1, row.color());
  statement.BindInt(2, row.iconindex());
  statement.BindInt64(3, base::Time().Now().ToInternalValue());
  statement.BindInt64(4, base::Time().Now().ToInternalValue());

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool EventTypeDatabase::GetAllEventTypes(EventTypeRows* events) {
  events->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" CALENDAR_EVENT_TYPE_ROW_FIELDS " FROM event_type "));

  while (s.Step()) {
    EventTypeRow event;
    FillEventTypeRow(s, &event);
    events->push_back(event);
  }

  return true;
}

bool EventTypeDatabase::GetRowForEventType(calendar::EventTypeID event_id,
                                           EventTypeRow* out_event_type) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" CALENDAR_EVENT_TYPE_ROW_FIELDS "FROM event_type WHERE id=?"));
  statement.BindInt64(0, event_id);

  if (!statement.Step())
    return false;

  FillEventTypeRow(statement, out_event_type);

  return true;
}

bool EventTypeDatabase::UpdateEventTypeRow(const EventTypeRow& event) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                                      "UPDATE event_type SET \
        name=?, color=?, iconindex=? \
        WHERE id=?"));
  statement.BindString16(0, event.name());
  statement.BindString(1, event.color());
  statement.BindInt(2, event.iconindex());
  statement.BindInt64(3, event.id());

  return statement.Run();
}

// Must be in sync with CALENDAR_EVENT_TYPE_ROW_FIELDS.
// static
void EventTypeDatabase::FillEventTypeRow(sql::Statement& s,
                                         EventTypeRow* event) {
  EventTypeID id = s.ColumnInt64(0);
  std::u16string name = s.ColumnString16(1);
  std::string color = s.ColumnString(2);
  int iconindex = s.ColumnInt(3);
  event->set_id(id);
  event->set_name(name);
  event->set_color(color);
  event->set_iconindex(iconindex);
}

bool EventTypeDatabase::DeleteEventType(calendar::EventTypeID event_type_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from event_type WHERE id=?"));
  statement.BindInt64(0, event_type_id);

  return statement.Run();
}

}  // namespace calendar
