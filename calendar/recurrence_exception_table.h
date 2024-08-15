// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_RECURRENCE_EXCEPTION_TABLE_H_
#define CALENDAR_RECURRENCE_EXCEPTION_TABLE_H_

#include <stddef.h>

#include "calendar/event_type.h"
#include "calendar/recurrence_exception_type.h"
#include "sql/statement.h"

namespace sql {
class Database;
}

namespace calendar {

// Encapsulates an SQL database that holds info about event recurrence exeption.
//
class RecurrrenceExceptionTable {
 public:
  // Must call CreateRecurringExceptionTable() before using to make
  // sure the database is initialized.
  RecurrrenceExceptionTable() = default;

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~RecurrrenceExceptionTable() = default;

  RecurrrenceExceptionTable(const RecurrrenceExceptionTable&) = delete;
  RecurrrenceExceptionTable& operator=(const RecurrrenceExceptionTable&) =
      delete;

  bool CreateRecurringExceptionTable();

  RecurrenceExceptionID CreateRecurrenceException(RecurrenceExceptionRow ev);
  bool GetAllRecurrenceExceptions(RecurrenceExceptionRows* recurrences);

  bool UpdateRecurrenceExceptionRow(const RecurrenceExceptionRow& exception_id);
  bool DeleteRecurrenceException(RecurrenceExceptionID exception_id);
  bool DeleteRecurrenceExceptions(EventID event_id);
  bool DeleteRecurrenceExceptionsForCalendar(CalendarID calendar_id);
  bool GetAllRecurrenceExceptionsForEvent(EventID event_id,
                                          RecurrenceExceptionRows* exceptions);
  bool GetRecurrenceException(RecurrenceExceptionID exception_id,
                              RecurrenceExceptionRow* recurrence_exception);

  bool DoesRecurrenceExceptionExistForEvent(EventID event_id);
  bool GetAllEventExceptionIds(EventID event_id, EventIDs* event_ids);
  EventID GetParentExceptionEventId(EventID exception_event_id);

 protected:
  virtual sql::Database& GetDB() = 0;
  void FillRecurrenceExceptionRow(sql::Statement& statement,
                                  RecurrenceExceptionRow* exception_row);
};

// Use the macro if you want to put this in the middle of an otherwise constant
// string, it will save time doing string appends. If you have to build a SQL
// string dynamically anyway, use the constant, it will save space.
#define CALENDAR_RECURRING_EXCEPTION_ROW_FIELDS \
  " id, parent_event_id, exception_event_id, exception_day, cancelled, \
    created, last_modified "

}  // namespace calendar
#endif  // CALENDAR_RECURRENCE_EXCEPTION_TABLE_H_
