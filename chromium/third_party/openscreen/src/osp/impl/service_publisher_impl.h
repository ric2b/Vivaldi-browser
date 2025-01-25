// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_SERVICE_PUBLISHER_IMPL_H_
#define OSP_IMPL_SERVICE_PUBLISHER_IMPL_H_

#include <memory>

#include "discovery/common/reporting_client.h"
#include "osp/public/service_publisher.h"
#include "platform/base/macros.h"

namespace openscreen::osp {

class ServicePublisherImpl final
    : public ServicePublisher,
      public openscreen::discovery::ReportingClient {
 public:
  class Delegate {
   public:
    Delegate();
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    Delegate(Delegate&&) noexcept = delete;
    Delegate& operator=(Delegate&&) noexcept = delete;
    virtual ~Delegate();

    void SetPublisherImpl(ServicePublisherImpl* publisher);

    virtual void StartPublisher(const ServicePublisher::Config& config) = 0;
    virtual void StartAndSuspendPublisher(
        const ServicePublisher::Config& config) = 0;
    virtual void StopPublisher() = 0;
    virtual void SuspendPublisher() = 0;
    virtual void ResumePublisher(const ServicePublisher::Config& config) = 0;

   protected:
    void SetState(State state) { publisher_->SetState(state); }

    ServicePublisherImpl* publisher_ = nullptr;
  };

  // `delegate` is required and is used to implement state transitions.
  explicit ServicePublisherImpl(std::unique_ptr<Delegate> delegate);
  ServicePublisherImpl(const ServicePublisherImpl&) = delete;
  ServicePublisherImpl& operator=(const ServicePublisherImpl&) = delete;
  ServicePublisherImpl(ServicePublisherImpl&&) noexcept = delete;
  ServicePublisherImpl& operator=(ServicePublisherImpl&&) noexcept = delete;
  ~ServicePublisherImpl() override;

  // Called by `delegate_` when an internal error occurs.
  void OnError(const Error& error);

  // ServicePublisher overrides.
  bool Start() override;
  bool StartAndSuspend() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  void AddObserver(Observer& observer) override;
  void RemoveObserver(Observer& observer) override;

 private:
  // openscreen::discovery::ReportingClient overrides.
  void OnFatalError(const Error&) override;
  void OnRecoverableError(const Error&) override;

  // Called by `delegate_` to transition the state machine (except kStarting and
  // kStopping which are done automatically).
  void SetState(State state);

  // Notifies each observer in `observers_` if the transition to `state_` is one
  // that is watched by the observer interface.
  void MaybeNotifyObserver();

  std::unique_ptr<Delegate> delegate_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_SERVICE_PUBLISHER_IMPL_H_
