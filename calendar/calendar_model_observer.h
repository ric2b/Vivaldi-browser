// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_CALENDAR_MODEL_OBSERVER_H_
#define CALENDAR_CALENDAR_MODEL_OBSERVER_H_

namespace calendar {

class CalendarService;

// Observer for the Calendar Model/Service.
class CalendarModelObserver {
 public:
  // Invoked when the model has finished loading.
  virtual void CalendarModelLoaded(CalendarService* model) {}

  virtual void ExtensiveCalendarChangesBeginning(CalendarService* model) {}
  virtual void ExtensiveCalendarChangesEnded(CalendarService* model) {}

  // Invoked from the destructor of the CalendarService.
  virtual void CalendarModelBeingDeleted(CalendarService* model) {}

  virtual void OnCalendarServiceLoaded(CalendarService* model) {}

 protected:
  virtual ~CalendarModelObserver() {}
};

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_MODEL_OBSERVER_H_
