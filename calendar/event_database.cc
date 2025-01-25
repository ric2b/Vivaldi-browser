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
      "etag LONGVARCHAR,"
      "href LONGVARCHAR,"
      "uid LONGVARCHAR,"
      "event_type_id INTEGER,"
      "task INTEGER,"
      "complete INTEGER,"
      "trash INTEGER,"
      "trash_time INTEGER, "
      "sequence INTEGER DEFAULT 0 NOT NULL,"
      "ical LONGVARCHAR,"
      "rrule LONGVARCHAR,"
      "organizer LONGVARCHAR,"
      "timezone LONGVARCHAR,"
      "is_template INTEGER DEFAULT 0 NOT NULL,"
      "due INTEGER,"
      "priority INTEGER,"
      "status LONGVARCHAR,"
      "percentage_complete INTEGER,"
      "categories LONGVARCHAR,"
      "component_class LONGVARCHAR,"
      "attachment LONGVARCHAR,"
      "completed INTEGER,"
      "sync_pending INTEGER,"
      "delete_pending INTEGER,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  return GetDB().Execute(sql);
}

EventID EventDatabase::CreateCalendarEvent(calendar::EventRow row) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO events "
      "(calendar_id, alarm_id, title, description, "
      "start, end, all_day, is_recurring, "
      "location, url, etag, href, uid, event_type_id, task, complete, trash, "
      "trash_time, sequence, ical, rrule, organizer, timezone, is_template, "
      "priority, status, percentage_complete, categories, "
      "component_class, attachment, completed, sync_pending, delete_pending, "
      "end_recurring, created, last_modified) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
      "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )"));

  int column_index = 0;
  statement.BindInt64(column_index++, row.calendar_id);
  statement.BindInt64(column_index++, row.alarm_id);
  statement.BindString16(column_index++, row.title);
  statement.BindString16(column_index++, row.description);
  statement.BindInt64(column_index++, row.start.ToInternalValue());
  statement.BindInt64(column_index++, row.end.ToInternalValue());
  statement.BindInt(column_index++, row.all_day ? 1 : 0);
  statement.BindInt(column_index++, row.is_recurring ? 1 : 0);
  statement.BindString16(column_index++, row.location);
  statement.BindString16(column_index++, row.url);
  statement.BindString(column_index++, row.etag);
  statement.BindString(column_index++, row.href);
  statement.BindString(column_index++, row.uid);
  statement.BindInt64(column_index++, row.event_type_id);
  statement.BindInt(column_index++, row.task ? 1 : 0);
  statement.BindInt(column_index++, row.complete ? 1 : 0);
  statement.BindInt(column_index++, row.trash ? 1 : 0);
  statement.BindInt64(column_index++,
                      row.trash ? base::Time().Now().ToInternalValue() : 0);
  statement.BindInt64(column_index++, row.sequence);
  statement.BindString16(column_index++, row.ical);
  statement.BindString(column_index++, row.rrule);
  statement.BindString(column_index++, row.organizer);
  statement.BindString(column_index++, row.timezone);
  statement.BindInt(column_index++, row.is_template ? 1 : 0);
  statement.BindInt(column_index++, row.priority);
  statement.BindString(column_index++, row.status);
  statement.BindInt(column_index++, row.percentage_complete);
  statement.BindString16(column_index++, row.categories);
  statement.BindString16(column_index++, row.component_class);
  statement.BindString16(column_index++, row.attachment);
  statement.BindInt64(column_index++, row.completed.ToInternalValue());
  statement.BindInt(column_index++, row.sync_pending ? 1 : 0);
  statement.BindInt(column_index++, row.delete_pending ? 1 : 0);
  statement.BindInt64(column_index++, row.end_recurring.ToInternalValue());
  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());
  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool EventDatabase::GetAllCalendarEvents(EventRows* events) {
  events->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" CALENDAR_EVENT_ROW_FIELDS " FROM events "
                     "where is_template = 0"));

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
        all_day=?, is_recurring=?, \
        location=?, url=?, etag=?, href=?, uid=?, event_type_id=?, \
        task=?, complete=?, trash=?, trash_time=?, sequence=?, ical=?, \
        rrule=?, organizer=?, timezone=?, \
        priority=?, status=?, percentage_complete=?,  \
        categories=?, component_class=?, attachment=?, completed=?, \
        sync_pending=?, delete_pending=?, end_recurring=?, last_modified=? \
        WHERE id=?"));
  int column_index = 0;
  statement.BindInt64(column_index++, event.calendar_id);
  statement.BindInt64(column_index++, event.alarm_id);
  statement.BindString16(column_index++, event.title);
  statement.BindString16(column_index++, event.description);
  statement.BindInt64(column_index++, event.start.ToInternalValue());
  statement.BindInt64(column_index++, event.end.ToInternalValue());
  statement.BindInt(column_index++, event.all_day ? 1 : 0);
  statement.BindInt(column_index++, event.is_recurring ? 1 : 0);
  statement.BindString16(column_index++, event.location);
  statement.BindString16(column_index++, event.url);
  statement.BindString(column_index++, event.etag);
  statement.BindString(column_index++, event.href);
  statement.BindString(column_index++, event.uid);
  statement.BindInt64(column_index++, event.event_type_id);
  statement.BindInt(column_index++, event.task ? 1 : 0);
  statement.BindInt(column_index++, event.complete ? 1 : 0);
  statement.BindInt(column_index++, event.trash ? 1 : 0);
  statement.BindInt64(column_index++,
                      event.trash ? base::Time().Now().ToInternalValue() : 0);
  statement.BindInt(column_index++, event.sequence);
  statement.BindString16(column_index++, event.ical);
  statement.BindString(column_index++, event.rrule);
  statement.BindString(column_index++, event.organizer);
  statement.BindString(column_index++, event.timezone);
  statement.BindInt64(column_index++, event.priority);
  statement.BindString(column_index++, event.status);
  statement.BindInt(column_index++, event.percentage_complete);
  statement.BindString16(column_index++, event.categories);
  statement.BindString16(column_index++, event.component_class);
  statement.BindString16(column_index++, event.attachment);
  statement.BindInt64(column_index++, event.completed.ToInternalValue());
  statement.BindInt64(column_index++, event.sync_pending ? 1 : 0);
  statement.BindInt64(column_index++, event.delete_pending ? 1 : 0);
  statement.BindInt64(column_index++, event.end_recurring.ToInternalValue());
  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());

  statement.BindInt64(column_index++, event.id);

  return statement.Run();
}

