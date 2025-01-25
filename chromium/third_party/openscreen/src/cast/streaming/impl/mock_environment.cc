// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/testing/mock_environment.h"

namespace openscreen::cast {

MockEnvironment::MockEnvironment(ClockNowFunctionPtr now_function,
                                 TaskRunner& task_runner)
    : Environment(now_function, task_runner) {}

MockEnvironment::~MockEnvironment() = default;

}  // namespace openscreen::cast
