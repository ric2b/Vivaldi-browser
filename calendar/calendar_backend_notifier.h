// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_CALENDAR_BACKEND_NOTIFIER_H_
#define CALENDAR_CALENDAR_BACKEND_NOTIFIER_H_

namespace calendar {

// The CalendarBackendNotifier forwards notifications from the CalendarBackend's
// client to all the interested observers (in both calendar and main thread).
class CalendarBackendNotifier {
 public:
  CalendarBackendNotifier() {}
  virtual ~CalendarBackendNotifier() {}

  // Sends notification that |event| was created
  virtual void NotifyEventCreated() = 0;

  // Sends notification that |events| have been changed or added.
  virtual void NotifyEventModified() = 0;

  // Sends notification that |event| has been deleted.
  virtual void NotifyEventDeleted() = 0;
};

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_BACKEND_NOTIFIER_H_
