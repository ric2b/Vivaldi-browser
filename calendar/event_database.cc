// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/event_database.h"

#include <string>
#include <vector>
#include "base/strings/utf_string_conversions.h"
#include "calendar/calendar_types.h"
#include "sql/statement.h"

namespace calendar {

EventDatabase::EventDatabase() {}

EventDatabase::~EventDatabase() {}

bool EventDatabase::CreateEventTable() {
  const char* name = "events";
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
      "calendar_id INTEGER, "
      "title LONGVARCHAR,"
      "description LONGVARCHAR,"
      "start INTEGER NOT NULL,"
      "end INTEGER NOT NULL)");

  return GetDB().Execute(sql.c_str());
}

bool EventDatabase::CreateCalendarEvent(calendar::EventRow row) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT OR REPLACE INTO events "
                                 "(title, description, start, end) "
                                 "VALUES (?, ?, ?,?)"));

  statement.BindString16(0, row.title());
  statement.BindString16(1, row.description());
  statement.BindInt64(2, row.start().ToInternalValue());
  statement.BindInt64(3, row.end().ToInternalValue());
  return statement.Run();
}

bool EventDatabase::GetAllCalendarEvents(EventRows* events) {
  events->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" CALENDAR_EVENT_ROW_FIELDS " FROM events "));

  while (s.Step()) {
    base::string16 id = s.ColumnString16(0);
    base::string16 calendar_id = s.ColumnString16(1);
    base::string16 title = s.ColumnString16(2);
    base::string16 description = s.ColumnString16(3);

    EventRow eve;
    eve.set_id(id);
    eve.set_calendar_id(calendar_id);
    eve.set_title(title);
    eve.set_description(description);
    eve.set_start(base::Time::FromInternalValue(s.ColumnInt64(4)));
    eve.set_end(base::Time::FromInternalValue(s.ColumnInt64(5)));

    events->push_back(eve);
  }

  return true;
}

bool EventDatabase::GetRowForEvent(calendar::EventID event_id,
                                   EventRow* out_event) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" CALENDAR_EVENT_ROW_FIELDS "FROM events WHERE id=?"));
  statement.BindInt64(0, event_id);

  if (!statement.Step())
    return false;

  FillEventRow(statement, out_event);

  return true;
}

bool EventDatabase::UpdateEventRow(const EventRow& event) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE events SET \
        calendar_id=?,title=?,description=?,start=?,end=? \
        WHERE id=?"));
  statement.BindString16(0, event.calendar_id());
  statement.BindString16(1, event.title());
  statement.BindString16(2, event.description());
  statement.BindInt64(3, event.start().ToInternalValue());
  statement.BindInt64(4, event.end().ToInternalValue());
  statement.BindString16(5, event.id());

  return statement.Run();
}

// Must be in sync with CALENDAR_EVENT_ROW_FIELDS.
// static
void EventDatabase::FillEventRow(sql::Statement& statement, EventRow* event) {
  base::string16 id = statement.ColumnString16(0);
  base::string16 calendar_id = statement.ColumnString16(1);
  base::string16 title = statement.ColumnString16(2);
  base::string16 description = statement.ColumnString16(3);

  event->set_id(id);
  event->set_calendar_id(calendar_id);
  event->set_title(title);
  event->set_description(description);
  event->set_start(base::Time::FromInternalValue(statement.ColumnInt64(4)));
  event->set_end(base::Time::FromInternalValue(statement.ColumnInt64(5)));
}

}  // namespace calendar
