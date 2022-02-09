// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_RECURRENCE_EXCEPTION_TYPE_H_
#define CALENDAR_RECURRENCE_EXCEPTION_TYPE_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "calendar/calendar_typedefs.h"

namespace calendar {

// RecurrenceExceptionRow
// Holds all information associated with a recurrence exception row.
class RecurrenceExceptionRow {
 public:
  RecurrenceExceptionRow() = default;
  ~RecurrenceExceptionRow() = default;

  RecurrenceExceptionID id;
  EventID parent_event_id;
  EventID exception_event_id;
  bool cancelled;
  base::Time exception_day;
};

typedef std::vector<RecurrenceExceptionRow> RecurrenceExceptionRows;

class CreateRecurrenceExceptionResult {
 public:
  CreateRecurrenceExceptionResult() = default;
  CreateRecurrenceExceptionResult(const CreateRecurrenceExceptionResult&) =
      delete;
  CreateRecurrenceExceptionResult& operator=(
      const CreateRecurrenceExceptionResult&) = delete;

  bool success;
  std::string message;
  RecurrenceExceptionRow createdRow;
};

}  // namespace calendar

#endif  // CALENDAR_RECURRENCE_EXCEPTION_TYPE_H_
