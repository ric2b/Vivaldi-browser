// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_CALENDAR_TYPES_H_
#define CALENDAR_CALENDAR_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace calendar {

typedef int64_t EventID;

// Bit flags determing which fields should be updated in the
// UpdateEvent method
enum UpdateEventFields {
  CALENDAR_ID = 1 << 0,
  TITLE = 1 << 1,
  DESCRIPTION = 1 << 2,
  START = 1 << 3,
  END = 1 << 4,
};

// Represents a simplified version of a event.
struct CalendarEvent {
  CalendarEvent();
  CalendarEvent(const CalendarEvent& event);

  base::string16 calendar_id;
  base::string16 title;
  base::string16 description;
  base::Time start;
  base::Time end;
  int updateFields;
};

// EventRow -------------------------------------------------------------------

// Holds all information associated with a specific event.
class EventRow {
 public:
  EventRow();
  EventRow(base::string16 id,
           base::string16 calendar_id,
           base::string16 title,
           base::string16 description,
           base::Time start,
           base::Time end);
  ~EventRow();

  EventRow(const EventRow& row);

  base::string16 id() const { return id_; }
  void set_id(base::string16 id) { id_ = id; }

  base::string16 calendar_id() const { return calendar_id_; }
  void set_calendar_id(base::string16 calendar_id) {
    calendar_id_ = calendar_id;
  }

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

 protected:
  void Swap(EventRow* other);

  base::string16 id_;
  base::string16 calendar_id_;
  base::string16 title_;
  base::string16 description_;
  base::Time start_;
  base::Time end_;
};

typedef std::vector<EventRow> EventRows;

class EventResult : public EventRow {
 public:
  EventResult();
  EventResult(base::string16 id,
              base::string16 title,
              base::string16 description);
  EventResult(const EventRow& event_row);
  EventResult(const EventResult& other);
  ~EventResult();

  base::string16 id() const { return id_; }
  void set_id(base::string16 id) { id_ = id; }

  base::string16 calendar_id() const { return calendar_id_; }
  void set_calendar_id(base::string16 calendar_id) {
    calendar_id_ = calendar_id;
  }

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

  void SwapResult(EventResult* other);

 private:
  base::string16 id_;
  base::string16 calendar_id_;
  base::string16 title_;
  base::string16 description_;
};

class EventQueryResults {
 public:
  typedef std::vector<EventResult*> EventResultVector;

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
  ScopedVector<EventResult> results_;

  DISALLOW_COPY_AND_ASSIGN(EventQueryResults);
};

class UpdateEventResult {
 public:
  UpdateEventResult();
  bool success;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateEventResult);
};

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_TYPES_H_
