// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_DNS_SD_WATCHER_CLIENT_H_
#define OSP_IMPL_DNS_SD_WATCHER_CLIENT_H_

#include <memory>
#include <vector>

#include "discovery/dnssd/public/dns_sd_service.h"
#include "discovery/public/dns_sd_service_watcher.h"
#include "osp/impl/service_listener_impl.h"
#include "osp/public/service_info.h"
#include "platform/api/task_runner.h"

namespace openscreen {

namespace osp {

class DnsSdWatcherClient final : public ServiceListenerImpl::Delegate {
 public:
  explicit DnsSdWatcherClient(TaskRunner& task_runner);
  DnsSdWatcherClient(const DnsSdWatcherClient&) = delete;
  DnsSdWatcherClient& operator=(const DnsSdWatcherClient&) = delete;
  DnsSdWatcherClient(DnsSdWatcherClient&&) noexcept = delete;
  DnsSdWatcherClient& operator=(DnsSdWatcherClient&&) noexcept = delete;
  ~DnsSdWatcherClient() override;

  // ServiceListenerImpl::Delegate overrides.
  void StartListener(const ServiceListener::Config& config) override;
  void StartAndSuspendListener(const ServiceListener::Config& config) override;
  void StopListener() override;
  void SuspendListener() override;
  void ResumeListener() override;
  void SearchNow(ServiceListener::State from) override;

 private:
  void StartWatcherInternal(const ServiceListener::Config& config);
  discovery::DnsSdServicePtr CreateDnsSdServiceInternal(
      const ServiceListener::Config& config);

  using OspDnsSdWatcher = discovery::DnsSdServiceWatcher<ServiceInfo>;
  void OnDnsWatcherUpdated(std::vector<OspDnsSdWatcher::ConstRefT> all);

  TaskRunner& task_runner_;
  discovery::DnsSdServicePtr dns_sd_service_;

  std::unique_ptr<OspDnsSdWatcher> dns_sd_watcher_;
};

}  // namespace osp
}  // namespace openscreen

#endif  // OSP_IMPL_DNS_SD_WATCHER_CLIENT_H_
