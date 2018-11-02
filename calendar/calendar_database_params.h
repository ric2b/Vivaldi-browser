// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CALENDAR_CALENDAR_DATABASE_PARAMS_H_
#define CALENDAR_CALENDAR_DATABASE_PARAMS_H_

#include "base/files/file_path.h"

namespace calendar {

// CalendarDatabaseParams store parameters for CalendarDatabase constructor and
// Init methods so that they can be easily passed around between CalendarService
// and CalendarBackend.
struct CalendarDatabaseParams {
  CalendarDatabaseParams();
  CalendarDatabaseParams(const base::FilePath& calendar_dir);
  ~CalendarDatabaseParams();

  base::FilePath calendar_dir;
};

}  // namespace calendar

#endif  // CALENDAR_CALENDAR_DATABASE_PARAMS_H_
