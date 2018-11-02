// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/recurrence_table.h"

#include <string>
#include <vector>
#include "base/strings/utf_string_conversions.h"
#include "sql/statement.h"

namespace calendar {

RecurrrenceTable::RecurrrenceTable() {}

RecurrrenceTable::~RecurrrenceTable() {}

bool RecurrrenceTable::CreateRecurringTable() {
  const char* name = "recurring_events";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "event_id INTEGER NOT NULL UNIQUE,"
      "interval VARCHAR NOT NULL,"  // daily, weekly, monthly, yearly
      "number_of_ocurrences INTEGER,"
      "skip_count INTEGER,"
      "day_of_week INTEGER,"
      "week_of_month INTEGER,"
      "day_of_month INTEGER,"
      "month_of_year INTGER,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  return GetDB().Execute(sql.c_str());
}

RecurrenceID RecurrrenceTable::CreateRecurrenceEvent(RecurrenceRow row) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE into recurring_events "
      "(event_id, interval, number_of_ocurrences, skip_count, "
      "day_of_week, "
      " week_of_month, day_of_month, month_of_year) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

  std::string interval = "";

  switch (row.recurrence_interval()) {
    case RecurrenceInterval::DAILY:
      interval = "days";
      break;

    case RecurrenceInterval::WEEKLY:
      interval = "weeks";
      break;

    case RecurrenceInterval::MONTHLY:
      interval = "months";
      break;

    case RecurrenceInterval::YEARLY:
      interval = "years";
      break;

    default:
      interval = "none";
  }

  statement.BindInt64(0, row.event_id());
  statement.BindString(1, interval);
  statement.BindInt(2, row.number_of_ocurrences());
  statement.BindInt(3, row.skip_count());
  statement.BindInt(4, row.day_of_week());
  statement.BindInt(5, row.week_of_month());
  statement.BindInt(6, row.day_of_month());
  statement.BindInt(7, row.month_of_year());

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool RecurrrenceTable::GetAllRecurrences(RecurrenceRows* recurrences) {
  recurrences->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" CALENDAR_RECURRING_ROW_FIELDS " FROM recurring_events "));

  while (s.Step()) {
    RecurrenceRow recurrence;
    FillRecurrenceRow(s, &recurrence);
    recurrences->push_back(recurrence);
  }

  return true;
}

bool RecurrrenceTable::GetRecurrenceRow(EventID event_id,
                                        RecurrenceRow* out_recurrence) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" CALENDAR_RECURRING_ROW_FIELDS
                     "FROM recurring_events WHERE event_id=?"));
  statement.BindInt64(0, event_id);

  if (!statement.Step())
    return false;

  FillRecurrenceRow(statement, out_recurrence);

  return true;
}

bool RecurrrenceTable::UpdateRecurrenceRow(const RecurrenceRow& recurrence) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE recurring_events SET \
        event_id=?, interval=?, number_of_ocurrences=?, skip_count=?, "
      "day_of_week=?, week_of_month=?, day_of_month=?, month_of_year=? \
        WHERE id=?"));

  std::string interval = "";

  switch (recurrence.recurrence_interval()) {
    case RecurrenceInterval::DAILY:
      interval = "days";
      break;

    case RecurrenceInterval::WEEKLY:
      interval = "weeks";
      break;

    case RecurrenceInterval::MONTHLY:
      interval = "months";
      break;

    case RecurrenceInterval::YEARLY:
      interval = "years";
      break;

    default:
      interval = "";
  }

  statement.BindInt64(0, recurrence.event_id());
  statement.BindString(1, interval);
  statement.BindInt(2, recurrence.number_of_ocurrences());
  statement.BindInt(3, recurrence.skip_count());
  statement.BindInt(4, recurrence.day_of_week());
  statement.BindInt(5, recurrence.week_of_month());
  statement.BindInt(6, recurrence.day_of_month());
  statement.BindInt(7, recurrence.month_of_year());
  statement.BindInt(8, recurrence.id());

  return statement.Run();
}

void RecurrrenceTable::FillRecurrenceRow(const sql::Statement& statement,
                                         RecurrenceRow* recurrenceRow) {
  int64_t id = statement.ColumnInt64(0);
  int64_t event_id = statement.ColumnInt64(1);
  std::string recurrence_interval = statement.ColumnString(2);
  int number_of_ocurrences = statement.ColumnInt(3);
  int skip_count = statement.ColumnInt(4);
  int day_of_week = statement.ColumnInt(5);
  int week_of_month = statement.ColumnInt(6);
  int day_of_month = statement.ColumnInt(7);
  int month_of_year = statement.ColumnInt(8);

  RecurrenceInterval interval = RecurrenceInterval::NONE;
  if (recurrence_interval == "days") {
    interval = RecurrenceInterval::DAILY;
  } else if (recurrence_interval == "weeks") {
    interval = RecurrenceInterval::WEEKLY;
  } else if (recurrence_interval == "months") {
    interval = RecurrenceInterval::MONTHLY;
  } else if (recurrence_interval == "years") {
    interval = RecurrenceInterval::YEARLY;
  }

  recurrenceRow->set_id(id);
  recurrenceRow->set_event_id(event_id);
  recurrenceRow->set_recurrence_interval(interval);
  recurrenceRow->set_number_of_ocurrences(number_of_ocurrences);
  recurrenceRow->set_skip_count(skip_count);
  recurrenceRow->set_day_of_week(day_of_week);
  recurrenceRow->set_week_of_month(week_of_month);
  recurrenceRow->set_day_of_month(day_of_month);
  recurrenceRow->set_month_of_year(month_of_year);
}

bool RecurrrenceTable::DeleteRecurrence(RecurrenceID recurrence_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from recurring_events WHERE id=?"));
  statement.BindInt64(0, recurrence_id);

  return statement.Run();
}

}  // namespace calendar
