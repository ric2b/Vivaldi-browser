// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/coral_util.h"

#include "base/notreached.h"

namespace ash::coral_util {

std::string GetIdentifier(const ContentItem& item) {
  if (std::holds_alternative<AppData>(item)) {
    return std::get<AppData>(item).app_id;
  }
  if (std::holds_alternative<TabData>(item)) {
    return std::get<TabData>(item).source;
  }
  NOTREACHED();
}

CoralRequest::CoralRequest() = default;

CoralRequest::~CoralRequest() = default;

CoralCluster::CoralCluster() = default;

CoralCluster::~CoralCluster() = default;

CoralResponse::CoralResponse() = default;

CoralResponse::~CoralResponse() = default;

}  // namespace ash::coral_util
