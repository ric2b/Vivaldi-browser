// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/impl/receiver_base.h"

namespace openscreen::cast {

ReceiverBase::Consumer::~Consumer() = default;

ReceiverBase::ReceiverBase() = default;

ReceiverBase::~ReceiverBase() = default;

}  // namespace openscreen::cast
