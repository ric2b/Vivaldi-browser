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
#include "base/strings/string16.h"
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
  STARTRECURRING = 1 << 8,
  ENDRECURRING = 1 << 9,
  LOCATION = 1 << 10,
  URL = 1 << 11,
  ETAG = 1 << 12,
  HREF = 1 << 13,
  UID = 1 << 14,
  EVENT_TYPE_ID = 1 << 15,
  TASK = 1 << 16,
  COMPLETE = 1 << 17,
  TRASH = 1 << 18,
  SEQUENCE = 1 << 19,
  ICAL = 1 << 20,
  RRULE = 1 << 21,
  ORGANIZER = 1 << 22,
  TIMEZONE = 1 << 23,
  DUE = 1 << 24,
  PRIORITY = 1 << 25,
  STATUS = 1 << 26,
  PERCENTAGE_COMPLETE = 1 << 27,
  CATEGORIES = 1 << 28,
  COMPONENT_CLASS = 1 << 29,
  ATTACHMENT = 1 << 30,
  COMPLETED = 1 << 31,
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
  base::string16 title;
  base::string16 description;
  base::Time start;
  base::Time end;
  bool all_day;
  bool is_recurring;
  base::Time start_recurring;
  base::Time end_recurring;
  base::string16 location;
  base::string16 url;
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
  base::string16 ical;
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
  base::string16 categories;
  base::string16 component_class;
  base::string16 attachment;
  base::Time completed;
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

  DISALLOW_COPY_AND_ASSIGN(EventQueryResults);
};

class EventResultCB {
 public:
  EventResultCB() = default;
  bool success;
  std::string message;
  EventResult createdEvent;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventResultCB);
};

class CreateEventsResult {
 public:
  CreateEventsResult() = default;
  int number_failed;
  int number_success;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateEventsResult);
};

class UpdateEventResult {
 public:
  UpdateEventResult() = default;
  bool success;
  std::string message;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateEventResult);
};

class DeleteEventResult {
 public:
  DeleteEventResult();
  bool success;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteEventResult);
};

class EventTypeRow {
 public:
  EventTypeRow() = default;
  ~EventTypeRow() = default;

  EventTypeID id() const { return id_; }
  void set_id(EventTypeID id) { id_ = id; }
  base::string16 name() const { return name_; }
  void set_name(base::string16 name) { name_ = name; }
  std::string color() const { return color_; }
  void set_color(std::string color) { color_ = color; }
  int iconindex() const { return iconindex_; }
  void set_iconindex(int iconindex) { iconindex_ = iconindex; }

  EventTypeID id_;
  base::string16 name_;
  std::string color_;
  int iconindex_;
};

class CreateEventTypeResult {
 public:
  CreateEventTypeResult() = default;
  bool success;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateEventTypeResult);
};

class UpdateEventTypeResult {
 public:
  UpdateEventTypeResult();
  bool success;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateEventTypeResult);
};

typedef std::vector<calendar::EventTypeRow> EventTypeRows;

// Represents a simplified version of a event type.
struct EventType {
  EventType();
  ~EventType();
  EventType(const EventType& event_type);
  EventTypeID event_type_id;
  base::string16 name;
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
  bool success;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteEventTypeResult);
};

}  // namespace calendar

#endif  // CALENDAR_EVENT_TYPE_H_
