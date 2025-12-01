// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_SERVICE_LISTENER_H_
#define OSP_PUBLIC_SERVICE_LISTENER_H_

#include <memory>
#include <vector>

#include "osp/public/receiver_list.h"
#include "osp/public/service_info.h"
#include "platform/base/error.h"
#include "platform/base/interface_info.h"

namespace openscreen::osp {

class ServiceListener final {
 public:
  enum class State {
    kStopped = 0,
    kStarting,
    kRunning,
    kStopping,
    kSearching,
    kSuspended,
  };

  class Observer {
   public:
    Observer();
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    Observer(Observer&&) noexcept = delete;
    Observer& operator=(Observer&&) noexcept = delete;
    virtual ~Observer();

    // Called when the state becomes kRunning.
    virtual void OnStarted() = 0;
    // Called when the state becomes kStopped.
    virtual void OnStopped() = 0;
    // Called when the state becomes kSuspended.
    virtual void OnSuspended() = 0;
    // Called when the state becomes kSearching.
    virtual void OnSearching() = 0;

    // Notifications to changes to the listener's receiver list.
    virtual void OnReceiverAdded(const ServiceInfo&) = 0;
    virtual void OnReceiverChanged(const ServiceInfo&) = 0;
    virtual void OnReceiverRemoved(const ServiceInfo&) = 0;
    // Called if all receivers are no longer available, e.g. all network
    // interfaces have been disabled.
    virtual void OnAllReceiversRemoved() = 0;

    // Reports an error.
    virtual void OnError(const Error&) = 0;
  };

  struct Config {
    // A list of network interfaces that the listener should use.
    // By default, all enabled Ethernet and WiFi interfaces are used.
    std::vector<InterfaceInfo> network_interfaces;

    // Returns true if the config object is valid.
    bool IsValid() const;
  };

  class Delegate {
   public:
    Delegate();
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    Delegate(Delegate&&) noexcept = delete;
    Delegate& operator=(Delegate&&) noexcept = delete;
    virtual ~Delegate();

    void SetListener(ServiceListener* listener);

    virtual void StartListener(const ServiceListener::Config& config) = 0;
    virtual void StartAndSuspendListener(
        const ServiceListener::Config& config) = 0;
    virtual void StopListener() = 0;
    virtual void SuspendListener() = 0;
    virtual void ResumeListener() = 0;
    virtual void SearchNow(State from) = 0;

   protected:
    void SetState(State state);

    ServiceListener* listener_ = nullptr;
  };

  // `delegate` is used to implement state transitions.
  explicit ServiceListener(std::unique_ptr<Delegate> delegate);
  ServiceListener(const ServiceListener&) = delete;
  ServiceListener& operator=(const ServiceListener&) = delete;
  ServiceListener(ServiceListener&&) noexcept = delete;
  ServiceListener& operator=(ServiceListener&&) noexcept = delete;
  virtual ~ServiceListener();

  // Sets the service configuration for this listener.
  void SetConfig(const Config& config);

  // Starts listening for receivers using the config object.
  // Returns true if state() == kStopped and the service will be started, false
  // otherwise.
  bool Start();

  // Starts the listener in kSuspended mode.  This could be used to enable
  // immediate search via SearchNow() in the future.
  // Returns true if state() == kStopped and the service will be started, false
  // otherwise.
  bool StartAndSuspend();

  // Stops listening and cancels any search in progress.
  // Returns true if state() != (kStopped|kStopping).
  bool Stop();

  // Suspends background listening. For example, the tab wanting receiver
  // availability might go in the background, meaning we can suspend listening
  // to save power.
  // Returns true if state() == (kRunning|kSearching|kStarting), meaning the
  // suspension will take effect.
  bool Suspend();

  // Resumes listening.  Returns true if state() == (kSuspended|kSearching).
  bool Resume();

  // Asks the listener to search for receivers now, even if the listener is
  // is currently suspended.  If a background search is already in
  // progress, this has no effect.  Returns true if state() ==
  // (kRunning|kSuspended).
  bool SearchNow();

  void AddObserver(Observer& observer);
  void RemoveObserver(Observer& observer);

  // Called by `delegate_` to transition the state machine (except kStarting and
  // kStopping which are done automatically).
  void SetState(State state);

  // OnReceiverUpdated is called by `delegate_` when there are updates to the
  // available receivers.
  void OnReceiverUpdated(const std::vector<ServiceInfo>& new_receivers);

  // Called by `delegate_` when an internal error occurs.
  void OnError(const Error& error);

  // Returns the current state of the listener.
  State state() const { return state_; }

  // Returns the last error reported by this listener.
  const Error& last_error() const { return last_error_; }

  // Returns the current list of receivers known to the ServiceListener.
  const std::vector<ServiceInfo>& GetReceivers() const {
    return receiver_list_.receivers();
  }

 private:
  // Called by OnReceiverUpdated according to different situations, repectively.
  void OnReceiverAdded(const ServiceInfo& info);
  void OnReceiverChanged(const ServiceInfo& info);
  void OnReceiverRemoved(const ServiceInfo& info);
  void OnAllReceiversRemoved();

  // Notifies each observer in `observers_` if the transition to `state_` is one
  // that is watched by the observer interface.
  void MaybeNotifyObservers();

  State state_ = State::kStopped;
  Error last_error_;
  Config config_;
  std::unique_ptr<Delegate> delegate_;
  std::vector<Observer*> observers_;
  ReceiverList receiver_list_;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_SERVICE_LISTENER_H_
