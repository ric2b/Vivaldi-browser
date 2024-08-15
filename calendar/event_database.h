// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_EVENT_DATABASE_H_
#define CALENDAR_EVENT_DATABASE_H_

#include <stddef.h>

#include "calendar/event_type.h"
#include "sql/statement.h"

namespace sql {
class Database;
}

namespace calendar {

// Encapsulates an SQL database that holds Event info.
//
// This is refcounted to support calling InvokeLater() with some of its methods
// (necessary to maintain ordering of DB operations).
class EventDatabase {
 public:
  // Must call CreateEventTable() and CreateEventIndexes() before using to make
  // sure the database is initialized.
  EventDatabase();

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~EventDatabase();

  EventDatabase(const EventDatabase&) = delete;
  EventDatabase& operator=(const EventDatabase&) = delete;

  EventID CreateCalendarEvent(calendar::EventRow ev);

  bool CreateEventTable();
  bool GetAllCalendarEvents(EventRows* events);
  bool GetRowForEvent(EventID event_id, EventRow* out_event);
  bool UpdateEventRow(const EventRow& event);
  bool DeleteEvent(EventID event_id);
  bool DeleteEventsForCalendar(CalendarID calendar_id);
  bool DoesEventIdExist(EventID event_id);

 protected:
  virtual sql::Database& GetDB() = 0;
  void FillEventRow(sql::Statement& statement, EventRow* event);
  bool MigrateEventsWithoutSequenceAndIcalColumns();
  bool MigrateCalendarToVersion4();
  bool MigrateCalendarToVersion6();
  bool MigrateCalendarToVersion8();
  bool MigrateCalendarToVersion9();
  bool MigrateCalendarToVersion11();
  bool MigrateCalendarToVersion12();
  bool MigrateCalendarToVersion13();
  bool MigrateCalendarToVersion14();
};

// This is available BOTH as a macro and a static string (kURLRowFields). Use
// the macro if you want to put this in the middle of an otherwise constant
// string, it will save time doing string appends. If you have to build a SQL
// string dynamically anyway, use the constant, it will save space.
#define CALENDAR_EVENT_ROW_FIELDS                                           \
  " id, calendar_id, alarm_id, title, description, start, end, all_day, "   \
  "is_recurring, location, url, etag, href,"                                \
  "uid, event_type_id, task, complete, trash, trash_time, sequence, ical, " \
  "rrule, organizer, timezone, priority, status, percentage_complete, "     \
  "categories, component_class, attachment, completed, sync_pending, "      \
  "delete_pending, end_recurring "

}  // namespace calendar

#endif  // CALENDAR_EVENT_DATABASE_H_
