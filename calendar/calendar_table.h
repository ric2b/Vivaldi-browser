// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_CALENDAR_TABLE_H_
#define CALENDAR_CALENDAR_TABLE_H_

#include <stddef.h>

#include "calendar/calendar_type.h"
#include "sql/statement.h"

namespace sql {
class Database;
}

namespace calendar {

// Encapsulates an SQL table that holds Calendar info.
//
// This is refcounted to support calling InvokeLater() with some of its methods
// (necessary to maintain ordering of DB operations).
class CalendarTable {
 public:
  // Must call CreateCalendarTable() before to make sure the database is
  // initialized.
  CalendarTable();
  CalendarTable(const CalendarTable&) = delete;
  CalendarTable& operator=(const CalendarTable&) = delete;

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~CalendarTable();

  bool CreateCalendarTable();
  bool CreateDefaultCalendar(AccountID account_id);

  CalendarID CreateCalendar(CalendarRow row);
  bool GetAllCalendars(CalendarRows* calendars);
  bool GetAllCalendarIdsForAccount(CalendarIDs* calendars,
                                   AccountID account_id);
  bool GetRowForCalendar(CalendarID calendar_id, CalendarRow* out_calendar);
  bool UpdateCalendarRow(const CalendarRow& calendar);
  bool DeleteCalendar(CalendarID calendar_id);
  bool DoesCalendarIdExist(CalendarID calendar_id);
  bool MigrateCalendarToVersion3();
  bool MigrateCalendarToVersion10();

 protected:
  virtual sql::Database& GetDB() = 0;
  void FillCalendarRow(sql::Statement& statement, CalendarRow* calendar);

 private:
  bool DoesAnyCalendarExist();
};

#define CALENDAR_ROW_FIELDS                                        \
  " id, account_id, name, description,  ctag, orderindex, color, " \
  " hidden, active, iconindex, last_checked , "                    \
  " timezone, supported_component_set, created, last_modified "

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_TABLE_H_
