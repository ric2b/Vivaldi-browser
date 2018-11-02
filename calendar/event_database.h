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

#include "base/macros.h"
#include "calendar/event_type.h"
#include "sql/statement.h"

namespace sql {
class Connection;
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

  EventID CreateCalendarEvent(calendar::EventRow ev);

  bool CreateEventTable();
  bool GetAllCalendarEvents(EventRows* events);

  bool GetRowForEvent(EventID event_id, EventRow* out_event);
  bool UpdateEventRow(const EventRow& event);
  bool DeleteEvent(EventID event_id);

 protected:
  virtual sql::Connection& GetDB() = 0;
  void FillEventRow(sql::Statement& statement, EventRow* event);

 private:
  DISALLOW_COPY_AND_ASSIGN(EventDatabase);
};

// This is available BOTH as a macro and a static string (kURLRowFields). Use
// the macro if you want to put this in the middle of an otherwise constant
// string, it will save time doing string appends. If you have to build a SQL
// string dynamically anyway, use the constant, it will save space.
#define CALENDAR_EVENT_ROW_FIELDS                                         \
  " id, calendar_id, alarm_id, title, description, start, end, all_day, " \
  "is_recurring, start_recurring, end_recurring, location, url "

}  // namespace calendar

#endif  // CALENDAR_EVENT_DATABASE_H_
