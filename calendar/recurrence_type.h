// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_RECURRENCE_TYPE_H_
#define CALENDAR_RECURRENCE_TYPE_H_

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
#include "url/gurl.h"

namespace calendar {

class EventRow;

// Bit flags determing which fields should be updated in the
// UpdateRecurrence method
enum UpdateRecurrenceFields {
  RECURRENCE_EVENT_ID = 1 << 0,
  RECURRENCE_INTERVAL = 1 << 1,
  NUMBER_OF_OCCURRENCES = 1 << 2,
  RECURRENCE_SKIP_COUNT = 1 << 3,
  RECURRENCE_DAY_OF_WEEK = 1 << 4,
  RECURRENCE_WEEK_OF_MONTH = 1 << 5,
  RECURRENCE_DAY_OF_MONTH = 1 << 6,
  RECURRENCE_MONTH_OF_YEAR = 1 << 7,
};

enum class RecurrenceInterval { NONE = 0, DAILY, WEEKLY, MONTHLY, YEARLY };

// Represents a simplified version of a event.
struct EventRecurrence {
  EventRecurrence();
  EventRecurrence(const EventRecurrence& event);
  ~EventRecurrence();
  RecurrenceInterval interval;
  int number_of_occurrences;
  int skip_count;
  int day_of_week;
  int week_of_month;
  int day_of_month;
  int month_of_year;
  int updateFields;
};

// RecurrenceRow
// Holds all information associated with a recurrence row.
class RecurrenceRow {
 public:
  RecurrenceRow();
  RecurrenceRow(RecurrenceID id,
                EventID event_id,
                RecurrenceInterval recurrence_interval,
                int number_of_ocurrences,
                int skip_count,
                int day_of_week,
                int week_of_month,
                int day_of_month,
                int month_of_year);
  ~RecurrenceRow();

  RecurrenceRow(const RecurrenceRow& row);

  RecurrenceID id() const { return id_; }
  void set_id(RecurrenceID id) { id_ = id; }

  EventID event_id() const { return event_id_; }
  void set_event_id(EventID event_id) { event_id_ = event_id; }

  RecurrenceInterval recurrence_interval() const {
    return recurrence_interval_;
  }

  void set_recurrence_interval(RecurrenceInterval recurrence_interval) {
    recurrence_interval_ = recurrence_interval;
  }

  int number_of_ocurrences() const { return number_of_ocurrences_; }
  void set_number_of_ocurrences(int number_of_ocurrences) {
    number_of_ocurrences_ = number_of_ocurrences;
  }

  int skip_count() const { return skip_count_; }
  void set_skip_count(int skip_count) { skip_count_ = skip_count; }

  int day_of_week() const { return day_of_week_; }
  void set_day_of_week(int day_of_week) { day_of_week_ = day_of_week; }

  int week_of_month() const { return week_of_month_; }
  void set_week_of_month(int week_of_month) { week_of_month_ = week_of_month; }

  int day_of_month() const { return day_of_month_; }
  void set_day_of_month(int day_of_month) { day_of_month_ = day_of_month; }

  int month_of_year() const { return month_of_year_; }
  void set_month_of_year(int month_of_year) { month_of_year_ = month_of_year; }

 protected:
  void Swap(RecurrenceRow* other);

  RecurrenceID id_;
  EventID event_id_;
  RecurrenceInterval recurrence_interval_;
  int number_of_ocurrences_;
  int skip_count_;
  int day_of_week_;
  int week_of_month_;
  int day_of_month_;
  int month_of_year_;
};

typedef std::vector<RecurrenceRow> RecurrenceRows;

}  // namespace calendar

#endif  // CALENDAR_RECURRENCE_TYPE_H_
