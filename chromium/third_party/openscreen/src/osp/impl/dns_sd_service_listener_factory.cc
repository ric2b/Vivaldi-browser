// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "osp/impl/dns_sd_watcher_client.h"
#include "osp/public/service_listener.h"
#include "osp/public/service_listener_factory.h"
#include "platform/api/task_runner.h"

namespace openscreen::osp {

// static
std::unique_ptr<ServiceListener> ServiceListenerFactory::Create(
    const ServiceListener::Config& config,
    TaskRunner& task_runner) {
  auto dns_sd_client = std::make_unique<DnsSdWatcherClient>(task_runner);
  auto listener = std::make_unique<ServiceListener>(std::move(dns_sd_client));
  listener->SetConfig(config);
  return listener;
}

}  // namespace openscreen::osp