// Must be in sync with CALENDAR_EVENT_ROW_FIELDS.
// static
void EventDatabase::FillEventRow(sql::Statement& s, EventRow* event) {
  int column_index = 0;
  EventID id = s.ColumnInt64(column_index++);
  CalendarID calendar_id = s.ColumnInt64(column_index++);
  AlarmID alarm_id = s.ColumnInt64(column_index++);
  std::u16string title = s.ColumnString16(column_index++);
  std::u16string description = s.ColumnString16(column_index++);
  base::Time start =
      base::Time::FromInternalValue(s.ColumnInt64(column_index++));
  base::Time end = base::Time::FromInternalValue(s.ColumnInt64(column_index++));
  int all_day = s.ColumnInt(column_index++);
  int is_recurring = s.ColumnInt(column_index++);
  std::u16string location = s.ColumnString16(column_index++);
  std::u16string url = s.ColumnString16(column_index++);
  std::string etag = s.ColumnString(column_index++);
  std::string href = s.ColumnString(column_index++);
  std::string uid = s.ColumnString(column_index++);
  EventTypeID event_type_id = s.ColumnInt64(column_index++);
  int task = s.ColumnInt(column_index++);
  int complete = s.ColumnInt(column_index++);
  int trash = s.ColumnInt(column_index++);
  base::Time trash_time =
      base::Time::FromInternalValue(s.ColumnInt64(column_index++));
  int sequence = s.ColumnInt(column_index++);
  std::u16string ical = s.ColumnString16(column_index++);
  std::string rrule = s.ColumnString(column_index++);
  std::string organizer = s.ColumnString(column_index++);
  std::string timezone = s.ColumnString(column_index++);
  int priority = s.ColumnInt(column_index++);
  std::string status = s.ColumnString(column_index++);
  int percentage_complete = s.ColumnInt(column_index++);
  std::u16string categories = s.ColumnString16(column_index++);
  std::u16string component_class = s.ColumnString16(column_index++);
  std::u16string attachment = s.ColumnString16(column_index++);
  base::Time completed =
      base::Time::FromInternalValue(s.ColumnInt64(column_index++));
  int sync_pending = s.ColumnInt(column_index++);
  int delete_pending = s.ColumnInt(column_index++);
  base::Time end_recurring =
      base::Time::FromInternalValue(s.ColumnInt64(column_index++));

  event->id = id;
  event->calendar_id = calendar_id;
  event->alarm_id = alarm_id;
  event->title = title;
  event->description = description;
  event->start = start;
  event->end = end;
  event->all_day = all_day == 1 ? true : false;
  event->is_recurring = is_recurring == 1 ? true : false;
  event->location = location;
  event->url = url;
  event->etag = etag;
  event->href = href;
  event->uid = uid;
  event->event_type_id = event_type_id;
  event->task = task == 1 ? true : false;
  event->complete = complete == 1 ? true : false;
  event->trash = trash == 1 ? true : false;
  event->trash_time = trash_time;
  event->sequence = sequence;
  event->ical = ical;
  event->rrule = rrule;
  event->organizer = organizer;
  event->timezone = timezone;
  event->priority = priority;
  event->status = status;
  event->percentage_complete = percentage_complete;
  event->categories = categories;
  event->component_class = component_class;
  event->attachment = attachment;
  event->completed = completed;
  event->sync_pending = sync_pending == 1 ? true : false;
  event->delete_pending = delete_pending == 1 ? true : false;
  event->end_recurring = end_recurring;
}

