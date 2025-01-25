// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_SERVICE_LISTENER_IMPL_H_
#define OSP_IMPL_SERVICE_LISTENER_IMPL_H_

#include <memory>
#include <vector>

#include "discovery/common/reporting_client.h"
#include "osp/impl/receiver_list.h"
#include "osp/public/service_info.h"
#include "osp/public/service_listener.h"

namespace openscreen::osp {

class ServiceListenerImpl final
    : public ServiceListener,
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

    void SetListenerImpl(ServiceListenerImpl* listener);

    virtual void StartListener(const ServiceListener::Config& config) = 0;
    virtual void StartAndSuspendListener(
        const ServiceListener::Config& config) = 0;
    virtual void StopListener() = 0;
    virtual void SuspendListener() = 0;
    virtual void ResumeListener() = 0;
    virtual void SearchNow(State from) = 0;

   protected:
    void SetState(State state) { listener_->SetState(state); }

    ServiceListenerImpl* listener_ = nullptr;
  };

  // `delegate` is used to implement state transitions.
  explicit ServiceListenerImpl(std::unique_ptr<Delegate> delegate);
  ServiceListenerImpl(const ServiceListenerImpl&) = delete;
  ServiceListenerImpl& operator=(const ServiceListenerImpl&) = delete;
  ServiceListenerImpl(ServiceListenerImpl&&) noexcept = delete;
  ServiceListenerImpl& operator=(ServiceListenerImpl&&) noexcept = delete;
  ~ServiceListenerImpl() override;

  // OnReceiverUpdated is called by `delegate_` when there are updates to the
  // available receivers.
  void OnReceiverUpdated(const std::vector<ServiceInfo>& new_receivers);

  // Called by `delegate_` when an internal error occurs.
  void OnError(const Error& error);

  // ServiceListener overrides.
  bool Start() override;
  bool StartAndSuspend() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  bool SearchNow() override;
  void AddObserver(Observer& observer) override;
  void RemoveObserver(Observer& observer) override;
  const std::vector<ServiceInfo>& GetReceivers() const override;

 private:
  // openscreen::discovery::ReportingClient overrides.
  void OnFatalError(const Error&) override;
  void OnRecoverableError(const Error&) override;

  // Called by OnReceiverUpdated according to different situations, repectively.
  void OnReceiverAdded(const ServiceInfo& info);
  void OnReceiverChanged(const ServiceInfo& info);
  void OnReceiverRemoved(const ServiceInfo& info);
  void OnAllReceiversRemoved();

  // Called by `delegate_` to transition the state machine (except kStarting and
  // kStopping which are done automatically).
  void SetState(State state);

  // Notifies each observer in `observers_` if the transition to `state_` is one
  // that is watched by the observer interface.
  void MaybeNotifyObservers();

  std::unique_ptr<Delegate> delegate_;
  ReceiverList receiver_list_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_SERVICE_LISTENER_IMPL_H_
