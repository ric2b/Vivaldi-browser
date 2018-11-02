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

#include "base/macros.h"
#include "calendar/calendar_type.h"
#include "sql/statement.h"

namespace sql {
class Connection;
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

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~CalendarTable();

  bool CreateCalendarTable();
  bool CreateDefaultCalendar();

  CalendarID CreateCalendar(CalendarRow row);
  bool GetAllCalendars(CalendarRows* calendars);
  bool GetRowForCalendar(CalendarID calendar_id, CalendarRow* out_calendar);
  bool UpdateCalendarRow(const CalendarRow& calendar);
  bool DeleteCalendar(CalendarID calendar_id);
  bool DoesCalendarIdExist(CalendarID calendar_id);

 protected:
  virtual sql::Connection& GetDB() = 0;
  void FillCalendarRow(sql::Statement& statement, CalendarRow* calendar);

 private:
  std::string GURLToDatabaseURL(const GURL& gurl);
  bool DoesAnyCalendarExist();

  DISALLOW_COPY_AND_ASSIGN(CalendarTable);
};

#define CALENDAR_ROW_FIELDS                                         \
  " id, name, description,  url, ctag, orderindex, color, hidden, " \
  " created, last_modified "

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_TABLE_H_
