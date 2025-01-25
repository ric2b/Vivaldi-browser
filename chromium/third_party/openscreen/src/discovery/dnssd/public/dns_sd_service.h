// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_DNS_SD_SERVICE_H_
#define DISCOVERY_DNSSD_PUBLIC_DNS_SD_SERVICE_H_

#include <functional>
#include <memory>

#include "platform/api/task_runner_deleter.h"
#include "platform/base/error.h"
#include "platform/base/interface_info.h"
#include "platform/base/ip_address.h"

namespace openscreen::discovery {

class DnsSdPublisher;
class DnsSdQuerier;

// This class provides a wrapper around DnsSdQuerier and DnsSdPublisher to
// allow for an embedder-overridable factory method below.
class DnsSdService {
 public:
  virtual ~DnsSdService() = default;

  // Returns the DnsSdQuerier owned by this DnsSdService. If queries are not
  // supported, returns nullptr.
  virtual DnsSdQuerier* GetQuerier() = 0;

  // Returns the DnsSdPublisher owned by this DnsSdService. If publishing is not
  // supported, returns nullptr.
  virtual DnsSdPublisher* GetPublisher() = 0;
};

using DnsSdServicePtr = std::unique_ptr<DnsSdService, TaskRunnerDeleter>;

}  // namespace openscreen::discovery

#endif  // DISCOVERY_DNSSD_PUBLIC_DNS_SD_SERVICE_H_
