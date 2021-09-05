// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_NOTIFICATION_TYPE_H_
#define CALENDAR_NOTIFICATION_TYPE_H_

#include <string>

#include "calendar/calendar_typedefs.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"

namespace calendar {

// Simplified notification. Used when notification has to be created during
// event create
struct NotificationToCreate {
  base::string16 name;
  base::Time when;
};

// NotificationRow
// Holds all information associated with a notification row.
class NotificationRow {
 public:
  NotificationRow();

  NotificationRow(const NotificationRow& other);
  ~NotificationRow() = default;

  NotificationID id;
  EventID event_id;
  base::string16 name;
  base::string16 description;
  int period;
  int delay;
  base::Time when;
};

typedef std::vector<NotificationRow> NotificationRows;
typedef std::vector<NotificationToCreate> NotificationsToCreate;

class CreateNotificationResult {
 public:
  CreateNotificationResult() = default;
  bool success;
  std::string message;
  NotificationRow createdRow;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateNotificationResult);
};

class GetAllNotificationResult {
 public:
  GetAllNotificationResult();
  ~GetAllNotificationResult();
  bool success;
  std::string message;
  NotificationRows notifications;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetAllNotificationResult);
};

class DeleteNotificationResult {
 public:
  DeleteNotificationResult() = default;
  bool success;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteNotificationResult);
};

}  // namespace calendar

#endif  // CALENDAR_NOTIFICATION_TYPE_H_
