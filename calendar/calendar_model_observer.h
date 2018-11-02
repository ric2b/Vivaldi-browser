// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_CALENDAR_MODEL_OBSERVER_H_
#define CALENDAR_CALENDAR_MODEL_OBSERVER_H_

#include "calendar/calendar_type.h"
#include "calendar/event_type.h"

namespace calendar {

class CalendarService;

// Observer for the Calendar Model/Service.
class CalendarModelObserver {
 public:
  // Invoked when the model has finished loading.
  virtual void CalendarModelLoaded(CalendarService* model) {}

  virtual void ExtensiveCalendarChangesBeginning(CalendarService* service) {}
  virtual void ExtensiveCalendarChangesEnded(CalendarService* service) {}

  // Invoked from the destructor of the CalendarService.
  virtual void CalendarModelBeingDeleted(CalendarService* service) {}

  virtual void OnCalendarServiceLoaded(CalendarService* service) {}

  virtual void OnEventCreated(CalendarService* service, const EventRow& row) {}
  virtual void OnEventDeleted(CalendarService* service, const EventRow& row) {}
  virtual void OnEventChanged(CalendarService* service, const EventRow& row) {}

  virtual void OnCalendarCreated(CalendarService* service,
                                 const CalendarRow& row) {}
  virtual void OnCalendarDeleted(CalendarService* service,
                                 const CalendarRow& row) {}
  virtual void OnCalendarChanged(CalendarService* service,
                                 const CalendarRow& row) {}

 protected:
  virtual ~CalendarModelObserver() {}
};

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_MODEL_OBSERVER_H_
