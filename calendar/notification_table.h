// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_NOTIFICATION_TABLE_H_
#define CALENDAR_NOTIFICATION_TABLE_H_

#include <stddef.h>

#include "calendar/notification_type.h"
#include "sql/statement.h"

namespace sql {
class Database;
}

namespace calendar {

// Encapsulates an SQL database that holds info about notifications.
//
class NotificationTable {
 public:
  // Must call CreateNotificationTable() before using to make
  // sure the database is initialized.
  NotificationTable() = default;

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~NotificationTable() = default;

  NotificationTable(const NotificationTable&) = delete;
  NotificationTable& operator=(const NotificationTable&) = delete;

  bool CreateNotificationTable();

  NotificationID CreateNotification(NotificationRow notification);
  bool GetAllNotifications(NotificationRows* notifications);
  bool GetNotificationRow(NotificationID notification_id,
                          NotificationRow* out_notification);
  bool UpdateNotificationRow(const NotificationRow& notification_id);
  bool DeleteNotification(NotificationID notification_id);
  bool DeleteNotificationsForEvent(EventID event_id);
  bool GetAllNotificationsForEvent(EventID event_id,
                                   NotificationRows* notifications);
  bool DoesNotificationExistForEvent(EventID event_id);
  bool DeleteNotificationsForCalendar(CalendarID calendar_id);

 protected:
  virtual sql::Database& GetDB() = 0;
  void FillNotificationRow(sql::Statement& statement,
                           NotificationRow* notification);
};

// Use the macro if you want to put this in the middle of an otherwise constant
// string, it will save time doing string appends. If you have to build a SQL
// string dynamically anyway, use the constant, it will save space.
#define CALENDAR_NOTIFICATION_ROW_FIELDS \
  " id, event_id, name, description, when_time, period, delay, \
    created, last_modified "

}  // namespace calendar
#endif  // CALENDAR_NOTIFICATION_TABLE_H_