bool EventDatabase::DeleteEvent(calendar::EventID event_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from events WHERE id=?"));
  statement.BindInt64(0, event_id);

  return statement.Run();
}

bool EventDatabase::DeleteEventsForCalendar(CalendarID calendar_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM events WHERE calendar_id=?"));
  statement.BindInt64(0, calendar_id);
  return statement.Run();
}

bool EventDatabase::DoesEventIdExist(EventID event_id) {
  sql::Statement statement(
      GetDB().GetUniqueStatement("select count(*) as count from events \
        WHERE id=?"));
  statement.BindInt64(0, event_id);

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) == 1;
}

// Updates to version 2
bool EventDatabase::MigrateEventsWithoutSequenceAndIcalColumns() {
  if (!GetDB().DoesTableExist("events")) {
    NOTREACHED() << "Events table should exist before migration";
    //return false;
  }

  if (!GetDB().DoesColumnExist("events", "sequence") &&
      !GetDB().DoesColumnExist("events", "ical")) {
    // Old versions don't have the sequence and ical column, we modify the table
    // to add that field.
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN sequence INTEGER DEFAULT 0 NOT NULL"))
      return false;

    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN ical LONGVARCHAR"))
      return false;

    if (!GetDB().Execute("update calendar set ctag = ''"))
      return false;

    if (!GetDB().Execute("update events set etag = ''"))
      return false;
  }
  return true;
}

// Updates to version 4. Adds columns rrule to events
bool EventDatabase::MigrateCalendarToVersion4() {
  if (!GetDB().DoesTableExist("events")) {
    NOTREACHED() << "events table should exist before migration";
    //return false;
  }

  if (!GetDB().DoesColumnExist("events", "rrule")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN rrule LONGVARCHAR"))
      return false;
  }
  return true;
}

