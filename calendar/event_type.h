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
#include "calendar/recurrence_type.h"
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
  RECURRENCE = 1 << 12,
};

// Represents a simplified version of a event.
struct CalendarEvent {
  CalendarEvent();
  CalendarEvent(const CalendarEvent& event);
  ~CalendarEvent();
  CalendarID calendar_id;
  AlarmID alarm_id;
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
  EventRecurrence recurrence;
  int updateFields;
};

// EventRow -------------------------------------------------------------------

// Holds all information associated with a specific event.
class EventRow {
 public:
  EventRow();
  EventRow(EventID id,
           CalendarID calendar_id,
           AlarmID alarm_id,
           base::string16 title,
           base::string16 description,
           base::Time start,
           base::Time end,
           bool all_day,
           bool is_recurring,
           base::Time start_recurring,
           base::Time end_recurring,
           base::string16 location,
           base::string16 url,
           EventRecurrence recurrence);
  ~EventRow();

  EventRow(const EventRow& row);

  EventRow(const EventRow&&) noexcept;

  EventRow& operator=(const EventRow& other);

  EventID id() const { return id_; }
  void set_id(EventID id) { id_ = id; }

  CalendarID calendar_id() const { return calendar_id_; }
  void set_calendar_id(CalendarID calendar_id) { calendar_id_ = calendar_id; }

  AlarmID alarm_id() const { return alarm_id_; }
  void set_alarm_id(AlarmID alarm_id) { alarm_id_ = alarm_id; }

  base::string16 title() const { return title_; }
  void set_title(base::string16 title) { title_ = title; }

  base::string16 description() const { return description_; }
  void set_description(base::string16 description) {
    description_ = description;
  }

  base::Time start() const { return start_; }
  void set_start(base::Time start) { start_ = start; }

  base::Time end() const { return end_; }
  void set_end(base::Time end) { end_ = end; }

  bool all_day() const { return all_day_; }
  void set_all_day(bool all_day) { all_day_ = all_day; }

  bool is_recurring() const { return is_recurring_; }
  void set_is_recurring(bool is_recurring) { is_recurring_ = is_recurring; }

  base::Time start_recurring() const { return start_recurring_; }
  void set_start_recurring(base::Time start_recurring) {
    start_recurring_ = start_recurring;
  }

  base::Time end_recurring() const { return end_recurring_; }
  void set_end_recurring(base::Time end_recurring) {
    end_recurring_ = end_recurring;
  }

  base::string16 location() const { return location_; }
  void set_location(base::string16 location) { location_ = location; }

  base::string16 url() const { return url_; }
  void set_url(base::string16 url) { url_ = url; }

  EventRecurrence recurrence() const { return recurrence_; }
  void set_recurrence(EventRecurrence recurrence) { recurrence_ = recurrence; }

  EventID id_;
  CalendarID calendar_id_;
  AlarmID alarm_id_;
  base::string16 title_;
  base::string16 description_;
  base::Time start_;
  base::Time end_;
  bool all_day_;
  bool is_recurring_;
  base::Time start_recurring_;
  base::Time end_recurring_;
  base::string16 location_;
  base::string16 url_;
  EventRecurrence recurrence_;

 protected:
  void Swap(EventRow* other);
};

typedef std::vector<EventRow> EventRows;

class EventResult : public EventRow {
 public:
  EventResult();
  EventResult(const EventRow& event_row);
  EventResult(const EventResult& other);
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

class CreateEventResult {
 public:
  CreateEventResult();
  bool success;
  std::string message;
  EventRow createdRow;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateEventResult);
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
  UpdateEventResult();
  bool success;

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

}  // namespace calendar

#endif  // CALENDAR_EVENT_TYPE_H_
