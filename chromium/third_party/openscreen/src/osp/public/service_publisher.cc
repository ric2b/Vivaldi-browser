// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/service_publisher.h"

namespace openscreen::osp {

bool ServicePublisher::Config::IsValid() const {
  return !friendly_name.empty() && !instance_name.empty() &&
         !fingerprint.empty() && !auth_token.empty() &&
         connection_server_port > 0 && !network_interfaces.empty();
}

ServicePublisher::~ServicePublisher() = default;

void ServicePublisher::SetConfig(const Config& config) {
  config_ = config;
}

ServicePublisher::ServicePublisher() : state_(State::kStopped) {}

}  // namespace openscreen::osp
