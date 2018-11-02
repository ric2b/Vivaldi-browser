// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/calendar_model_loaded_observer.h"

#include "calendar/calendar_service.h"

namespace calendar {

CalendarModelLoadedObserver::CalendarModelLoadedObserver() {}

void CalendarModelLoadedObserver::CalendarModelLoaded(
    calendar::CalendarService* service) {
  service->RemoveObserver(this);
  delete this;
}

void CalendarModelLoadedObserver::CalendarModelBeingDeleted(
    calendar::CalendarService* service) {
  service->RemoveObserver(this);
  delete this;
}

}  // namespace calendar
