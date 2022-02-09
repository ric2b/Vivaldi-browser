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
#include "calendar/event_exception_type.h"
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
  DUE = 1 << 22,
  PRIORITY = 1 << 23,
  STATUS = 1 << 24,
  PERCENTAGE_COMPLETE = 1 << 25,
  CATEGORIES = 1 << 26,
  COMPONENT_CLASS = 1 << 27,
  ATTACHMENT = 1 << 28,
  COMPLETED = 1 << 29,
  SYNC_PENDING = 1 << 30,
  DELETE_PENDING = 1 << 31,
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
  EventExceptions event_exceptions;
  std::string rrule;
  NotificationRows notifications;
  InviteRows invites;
  std::string organizer;
  NotificationsToCreate notifications_to_create;
  InvitesToCreate invites_to_create;
  std::string timezone;
  bool is_template;
  base::Time due;
  int priority;
  std::string status;
  int percentage_complete;
  std::u16string categories;
  std::u16string component_class;
  std::u16string attachment;
  base::Time completed;
  bool sync_pending;
  bool delete_pending;
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

  void SwapResult(EventResult* other);
};

class EventQueryResults {
 public:
  typedef std::vector<std::unique_ptr<EventResult>> EventResultVector;

  EventQueryResults();
  ~EventQueryResults();
  EventQueryResults(const EventQueryResults&) = delete;
  EventQueryResults& operator=(const EventQueryResults&) = delete;

  size_t size() const { return results_.size(); }
  bool empty() const { return results_.empty(); }

  EventResult& back() { return *results_.back(); }
  const EventResult& back() const { return *results_.back(); }

  EventResult& operator[](size_t i) { return *results_[i]; }
  const EventResult& operator[](size_t i) const { return *results_[i]; }

  EventResultVector::const_iterator begin() const { return results_.begin(); }
  EventResultVector::const_iterator end() const { return results_.end(); }
  EventResultVector::const_reverse_iterator rbegin() const {
    return results_.rbegin();
  }
  EventResultVector::const_reverse_iterator rend() const {
    return results_.rend();
  }

  // Swaps the current result with another. This allows ownership to be
  // efficiently transferred without copying.
  void Swap(EventQueryResults* other);

  // Adds the given result to the map, using swap() on the members to avoid
  // copying (there are a lot of strings and vectors). This means the parameter
  // object will be cleared after this call.
  void AppendEventBySwapping(EventResult* result);

 private:
  // The ordered list of results. The pointers inside this are owned by this
  // QueryResults object.
  EventResultVector results_;
};

class EventResultCB {
 public:
  EventResultCB() = default;
  EventResultCB(const EventResultCB&) = delete;
  EventResultCB& operator=(const EventResultCB&) = delete;

  bool success;
  std::string message;
  EventResult event;
};

class CreateEventsResult {
 public:
  CreateEventsResult() = default;
  CreateEventsResult(const CreateEventsResult&) = delete;
  CreateEventsResult& operator=(const CreateEventsResult&) = delete;

  int number_failed;
  int number_success;
};

class DeleteEventResult {
 public:
  DeleteEventResult();
  DeleteEventResult(const DeleteEventResult&) = delete;
  DeleteEventResult& operator=(const DeleteEventResult&) = delete;

  bool success;
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

class CreateEventTypeResult {
 public:
  CreateEventTypeResult() = default;
  CreateEventTypeResult(const CreateEventTypeResult&) = delete;
  CreateEventTypeResult& operator=(const CreateEventTypeResult&) = delete;

  bool success;
};

class UpdateEventTypeResult {
 public:
  UpdateEventTypeResult();
  UpdateEventTypeResult(const UpdateEventTypeResult&) = delete;
  UpdateEventTypeResult& operator=(const UpdateEventTypeResult&) = delete;

  bool success;
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

class DeleteEventTypeResult {
 public:
  DeleteEventTypeResult() = default;
  DeleteEventTypeResult(const DeleteEventTypeResult&) = delete;
  DeleteEventTypeResult& operator=(const DeleteEventTypeResult&) = delete;

  bool success;
};

}  // namespace calendar

#endif  // CALENDAR_EVENT_TYPE_H_
