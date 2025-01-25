// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/event_template_table.h"

#include <string>
#include <vector>

#include "calendar/event_template_type.h"
#include "sql/statement.h"

namespace calendar {

bool EventTemplateTable::CreateEventTemplateTable() {
  const char* name = "event_templates";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "name LONGVARCHAR,"
      "ical LONGVARCHAR,"
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  return GetDB().Execute(sql);
}

EventTemplateID EventTemplateTable::CreateEventTemplate(
    calendar::EventTemplateRow event_template) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT INTO event_templates "
                                 "(name, ical, "
                                 "created, last_modified) "
                                 "VALUES (?, ?, ?, ?)"));

  int column_index = 0;
  statement.BindString16(column_index++, event_template.name);
  statement.BindString16(column_index++, event_template.ical);
  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());
  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool EventTemplateTable::GetAllEventTemplates(EventTemplateRows* events) {
  events->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" CALENDAR_EVENT_TEMPLATE_ROW_FIELDS " FROM event_templates "));

  while (s.Step()) {
    EventTemplateRow event;
    FillEventTemplate(s, &event);
    events->push_back(event);
  }

  return true;
}

bool EventTemplateTable::GetRowForEventTemplate(
    calendar::EventTemplateID event_id,
    EventTemplateRow* out_event) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" CALENDAR_EVENT_TEMPLATE_ROW_FIELDS
                     "FROM event_templates WHERE id = ?"));
  statement.BindInt64(0, event_id);

  if (!statement.Step())
    return false;

  FillEventTemplate(statement, out_event);

  return true;
}

bool EventTemplateTable::UpdateEventTemplate(
    const EventTemplateRow& event_template) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE event_templates SET \
        name = ?, ical = ?, last_modified = ? \
        WHERE id = ?"));
  int column_index = 0;

  statement.BindString16(column_index++, event_template.name);
  statement.BindString16(column_index++, event_template.ical);
  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());

  statement.BindInt64(column_index++, event_template.id);

  return statement.Run();
}

// Must be in sync with CALENDAR_EVENT_TEMPLATE_ROW_FIELDS.
// static
void EventTemplateTable::FillEventTemplate(sql::Statement& s,
                                           EventTemplateRow* event_template) {
  int column_index = 0;
  EventTemplateID id = s.ColumnInt64(column_index++);
  std::u16string name = s.ColumnString16(column_index++);
  std::u16string ical = s.ColumnString16(column_index++);

  event_template->id = id;
  event_template->name = name;
  event_template->ical = ical;
}

bool EventTemplateTable::DeleteEventTemplate(
    calendar::EventTemplateID event_template_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from event_templates WHERE id = ?"));
  statement.BindInt64(0, event_template_id);

  return statement.Run();
}

}  // namespace calendar
