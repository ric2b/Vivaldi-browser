// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_DNS_SD_PUBLISHER_CLIENT_H_
#define OSP_IMPL_DNS_SD_PUBLISHER_CLIENT_H_

#include <memory>

#include "discovery/common/reporting_client.h"
#include "discovery/dnssd/public/dns_sd_service.h"
#include "discovery/public/dns_sd_service_publisher.h"
#include "osp/public/service_publisher.h"
#include "platform/api/task_runner.h"

namespace openscreen::osp {

class DnsSdPublisherClient final
    : public ServicePublisher::Delegate,
      public openscreen::discovery::ReportingClient {
 public:
  explicit DnsSdPublisherClient(TaskRunner& task_runner);
  DnsSdPublisherClient(const DnsSdPublisherClient&) = delete;
  DnsSdPublisherClient& operator=(const DnsSdPublisherClient&) = delete;
  DnsSdPublisherClient(DnsSdPublisherClient&&) noexcept = delete;
  DnsSdPublisherClient& operator=(DnsSdPublisherClient&&) noexcept = delete;
  ~DnsSdPublisherClient() override;

  // ServicePublisher::Delegate overrides.
  void StartPublisher(const ServicePublisher::Config& config) override;
  void StartAndSuspendPublisher(
      const ServicePublisher::Config& config) override;
  void StopPublisher() override;
  void SuspendPublisher() override;
  void ResumePublisher(const ServicePublisher::Config& config) override;

 private:
  // openscreen::discovery::ReportingClient overrides.
  void OnFatalError(const Error&) override;
  void OnRecoverableError(const Error&) override;

  void StartPublisherInternal(const ServicePublisher::Config& config);
  discovery::DnsSdServicePtr CreateDnsSdServiceInternal(
      const ServicePublisher::Config& config);

  TaskRunner& task_runner_;
  discovery::DnsSdServicePtr dns_sd_service_;

  using OspDnsSdPublisher =
      discovery::DnsSdServicePublisher<ServicePublisher::Config>;

  std::unique_ptr<OspDnsSdPublisher> dns_sd_publisher_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_DNS_SD_PUBLISHER_CLIENT_H_
