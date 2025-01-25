// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_EVENT_TYPE_H_
#define CALENDAR_EVENT_TYPE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "calendar/calendar_typedefs.h"
#include "calendar/invite_type.h"
#include "calendar/notification_type.h"
#include "calendar/recurrence_exception_type.h"
#include "calendar_type.h"
#include "url/gurl.h"

namespace calendar {

// Bit flags determing which fields should be updated in the
// UpdateEvent method
enum UpdateEventFields {
  CALENDAR_ID = 1 << 0,
  ALARM_ID = 1 << 1,
  TITLE = 1 << 2,
  DESCRIPTION = 1 << 3,
  START = 1 << 4,
  END = 1 << 5,
  ALLDAY = 1 << 6,
  ISRECURRING = 1 << 7,
  LOCATION = 1 << 8,
  URL = 1 << 9,
  ETAG = 1 << 10,
  HREF = 1 << 11,
  UID = 1 << 12,
  EVENT_TYPE_ID = 1 << 13,
  TASK = 1 << 14,
  COMPLETE = 1 << 15,
  TRASH = 1 << 16,
  SEQUENCE = 1 << 17,
  ICAL = 1 << 18,
  RRULE = 1 << 19,
  ORGANIZER = 1 << 20,
  TIMEZONE = 1 << 21,
  PRIORITY = 1 << 22,
  STATUS = 1 << 23,
  PERCENTAGE_COMPLETE = 1 << 24,
  CATEGORIES = 1 << 25,
  COMPONENT_CLASS = 1 << 26,
  ATTACHMENT = 1 << 27,
  COMPLETED = 1 << 28,
  SYNC_PENDING = 1 << 29,
  DELETE_PENDING = 1 << 30,
  END_RECURRING = 1 << 31,
};

// EventRow -------------------------------------------------------------------

// Holds all information associated with a specific event.
class EventRow {
 public:
  EventRow();
  ~EventRow();

  EventRow(const EventRow& row);

  EventRow(const EventRow&&) noexcept;

  EventRow& operator=(const EventRow& other);

  EventID id;
  CalendarID calendar_id;
  AlarmID alarm_id = 0;
  std::u16string title;
  std::u16string description;
  base::Time start;
  base::Time end;
  bool all_day;
  bool is_recurring;
  std::u16string location;
  std::u16string url;
  RecurrenceExceptionRows recurrence_exceptions;
  std::string etag;
  std::string href;
  std::string uid;
  EventTypeID event_type_id = 0;
  bool task;
  bool complete;
  bool trash = false;
  base::Time trash_time;
  int sequence;
  std::u16string ical;
  RecurrenceExceptionRows event_exceptions;
  std::string rrule;
  NotificationRows notifications;
  InviteRows invites;
  std::string organizer;
  NotificationsToCreate notifications_to_create;
  InvitesToCreate invites_to_create;
  std::string timezone;
  bool is_template;
  int priority;
  std::string status;
  int percentage_complete;
  std::u16string categories;
  std::u16string component_class;
  std::u16string attachment;
  base::Time completed;
  bool sync_pending;
  bool delete_pending;
  base::Time end_recurring;
  int updateFields;

 protected:
  void Swap(EventRow* other);
};

typedef std::vector<EventRow> EventRows;
typedef std::vector<EventID> EventIDs;

class EventResult : public EventRow {
 public:
  EventResult();
  EventResult(const EventRow& event_row);
  ~EventResult();
};

class EventResultCB {
 public:
  EventResultCB() = default;

  bool success;
  std::string message;
  EventResult event;
};

class StatusCB {
 public:
  StatusCB() = default;

  bool success;
  std::string message;
};

class CreateEventsResult {
 public:
  CreateEventsResult() = default;
  int number_failed;
  int number_success;
};

class EventTypeRow {
 public:
  EventTypeRow() = default;
  ~EventTypeRow() = default;

  EventTypeID id() const { return id_; }
  void set_id(EventTypeID id) { id_ = id; }
  std::u16string name() const { return name_; }
  void set_name(std::u16string name) { name_ = name; }
  std::string color() const { return color_; }
  void set_color(std::string color) { color_ = color; }
  int iconindex() const { return iconindex_; }
  void set_iconindex(int iconindex) { iconindex_ = iconindex; }

  EventTypeID id_;
  std::u16string name_;
  std::string color_;
  int iconindex_;
};

typedef std::vector<calendar::EventTypeRow> EventTypeRows;

// Represents a simplified version of a event type.
struct EventType {
  EventType();
  ~EventType();
  EventType(const EventType& event_type);
  EventTypeID event_type_id;
  std::u16string name;
  std::string color = "";
  int iconindex = 0;
  int updateFields;
};

// Bit flags determing which fields should be updated in the
// UpdateEventType method
enum UpdateEventTypeFields {
  CALENDAR_TYPE_ID = 1 << 0,
  NAME = 1 << 2,
  COLOR = 1 << 3,
  ICONINDEX = 1 << 4,
};

}  // namespace calendar

#endif  // CALENDAR_EVENT_TYPE_H_
