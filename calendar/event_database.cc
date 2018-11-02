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
#include "calendar/calendar_type.h"
#include "calendar/event_type.h"
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
      "alarm_id INTEGER, "
      "title LONGVARCHAR,"
      "description LONGVARCHAR,"
      "start INTEGER NOT NULL,"
      "end INTEGER NOT NULL,"
      "all_day INTEGER,"
      "is_recurring INTEGER,"
      "start_recurring INTEGER,"
      "end_recurring INTEGER,"
      "location LONGVARCHAR,"
      "url LONGVARCHAR,"
      "parent_event_id INTEGER,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  return GetDB().Execute(sql.c_str());
}

EventID EventDatabase::CreateCalendarEvent(calendar::EventRow row) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO events "
      "(calendar_id, alarm_id, title, description, "
      "start, end, all_day, is_recurring, start_recurring, end_recurring, "
      "location, url) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, row.calendar_id());
  statement.BindInt64(1, row.alarm_id());
  statement.BindString16(2, row.title());
  statement.BindString16(3, row.description());
  statement.BindInt64(4, row.start().ToInternalValue());
  statement.BindInt64(5, row.end().ToInternalValue());
  statement.BindInt(6, row.all_day() ? 1 : 0);
  statement.BindInt(7, row.is_recurring() ? 1 : 0);
  statement.BindInt64(8, row.start_recurring().ToInternalValue());
  statement.BindInt64(9, row.end_recurring().ToInternalValue());
  statement.BindString16(10, row.location());
  statement.BindString16(11, row.url());

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool EventDatabase::GetAllCalendarEvents(EventRows* events) {
  events->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" CALENDAR_EVENT_ROW_FIELDS " FROM events "));

  while (s.Step()) {
    EventRow event;
    FillEventRow(s, &event);
    events->push_back(event);
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
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                                      "UPDATE events SET \
        calendar_id=?, alarm_id=?, title=?, description=?, start=?, end=?, \
        all_day=?, is_recurring=?, start_recurring=?, end_recurring=?, \
        location=?, url=? \
        WHERE id=?"));
  statement.BindInt64(0, event.calendar_id());
  statement.BindInt64(1, event.alarm_id());
  statement.BindString16(2, event.title());
  statement.BindString16(3, event.description());
  statement.BindInt64(4, event.start().ToInternalValue());
  statement.BindInt64(5, event.end().ToInternalValue());
  statement.BindInt(6, event.all_day() ? 1 : 0);
  statement.BindInt(7, event.is_recurring() ? 1 : 0);
  statement.BindInt64(8, event.start_recurring().ToInternalValue());
  statement.BindInt64(9, event.end_recurring().ToInternalValue());
  statement.BindString16(10, event.location());
  statement.BindString16(11, event.url());
  statement.BindInt64(12, event.id());

  return statement.Run();
}

// Must be in sync with CALENDAR_EVENT_ROW_FIELDS.
// static
void EventDatabase::FillEventRow(sql::Statement& s, EventRow* event) {
  EventID id = s.ColumnInt64(0);
  CalendarID calendar_id = s.ColumnInt64(1);
  AlarmID alarm_id = s.ColumnInt64(2);
  base::string16 title = s.ColumnString16(3);
  base::string16 description = s.ColumnString16(4);
  base::Time start = base::Time::FromInternalValue(s.ColumnInt64(5));
  base::Time end = base::Time::FromInternalValue(s.ColumnInt64(6));
  int all_day = s.ColumnInt(7);
  int is_recurring = s.ColumnInt(8);
  base::Time start_recurring = base::Time::FromInternalValue(s.ColumnInt64(9));
  base::Time end_recurring = base::Time::FromInternalValue(s.ColumnInt64(10));
  base::string16 location = s.ColumnString16(11);
  base::string16 url = s.ColumnString16(12);

  event->set_id(id);
  event->set_calendar_id(calendar_id);
  event->set_alarm_id(alarm_id);
  event->set_title(title);
  event->set_description(description);
  event->set_start(start);
  event->set_end(end);
  event->set_all_day(all_day == 1 ? true : false);
  event->set_is_recurring(is_recurring == 1 ? true : false);
  event->set_start_recurring(start_recurring);
  event->set_end_recurring(end_recurring);
  event->set_location(location);
  event->set_url(url);
}

bool EventDatabase::DeleteEvent(calendar::EventID event_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from events WHERE id=?"));
  statement.BindInt64(0, event_id);

  return statement.Run();
}

}  // namespace calendar
