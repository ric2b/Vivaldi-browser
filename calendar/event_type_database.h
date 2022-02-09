// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_EVENT_TYPE_DATABASE_H_
#define CALENDAR_EVENT_TYPE_DATABASE_H_

#include <stddef.h>

#include "calendar/event_type.h"
#include "sql/statement.h"

namespace sql {
class Database;
}

namespace calendar {

// Encapsulates an SQL database that holds EventType info.
//
// This is refcounted to support calling InvokeLater() with some of its methods
// (necessary to maintain ordering of DB operations).
class EventTypeDatabase {
 public:
  // Must call CreateEventTypeTable() before using to make sure the
  // database is initialized.
  EventTypeDatabase() = default;

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~EventTypeDatabase() = default;

  EventTypeDatabase(const EventTypeDatabase&) = delete;
  EventTypeDatabase& operator=(const EventTypeDatabase&) = delete;

  EventTypeID CreateEventType(calendar::EventTypeRow ev);

  bool CreateEventTypeTable();
  bool GetAllEventTypes(EventTypeRows* events);

  bool GetRowForEventType(EventTypeID event_id, EventTypeRow* out_event);
  bool UpdateEventTypeRow(const EventTypeRow& event);
  bool DeleteEventType(EventTypeID event_type_id);

 protected:
  virtual sql::Database& GetDB() = 0;
  void FillEventTypeRow(sql::Statement& statement, EventTypeRow* event);
};

// This is available BOTH as a macro and a static string (kURLRowFields). Use
// the macro if you want to put this in the middle of an otherwise constant
// string, it will save time doing string appends. If you have to build a SQL
// string dynamically anyway, use the constant, it will save space.
#define CALENDAR_EVENT_TYPE_ROW_FIELDS " id, name, color, iconindex "

}  // namespace calendar

#endif  // CALENDAR_EVENT_TYPE_DATABASE_H_