// Updates to version 6. Adds columns timezone to events and calendar table
bool EventDatabase::MigrateCalendarToVersion6() {
  if (!GetDB().DoesTableExist("events")) {
    NOTREACHED() << "events table should exist before migration";
    //return false;
  }

  if (!GetDB().DoesColumnExist("events", "timezone")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN timezone LONGVARCHAR"))
      return false;
  }

  if (!GetDB().DoesTableExist("calendar")) {
    NOTREACHED() << "calendar table should exist before migration";
    //return false;
  }

  if (!GetDB().DoesColumnExist("calendar", "timezone")) {
    if (!GetDB().Execute("ALTER TABLE calendar "
                         "ADD COLUMN timezone LONGVARCHAR"))
      return false;
  }
  return true;
}

// Updates to version 8. Adds column is_template to events table
bool EventDatabase::MigrateCalendarToVersion8() {
  if (!GetDB().DoesTableExist("events")) {
    NOTREACHED() << "events table should exist before migration";
    //return false;
  }

  if (!GetDB().DoesColumnExist("events", "is_template")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN is_template INTEGER DEFAULT 0 NOT NULL"))
      return false;
  }
  return true;
}

// Updates to version 9. Adds columns
// due, priority, status, percentage_complete, categories, component_class,
// attachment, completed
bool EventDatabase::MigrateCalendarToVersion9() {
  if (!GetDB().DoesTableExist("events")) {
    NOTREACHED() << "events table should exist before migration";
    //return false;
  }

  if (!GetDB().DoesColumnExist("events", "due")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN due INTEGER"))
      return false;
  }

  if (!GetDB().DoesColumnExist("events", "priority")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN priority INTEGER"))
      return false;
  }

  if (!GetDB().DoesColumnExist("events", "status")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN status LONGVARCHAR"))
      return false;
  }

  if (!GetDB().DoesColumnExist("events", "percentage_complete")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN percentage_complete INTEGER"))
      return false;
  }

  if (!GetDB().DoesColumnExist("events", "categories")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN categories LONGVARCHAR"))
      return false;
  }

  if (!GetDB().DoesColumnExist("events", "component_class")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN component_class LONGVARCHAR"))
      return false;
  }

  if (!GetDB().DoesColumnExist("events", "attachment")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN attachment LONGVARCHAR"))
      return false;
  }

  if (!GetDB().DoesColumnExist("events", "completed")) {
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN completed INTEGER"))
      return false;
  }
  return true;
}

// Updates to version 11. Adds columns syncPending and deletePending to events
bool EventDatabase::MigrateCalendarToVersion11() {
  if (!GetDB().DoesTableExist("events")) {
    NOTREACHED() << "Events table should exist before migration";
    //return false;
  }

  if (!GetDB().DoesColumnExist("events", "sync_pending")) {
    LOG(ERROR) << "Dalkurinn sync_pending ekki til i events";
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN sync_pending INTEGER"))
      return false;
  }

  if (!GetDB().DoesColumnExist("events", "delete_pending")) {
    LOG(ERROR) << "Dalkurinn delete_pending ekki til i events";
    if (!GetDB().Execute("ALTER TABLE events "
                         "ADD COLUMN delete_pending INTEGER"))
      return false;
  }
  return true;
}

// Updates to version 12. Migrate deprecated due column to end column
bool EventDatabase::MigrateCalendarToVersion12() {
  if (!GetDB().Execute("UPDATE events "
                       " SET end = due WHERE task = true"))
    return false;

  return true;
}

// Updates to version 13. VB-94637 re-sync certain events
// to update invalid timezone on server
bool EventDatabase::MigrateCalendarToVersion13() {
  if (!GetDB().Execute(
          "UPDATE events "
          " SET sync_pending = 1 "
          " WHERE start = 11644473600000000 AND end = 11644473600000000"))
    return false;

  return true;
}

// Updates to version 14.
// VB-95275 re-sync certain events to update invalid reccurrence
bool EventDatabase::MigrateCalendarToVersion14() {
  if (!GetDB().Execute("UPDATE events "
                       " SET etag = ''"
                       "  WHERE etag != '' "
                       "   AND rrule = '' "
                       "   AND trash != 1;"))
    return false;

  if (!GetDB().Execute("UPDATE calendar "
                       " SET ctag = '' "
                       "  WHERE ctag != '';"))
    return false;

  return true;
}

}  // namespace calendar
