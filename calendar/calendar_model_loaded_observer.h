// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_CALENDAR_MODEL_LOADED_OBSERVER_H_
#define CALENDAR_CALENDAR_MODEL_LOADED_OBSERVER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "calendar/calendar_model_observer.h"
#include "calendar/calendar_service.h"

class Profile;

namespace calendar {

class CalendarModelLoadedObserver : public CalendarModelObserver {
 public:
  CalendarModelLoadedObserver();

 private:
  void CalendarModelLoaded(CalendarService* service) override;
  void CalendarModelBeingDeleted(CalendarService* service) override;

  DISALLOW_COPY_AND_ASSIGN(CalendarModelLoadedObserver);
};

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_MODEL_LOADED_OBSERVER_H_
