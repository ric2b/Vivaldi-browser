// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar_database_params.h"

namespace calendar {

CalendarDatabaseParams::CalendarDatabaseParams() {}

CalendarDatabaseParams::CalendarDatabaseParams(
    const base::FilePath& calendar_dir)
    : calendar_dir(calendar_dir) {}  // namespace calendar

CalendarDatabaseParams::~CalendarDatabaseParams() {}

}  // namespace calendar
