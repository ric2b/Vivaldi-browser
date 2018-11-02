// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/recurrence_type.h"

#include <limits>
#include "base/stl_util.h"
#include "calendar/event_type.h"

namespace calendar {

EventRecurrence::EventRecurrence()
    : interval(RecurrenceInterval::NONE),
      number_of_occurrences(0),
      skip_count(0),
      day_of_week(0),
      week_of_month(0),
      day_of_month(0),
      month_of_year(0),
      updateFields(0) {}

EventRecurrence::EventRecurrence(const EventRecurrence& event_recurrence)
    : interval(event_recurrence.interval),
      number_of_occurrences(event_recurrence.number_of_occurrences),
      skip_count(event_recurrence.skip_count),
      day_of_week(event_recurrence.day_of_week),
      week_of_month(event_recurrence.week_of_month),
      day_of_month(event_recurrence.day_of_month),
      month_of_year(event_recurrence.month_of_year),
      updateFields(event_recurrence.updateFields) {}

EventRecurrence::~EventRecurrence() {}

RecurrenceRow::RecurrenceRow()
    : id_(0),
      event_id_(0),
      recurrence_interval_(RecurrenceInterval::NONE),
      number_of_ocurrences_(0),
      skip_count_(0),
      day_of_week_(0),
      week_of_month_(0),
      day_of_month_(0),
      month_of_year_(0) {}

RecurrenceRow::RecurrenceRow(RecurrenceID id,
                             EventID event_id,
                             RecurrenceInterval recurrence_interval,
                             int number_of_ocurrences,
                             int skip_count,
                             int day_of_week,
                             int week_of_month,
                             int day_of_month,
                             int month_of_year)
    : id_(id),
      event_id_(event_id),
      recurrence_interval_(recurrence_interval),
      number_of_ocurrences_(number_of_ocurrences),
      skip_count_(skip_count),
      day_of_week_(day_of_week),
      week_of_month_(week_of_month),
      day_of_month_(day_of_month),
      month_of_year_(month_of_year) {}

RecurrenceRow::~RecurrenceRow() {}

RecurrenceRow::RecurrenceRow(const RecurrenceRow& other)
    : id_(other.id_),
      event_id_(other.event_id_),
      recurrence_interval_(other.recurrence_interval_),
      number_of_ocurrences_(other.number_of_ocurrences_),
      skip_count_(other.skip_count_),
      day_of_week_(other.day_of_week_),
      week_of_month_(other.week_of_month_),
      day_of_month_(other.day_of_month_),
      month_of_year_(other.month_of_year_) {}
}  // namespace calendar
