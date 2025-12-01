// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "discovery/dnssd/public/dns_sd_publisher.h"
#include "osp/impl/dns_sd_publisher_client.h"
#include "osp/public/service_publisher.h"
#include "osp/public/service_publisher_factory.h"
#include "platform/api/task_runner.h"

namespace openscreen::osp {

// static
std::unique_ptr<ServicePublisher> ServicePublisherFactory::Create(
    const ServicePublisher::Config& config,
    TaskRunner& task_runner) {
  auto dns_sd_client = std::make_unique<DnsSdPublisherClient>(task_runner);
  auto publisher = std::make_unique<ServicePublisher>(std::move(dns_sd_client));
  publisher->SetConfig(config);
  return publisher;
}

}  // namespace openscreen::osp
