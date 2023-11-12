// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calendar/notification_type.h"

namespace calendar {

NotificationRow::NotificationRow() : delay(0) {}

NotificationRow::NotificationRow(const NotificationRow& other) = default;

GetAllNotificationResult::GetAllNotificationResult() = default;

GetAllNotificationResult::~GetAllNotificationResult() = default;

GetAllNotificationResult::GetAllNotificationResult(
    const GetAllNotificationResult& notifications) = default;

}  // namespace calendar
