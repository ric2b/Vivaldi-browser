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

#include "app/vivaldi_resources.h"
#include "base/strings/utf_string_conversions.h"
#include "calendar/account_type.h"
#include "calendar/calendar_type.h"
#include "sql/statement.h"
#include "ui/base/l10n/l10n_util.h"

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
      "account_id INTEGER NOT NULL,"
      "name LONGVARCHAR,"
      "description LONGVARCHAR,"
      "ctag VARCHAR,"
      "orderindex INTEGER DEFAULT 0,"
      "color VARCHAR DEFAULT '#AAAAAAFF' NOT NULL,"
      "hidden INTEGER DEFAULT 0,"
      "active INTEGER DEFAULT 0,"
      "iconindex INTEGER DEFAULT 0,"
      "last_checked INTEGER NOT NULL,"
      "timezone LONGVARCHAR,"
      "supported_component_set INTEGER,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  bool res = GetDB().Execute(sql);

  return res;
}

bool CalendarTable::CreateDefaultCalendar(AccountID account_id) {
  if (DoesAnyCalendarExist())
    return false;

  CalendarRow row;
  row.set_name(l10n_util::GetStringUTF16(IDS_DEFAULT_CALENDAR_NAME));
  row.set_description(l10n_util::GetStringUTF16(IDS_DEFAULT_CALENDAR_NAME));
  row.set_color("#4FACF2");
  row.set_account_id(account_id);
  int component_set = CALENDAR_VEVENT;
  component_set |= CALENDAR_VTODO;
  row.set_supported_component_set(component_set);
  CalendarID id = CreateCalendar(row);
  if (id)
    return true;

  return false;
}

CalendarID CalendarTable::CreateCalendar(CalendarRow row) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO calendar "
      "(account_id, name, description, ctag, "
      "orderindex, color, hidden, active, iconindex, "
      "last_checked, timezone, supported_component_set, "
      "created, last_modified) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  int column_index = 0;
  statement.BindInt64(column_index++, row.account_id());
  statement.BindString16(column_index++, row.name());
  statement.BindString16(column_index++, row.description());
  statement.BindString(column_index++, row.ctag());
  statement.BindInt(column_index++, row.orderindex());
  statement.BindString(column_index++, row.color());
  statement.BindInt(column_index++, row.hidden() ? 1 : 0);
  statement.BindInt(column_index++, row.active() ? 1 : 0);
  statement.BindInt(column_index++, row.iconindex());
  statement.BindInt64(column_index++, row.last_checked().ToInternalValue());
  statement.BindString(column_index++, row.timezone());
  statement.BindInt(column_index++, row.supported_component_set());

  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());
  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool CalendarTable::GetAllCalendars(CalendarRows* calendars) {
  calendars->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, account_id, name, description, "
      " ctag, orderindex, color, hidden, active, iconindex, "
      "last_checked, timezone, supported_component_set, "
      "created, last_modified FROM calendar"));
  while (s.Step()) {
    CalendarRow calendar;
    FillCalendarRow(s, &calendar);
    calendars->push_back(calendar);
  }

  return true;
}

bool CalendarTable::GetAllCalendarIdsForAccount(CalendarIDs* calendars,
                                                AccountID account_id) {
  calendars->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT id FROM calendar where account_id=?"));

  s.BindInt64(0, account_id);

  while (s.Step()) {
    CalendarID id = s.ColumnInt64(0);
    calendars->push_back(id);
  }

  return true;
}

bool CalendarTable::UpdateCalendarRow(const CalendarRow& calendar) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                                      "UPDATE calendar SET \
        name=?, description=?, ctag=?, orderindex=?, color=?, hidden=?, \
        active=?, iconindex=?, last_checked=?, \
        timezone=?, supported_component_set=?, last_modified=? WHERE id=?"));
  int column_index = 0;
  statement.BindString16(column_index++, calendar.name());
  statement.BindString16(column_index++, calendar.description());
  statement.BindString(column_index++, calendar.ctag());
  statement.BindInt(column_index++, calendar.orderindex());
  statement.BindString(column_index++, calendar.color());
  statement.BindInt(column_index++, calendar.hidden() ? 1 : 0);
  statement.BindInt(column_index++, calendar.active() ? 1 : 0);
  statement.BindInt(column_index++, calendar.iconindex());
  statement.BindInt64(column_index++,
                      calendar.last_checked().ToInternalValue());
  statement.BindString(column_index++, calendar.timezone());
  statement.BindInt(column_index++, calendar.supported_component_set());
  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());

  statement.BindInt64(column_index++, calendar.id());

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
  int column_index = 0;
  CalendarID id = statement.ColumnInt64(column_index++);
  AccountID account_id = statement.ColumnInt64(column_index++);
  std::u16string name = statement.ColumnString16(column_index++);
  std::u16string description = statement.ColumnString16(column_index++);
  std::string ctag = statement.ColumnString(column_index++);
  int orderindex = statement.ColumnInt(column_index++);
  std::string color = statement.ColumnString(column_index++);
  bool hidden = statement.ColumnInt(column_index++) != 0;
  bool active = statement.ColumnInt(column_index++) != 0;
  int iconindex = statement.ColumnInt(column_index++);
  base::Time last_checked =
      base::Time::FromInternalValue(statement.ColumnInt64(column_index++));
  std::string timezone = statement.ColumnString(column_index++);
  int supported_component_set = statement.ColumnInt(column_index++);

  calendar->set_id(id);
  calendar->set_account_id(account_id);
  calendar->set_name(name);
  calendar->set_description(description);
  calendar->set_ctag(ctag);
  calendar->set_orderindex(orderindex);
  calendar->set_color(color);
  calendar->set_hidden(hidden);
  calendar->set_active(active);
  calendar->set_iconindex(iconindex);
  calendar->set_last_checked(last_checked);
  calendar->set_timezone(timezone);
  calendar->set_supported_component_set(supported_component_set);
}

bool CalendarTable::DoesCalendarIdExist(CalendarID calendar_id) {
  // NOTE(arnar@vivaldi.com): Special calendar_id for event templates
  if (calendar_id == -10) {
    return true;
  }

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

  return statement.ColumnInt(0) > 0;
}

// Updates to version 3. Adds columns last_checked
bool CalendarTable::MigrateCalendarToVersion3() {
  if (!GetDB().DoesTableExist("calendar")) {
    NOTREACHED() << "Calendar table should exist before migration";
    //return false;
  }

  if (!GetDB().DoesColumnExist("calendar", "last_checked")) {
    // Old versions don't have the last_checked column, we
    // modify the table to add that field.
    if (!GetDB().Execute("ALTER TABLE calendar "
                         "ADD COLUMN last_checked INTEGER DEFAULT 0 not NULL"))
      return false;
  }
  return true;
}

// Updates to version 10. Adds columns supported_component_set
bool CalendarTable::MigrateCalendarToVersion10() {
  if (!GetDB().DoesTableExist("calendar")) {
    NOTREACHED() << "Calendar table should exist before migration";
    //return false;
  }

  if (!GetDB().DoesColumnExist("calendar", "supported_component_set")) {
    if (!GetDB().Execute("ALTER TABLE calendar "
                         "ADD COLUMN supported_component_set INTEGER"))
      return false;
  }
  return true;
}

}  // namespace calendar
