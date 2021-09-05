// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/event_exception_type.h"

namespace calendar {

EventExceptionType::~EventExceptionType() {}

EventExceptionType::EventExceptionType() {}

EventExceptionType::EventExceptionType(const EventExceptionType& other) =
    default;

}  // namespace calendar
