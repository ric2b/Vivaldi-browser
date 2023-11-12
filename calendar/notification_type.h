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
#include <vector>

#include "calendar/calendar_typedefs.h"

#include "base/time/time.h"

namespace calendar {

// Bit flags determing which fields should be updated in the
// UpdateNotification API method
enum UpdateNotificationFields {
  NOTIFICATION_NAME = 1 << 0,
  NOTIFICATION_WHEN = 1 << 1,
  NOTIFICATION_PERIOD = 1 << 2,
  NOTIFICATION_DELAY = 1 << 3,
  NOTIFICATION_DESCRIPTION = 1 << 4,
};

// Simplified notification. Used when notification has to be created during
// event create
struct NotificationToCreate {
  std::u16string name;
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
  std::u16string name;
  std::u16string description;
  base::Time period;
  int delay;
  base::Time when;
};

class UpdateNotificationRow {
 public:
  UpdateNotificationRow() = default;
  ~UpdateNotificationRow() = default;
  UpdateNotificationRow(const UpdateNotificationRow& notification) = default;
  UpdateNotificationRow& operator=(UpdateNotificationRow& notification) =
      default;

  NotificationRow notification_row;
  int updateFields = 0;
};

typedef std::vector<NotificationRow> NotificationRows;
typedef std::vector<NotificationToCreate> NotificationsToCreate;

class NotificationResult {
 public:
  NotificationResult() = default;
  NotificationResult(const NotificationResult&) = default;
  NotificationResult& operator=(const NotificationResult&) = default;

  bool success;
  std::string message;
  NotificationRow notification_row;
};

class GetAllNotificationResult {
 public:
  GetAllNotificationResult();
  ~GetAllNotificationResult();
  GetAllNotificationResult(const GetAllNotificationResult&);

  bool success;
  std::string message;
  NotificationRows notifications;
};

class DeleteNotificationResult {
 public:
  DeleteNotificationResult() = default;
  DeleteNotificationResult(const DeleteNotificationResult&) = delete;
  DeleteNotificationResult& operator=(const DeleteNotificationResult&) = delete;

  bool success;
};

}  // namespace calendar

#endif  // CALENDAR_NOTIFICATION_TYPE_H_
