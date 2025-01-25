// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/recurrence_exception_table.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "sql/statement.h"

namespace calendar {

bool RecurrrenceExceptionTable ::CreateRecurringExceptionTable() {
  const char* name = "recurring_exceptions";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "parent_event_id INTEGER NOT NULL,"
      "exception_event_id INTEGER NOT NULL,"
      "exception_day INTEGER,"
      "cancelled INTEGER,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  return GetDB().Execute(sql);
}

RecurrenceExceptionID RecurrrenceExceptionTable ::CreateRecurrenceException(
    RecurrenceExceptionRow row) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE into recurring_exceptions "
      "(parent_event_id, exception_event_id, exception_day, cancelled, "
      " created, last_modified)"
      "VALUES (?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, row.parent_event_id);
  statement.BindInt64(1, row.exception_event_id);
  statement.BindInt64(2, row.exception_day.ToInternalValue());
  statement.BindInt(3, row.cancelled ? 1 : 0);

  statement.BindInt64(4, base::Time().Now().ToInternalValue());
  statement.BindInt64(5, base::Time().Now().ToInternalValue());

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool RecurrrenceExceptionTable ::GetAllRecurrenceExceptions(
    RecurrenceExceptionRows* recurrences) {
  recurrences->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" CALENDAR_RECURRING_EXCEPTION_ROW_FIELDS
                     " FROM recurring_exceptions "));

  while (s.Step()) {
    RecurrenceExceptionRow recurrence_exception;
    FillRecurrenceExceptionRow(s, &recurrence_exception);
    recurrences->push_back(recurrence_exception);
  }

  return true;
}

bool RecurrrenceExceptionTable ::GetAllRecurrenceExceptionsForEvent(
    EventID event_id,
    RecurrenceExceptionRows* recurrences) {
  recurrences->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" CALENDAR_RECURRING_EXCEPTION_ROW_FIELDS
                     " FROM recurring_exceptions  \
        where parent_event_id = ?"));

  s.BindInt64(0, event_id);

  while (s.Step()) {
    RecurrenceExceptionRow recurrence_exception;
    FillRecurrenceExceptionRow(s, &recurrence_exception);
    recurrences->push_back(recurrence_exception);
  }

  return true;
}

bool RecurrrenceExceptionTable ::GetRecurrenceException(
    RecurrenceExceptionID exception_id,
    RecurrenceExceptionRow* recurrence_exception) {
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" CALENDAR_RECURRING_EXCEPTION_ROW_FIELDS
                     " FROM recurring_exceptions  \
        where id = ?"));

  s.BindInt64(0, exception_id);

  if (!s.Step())
    return false;

  FillRecurrenceExceptionRow(s, recurrence_exception);

  return true;
}

bool RecurrrenceExceptionTable ::UpdateRecurrenceExceptionRow(
    const RecurrenceExceptionRow& rec_ex) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE recurring_exceptions SET \
        parent_event_id = ?, exception_event_id = ?, exception_day = ?, \
        cancelled = ?, last_modified = ? \
        WHERE id = ?"));

  statement.BindInt64(0, rec_ex.parent_event_id);
  statement.BindInt64(1, rec_ex.exception_event_id);
  statement.BindInt64(2, rec_ex.exception_day.ToInternalValue());
  statement.BindInt(3, rec_ex.cancelled ? 1 : 0);
  statement.BindInt64(4, base::Time().Now().ToInternalValue());

  statement.BindInt64(5, rec_ex.id);
  return statement.Run();
}

void RecurrrenceExceptionTable::FillRecurrenceExceptionRow(
    sql::Statement& s,
    RecurrenceExceptionRow* recurrenceRow) {
  RecurrenceExceptionID id = s.ColumnInt64(0);
  EventID parent_event_id = s.ColumnInt64(1);
  EventID exception_event_id = s.ColumnInt64(2);
  base::Time exception_day = base::Time::FromInternalValue(s.ColumnInt64(3));
  bool cancelled = s.ColumnInt(4) == 1 ? true : false;

  recurrenceRow->id = id;
  recurrenceRow->parent_event_id = parent_event_id;
  recurrenceRow->exception_event_id = exception_event_id;
  recurrenceRow->exception_day = exception_day;
  recurrenceRow->cancelled = cancelled;
}

bool RecurrrenceExceptionTable ::DeleteRecurrenceException(
    RecurrenceExceptionID exception_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from recurring_exceptions WHERE id=?"));
  statement.BindInt64(0, exception_id);

  return statement.Run();
}
bool RecurrrenceExceptionTable::DeleteRecurrenceExceptionsForCalendar(
    CalendarID calendar_id) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "DELETE from recurring_exceptions \
        WHERE id IN( \
          select re.id from recurring_exceptions re \
            inner join events e on(e.id = re.parent_event_id) \
            where e.calendar_id = ?)"));

  statement.BindInt64(0, calendar_id);

  return statement.Run();
}

bool RecurrrenceExceptionTable::DeleteRecurrenceExceptions(EventID event_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE from recurring_exceptions WHERE parent_event_id=?"));
  statement.BindInt64(0, event_id);

  return statement.Run();
}

bool RecurrrenceExceptionTable::DoesRecurrenceExceptionExistForEvent(
    EventID event_id) {
  sql::Statement statement(GetDB().GetUniqueStatement(
      "select count(*) as count from recurring_exceptions \
        WHERE parent_event_id = ?"));
  statement.BindInt64(0, event_id);

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) >= 1;
}

bool RecurrrenceExceptionTable::GetAllEventExceptionIds(EventID event_id,
                                                        EventIDs* event_ids) {
  event_ids->clear();
  sql::Statement s(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                              "SELECT exception_event_id \
     FROM recurring_exceptions  \
        where parent_event_id = ?"));

  s.BindInt64(0, event_id);

  while (s.Step()) {
    EventID id = s.ColumnInt64(0);
    event_ids->push_back(id);
  }

  return true;
}

EventID RecurrrenceExceptionTable::GetParentExceptionEventId(
    EventID exception_event_id) {
  sql::Statement statement(
      GetDB().GetUniqueStatement(" SELECT parent_event_id  \
     FROM recurring_exceptions  \
        where exception_event_id = ?"));
  statement.BindInt64(0, exception_event_id);

  if (!statement.Step())
    return 0;

  return statement.ColumnInt64(0);
}

}  // namespace calendar
