// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/notification_table.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "sql/statement.h"

namespace calendar {

bool NotificationTable::CreateNotificationTable() {
  const char* name = "notifications";
  if (GetDB().DoesTableExist(name))
    return true;

  std::string sql;
  sql.append("CREATE TABLE ");
  sql.append(name);
  sql.append(
      "("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "event_id INTEGER,"
      "name LONGVARCHAR,"
      "description LONGVARCHAR,"
      "when_time INTEGER NOT NULL,"
      "period INTEGER, "
      "delay INTEGER, "
      "created INTEGER,"
      "last_modified INTEGER"
      ")");

  return GetDB().Execute(sql);
}

NotificationID NotificationTable::CreateNotification(
    NotificationRow notification) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "INSERT OR REPLACE into notifications "
                                 "(event_id, name, description, when_time, "
                                 " period, delay, "
                                 " created, last_modified)"
                                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
  int column_index = 0;
  statement.BindInt64(column_index++, notification.event_id);
  statement.BindString16(column_index++, notification.name);
  statement.BindString16(column_index++, notification.description);
  statement.BindInt64(column_index++, notification.when.ToInternalValue());
  statement.BindInt64(column_index++, notification.period.ToInternalValue());
  statement.BindInt(column_index++, notification.delay);

  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());
  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());

  if (!statement.Run()) {
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool NotificationTable::GetAllNotifications(NotificationRows* notifications) {
  notifications->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" CALENDAR_NOTIFICATION_ROW_FIELDS " FROM notifications "));

  while (s.Step()) {
    NotificationRow notification_row;
    FillNotificationRow(s, &notification_row);
    notifications->push_back(notification_row);
  }

  return true;
}
bool NotificationTable::GetAllNotificationsForEvent(
    EventID event_id,
    NotificationRows* notifications) {
  notifications->clear();
  sql::Statement s(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" CALENDAR_NOTIFICATION_ROW_FIELDS
                     " FROM notifications  \
        where event_id = ?"));

  s.BindInt64(0, event_id);

  while (s.Step()) {
    NotificationRow notification_row;
    FillNotificationRow(s, &notification_row);
    notifications->push_back(notification_row);
  }

  return true;
}

bool NotificationTable::UpdateNotificationRow(const NotificationRow& row) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE notifications SET \
        event_id = ?, name = ?,  description = ?, when_time = ?, \
        period = ?, delay = ?, last_modified = ? \
        WHERE id = ?"));
  int column_index = 0;
  statement.BindInt64(column_index++, row.event_id);
  statement.BindString16(column_index++, row.name);
  statement.BindString16(column_index++, row.description);
  statement.BindInt64(column_index++, row.when.ToInternalValue());
  statement.BindInt64(column_index++, row.period.ToInternalValue());
  statement.BindInt64(column_index++, row.delay);
  statement.BindInt64(column_index++, base::Time().Now().ToInternalValue());
  statement.BindInt64(column_index++, row.id);

  return statement.Run();
}

void NotificationTable::FillNotificationRow(sql::Statement& s,
                                            NotificationRow* notification) {
  int column_index = 0;
  NotificationID id = s.ColumnInt64(column_index++);
  EventID event_id = s.ColumnInt64(column_index++);
  std::u16string name = s.ColumnString16(column_index++);
  std::u16string description = s.ColumnString16(column_index++);
  base::Time when =
      base::Time::FromInternalValue(s.ColumnInt64(column_index++));
  base::Time period =
      base::Time::FromInternalValue(s.ColumnInt64(column_index++));
  int delay = s.ColumnInt(column_index++);

  notification->id = id;
  notification->event_id = event_id;
  notification->name = name;
  notification->description = description;
  notification->when = when;
  notification->period = period;
  notification->delay = delay;
}

bool NotificationTable::GetNotificationRow(NotificationID notification_id,
                                           NotificationRow* out_notification) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" CALENDAR_NOTIFICATION_ROW_FIELDS
                     "FROM notifications WHERE id=?"));
  statement.BindInt64(0, notification_id);

  if (!statement.Step())
    return false;

  FillNotificationRow(statement, out_notification);

  return true;
}

bool NotificationTable::DeleteNotification(NotificationID notification_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from notifications WHERE id=?"));
  statement.BindInt64(0, notification_id);

  return statement.Run();
}

bool NotificationTable::DoesNotificationExistForEvent(EventID event_id) {
  sql::Statement statement(
      GetDB().GetUniqueStatement("select count(*) as count from notifications \
        WHERE event_id = ?"));
  statement.BindInt64(0, event_id);

  if (!statement.Step())
    return false;

  return statement.ColumnInt(0) >= 1;
}

bool NotificationTable::DeleteNotificationsForEvent(EventID event_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE from notifications WHERE event_id=?"));
  statement.BindInt64(0, event_id);

  return statement.Run();
}

bool NotificationTable::DeleteNotificationsForCalendar(CalendarID calendar_id) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "DELETE from notifications \
        WHERE event_id IN( \
          select n.event_id from notifications n \
            inner join events e on(e.id = n.event_id) \
            where e.calendar_id = ?)"));

  statement.BindInt64(0, calendar_id);

  return statement.Run();
}

}  // namespace calendar
