// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_EVENT_EXCEPTION_TYPE_H_
#define CALENDAR_EVENT_EXCEPTION_TYPE_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"

namespace calendar {

class EventRow;

// EventException
// Holds all information associated with a event exception.
// Used during event creation
class EventExceptionType {
 public:
  EventExceptionType();
  ~EventExceptionType();
  EventExceptionType(const EventExceptionType& other);

  std::shared_ptr<EventRow> event;
  base::Time exception_date;
  bool cancelled;
};

typedef std::vector<EventExceptionType> EventExceptions;

}  // namespace calendar

#endif  //  CALENDAR_EVENT_EXCEPTION_TYPE_H_
