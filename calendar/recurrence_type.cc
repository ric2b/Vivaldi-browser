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

namespace calendar {

RecurrenceEvent::RecurrenceEvent() {}

RecurrenceEvent::RecurrenceEvent(const RecurrenceEvent& event)
    : event_id(event.event_id), reccurence_type(event.reccurence_type) {}

RecurrenceRow::RecurrenceRow() {}

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
