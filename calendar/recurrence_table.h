// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_RECURRENCE_TABLE_H_
#define CALENDAR_RECURRENCE_TABLE_H_

#include <stddef.h>

#include "base/macros.h"
#include "calendar/recurrence_type.h"
#include "sql/statement.h"

namespace sql {
class Connection;
}

namespace calendar {

// Encapsulates an SQL database that holds info about event recurrence.
//
class RecurrrenceTable {
 public:
  // Must call CreateRecurringTable() before using to make
  // sure the database is initialized.
  RecurrrenceTable();

  bool CreateRecurringTable();

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~RecurrrenceTable();

  RecurrenceID CreateRecurrenceEvent(RecurrenceRow ev);
  bool GetAllRecurrences(RecurrenceRows* recurrences);

  bool GetRecurrenceRow(EventID event_id, RecurrenceRow* out_recurrence);

  bool UpdateRecurrenceRow(const RecurrenceRow& recurrence);
  bool DeleteRecurrence(RecurrenceID recurrence_id);

 protected:
  virtual sql::Connection& GetDB() = 0;
  void FillRecurrenceRow(const sql::Statement& statement,
                         RecurrenceRow* recurrence_row);

 private:
  DISALLOW_COPY_AND_ASSIGN(RecurrrenceTable);
};

// This is available BOTH as a macro and a static string (kURLRowFields). Use
// the macro if you want to put this in the middle of an otherwise constant
// string, it will save time doing string appends. If you have to build a SQL
// string dynamically anyway, use the constant, it will save space.
#define CALENDAR_RECURRING_ROW_FIELDS                                  \
  " id, event_id, interval, number_of_ocurrences, skip_count, "        \
  "day_of_week, week_of_month, day_of_month, month_of_year, created, " \
  "last_modified "

}  // namespace calendar

#endif  // CALENDAR_RECURRENCE_TABLE_H_
