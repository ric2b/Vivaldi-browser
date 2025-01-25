// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/service_listener.h"

namespace openscreen::osp {

ServiceListener::Observer::Observer() = default;
ServiceListener::Observer::~Observer() = default;

bool ServiceListener::Config::IsValid() const {
  return !network_interfaces.empty();
}

ServiceListener::ServiceListener() : state_(State::kStopped) {}
ServiceListener::~ServiceListener() = default;

void ServiceListener::SetConfig(const Config& config) {
  config_ = config;
}

}  // namespace openscreen::osp
