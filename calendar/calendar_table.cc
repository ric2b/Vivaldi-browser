// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/calendar_table.h"

#include <string>
#include <vector>
#include "base/strings/utf_string_conversions.h"
#include "calendar/calendar_type.h"
#include "sql/statement.h"

namespace calendar {

CalendarTable::CalendarTable() {}

CalendarTable::~CalendarTable() {}

bool CalendarTable::CreateCalendarTable() {
  const char* name = "calendar";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      // Using AUTOINCREMENT is for sync propose. Sync uses this |id| as an
      // unique key to identify the Calendar. If here did not use
      // AUTOINCREMEclNT, and Sync was not working somehow, a ROWID could be
      // deleted and re-used during this period. Once Sync come back, Sync would
      // use ROWIDs and timestamps to see if there are any updates need to be
      // synced. And sync
      //  will only see the new Calendar, but missed the deleted Calendar.
      "name LONGVARCHAR,"
      "description LONGVARCHAR,"
      "url LONGVARCHAR,"
      "ctag VARCHAR,"
      "orderindex INTEGER,"
      "color VARCHAR DEFAULT '#AAAAAAFF' NOT NULL,"
      "hidden INTEGER DEFAULT 0,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  bool res = GetDB().Execute(sql.c_str());

  return res;
}

bool CalendarTable::CreateDefaultCalendar() {
  if (DoesAnyCalendarExist())
    return false;

  CalendarRow row;
  row.set_name(base::UTF8ToUTF16("Default"));
  CalendarID id = CreateCalendar(row);
  if (id)
    return true;

  return false;
}

// static
std::string CalendarTable::GURLToDatabaseURL(const GURL& gurl) {
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();

  return (gurl.ReplaceComponents(replacements)).spec();
}

CalendarID CalendarTable::CreateCalendar(CalendarRow row) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO calendar "
                                 "(name, description, url, ctag, "
                                 "orderindex, color, created, last_modified) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

  statement.BindString16(0, row.name());
  statement.BindString16(1, row.description());
  statement.BindString(2, GURLToDatabaseURL(row.url()));
  statement.BindString(3, row.ctag());
  statement.BindInt(4, row.orderindex());
  statement.BindString(5, row.color());
  statement.BindInt64(6, base::Time().Now().ToInternalValue());
  statement.BindInt64(7, base::Time().Now().ToInternalValue());

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool CalendarTable::GetAllCalendars(CalendarRows* calendars) {
  calendars->clear();
  sql::Statement s(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT id, name, description, "
                                 "url, ctag, orderindex, color, hidden, "
                                 "created, last_modified FROM calendar"));
  while (s.Step()) {
    CalendarRow calendar;
    FillCalendarRow(s, &calendar);
    calendars->push_back(calendar);
  }

  return true;
}

bool CalendarTable::UpdateCalendarRow(const CalendarRow& calendar) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                                      "UPDATE calendar SET \
        name=?, description=?, url=?, orderindex=?, color=?, hidden=? "
                                                      " WHERE id=?"));
  statement.BindString16(0, calendar.name());
  statement.BindString16(1, calendar.description());
  statement.BindString(2, GURLToDatabaseURL(calendar.url()));
  statement.BindInt(3, calendar.orderindex());
  statement.BindString(4, calendar.color());
  statement.BindInt(5, calendar.hidden() ? 1 : 0);
  statement.BindInt64(6, calendar.id());

  return statement.Run();
}

bool CalendarTable::DeleteCalendar(calendar::CalendarID calendar_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from calendar WHERE id=?"));
  statement.BindInt64(0, calendar_id);

  return statement.Run();
}

bool CalendarTable::GetRowForCalendar(CalendarID calendar_id,
                                      CalendarRow* out_calendar) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" CALENDAR_ROW_FIELDS "FROM calendar WHERE id=?"));
  statement.BindInt64(0, calendar_id);

  if (!statement.Step())
    return false;

  FillCalendarRow(statement, out_calendar);

  return true;
}

void CalendarTable::FillCalendarRow(sql::Statement& statement,
                                    CalendarRow* calendar) {
  CalendarID id = statement.ColumnInt64(0);
  base::string16 name = statement.ColumnString16(1);
  base::string16 description = statement.ColumnString16(2);
  std::string url = statement.ColumnString(3);
  std::string ctag = statement.ColumnString(4);
  int orderindex = statement.ColumnInt(5);
  std::string color = statement.ColumnString(6);
  bool hidden = statement.ColumnInt(7) != 0;

  calendar->set_id(id);
  calendar->set_name(name);
  calendar->set_description(description);
  calendar->set_url(GURL(url));
  calendar->set_ctag(ctag);
  calendar->set_orderindex(orderindex);
  calendar->set_color(color);
  calendar->set_hidden(hidden);
}

bool CalendarTable::DoesCalendarIdExist(CalendarID calendar_id) {
  sql::Statement statement(
      GetDB().GetUniqueStatement("select count(*) as count from calendar \
        WHERE id=?"));
  statement.BindInt64(0, calendar_id);

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) == 1;
}

bool CalendarTable::DoesAnyCalendarExist() {
  sql::Statement statement(
      GetDB().GetUniqueStatement("select count(*) from calendar"));

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) > 1;
}

}  // namespace calendar
