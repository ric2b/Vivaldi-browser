// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_SERVICE_PUBLISHER_H_
#define OSP_PUBLIC_SERVICE_PUBLISHER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "platform/base/error.h"
#include "platform/base/interface_info.h"

namespace openscreen::osp {

class ServicePublisher final {
 public:
  enum class State {
    kStopped = 0,
    kStarting,
    kRunning,
    kStopping,
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

    // Reports an error.
    virtual void OnError(const Error&) = 0;
  };

  struct Config {
    // The DNS domain name label that should be used to identify this service
    // within the openscreen service type.
    std::string instance_name;

    // The fingerprint of the server's certificate and it is included in DNS TXT
    // records.
    std::string fingerprint;

    // An alphanumeric and unguessable token used for authentication and it is
    // included in DNS TXT records.
    std::string auth_token;

    // The port where openscreen connections are accepted.
    // Normally this should not be set, and must be identical to the port
    // configured in the ProtocolConnectionServer.
    uint16_t connection_server_port = 0;

    // A list of network interfaces that the publisher should use.
    // By default, all enabled Ethernet and WiFi interfaces are used.
    // This configuration must be identical to the interfaces configured
    // in the ScreenConnectionServer.
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

    void SetPublisher(ServicePublisher* publisher);

    virtual void StartPublisher(const ServicePublisher::Config& config) = 0;
    virtual void StartAndSuspendPublisher(
        const ServicePublisher::Config& config) = 0;
    virtual void StopPublisher() = 0;
    virtual void SuspendPublisher() = 0;
    virtual void ResumePublisher(const ServicePublisher::Config& config) = 0;

   protected:
    void SetState(State state);

    ServicePublisher* publisher_ = nullptr;
  };

  // `delegate` is required and is used to implement state transitions.
  explicit ServicePublisher(std::unique_ptr<Delegate> delegate);
  ServicePublisher(const ServicePublisher&) = delete;
  ServicePublisher& operator=(const ServicePublisher&) = delete;
  ServicePublisher(ServicePublisher&&) noexcept = delete;
  ServicePublisher& operator=(ServicePublisher&&) noexcept = delete;
  virtual ~ServicePublisher();

  // Sets the service configuration for this publisher.
  void SetConfig(const Config& config);

  // Starts publishing this service using the config object.
  // Returns true if state() == kStopped and the service will be started, false
  // otherwise.
  bool Start();

  // Starts publishing this service, but then immediately suspends the
  // publisher. No announcements will be sent until Resume() is called. Returns
  // true if state() == kStopped and the service will be started, false
  // otherwise.
  bool StartAndSuspend();

  // Stops publishing this service.
  // Returns true if state() != (kStopped|kStopping).
  bool Stop();

  // Suspends publishing, for example, if the service is in a power saving
  // mode. Returns true if state() == (kRunning|kStarting), meaning the
  // suspension will take effect.
  bool Suspend();

  // Resumes publishing.  Returns true if state() == kSuspended.
  bool Resume();

  void AddObserver(Observer& observer);
  void RemoveObserver(Observer& observer);

  // Called by `delegate_` to transition the state machine (except kStarting and
  // kStopping which are done automatically).
  void SetState(State state);

  // Called by `delegate_` when an internal error occurs.
  void OnError(const Error& error);

  // Returns the current state of the publisher.
  State state() const { return state_; }

  // Returns the last error reported by this publisher.
  const Error& last_error() const { return last_error_; }

 private:
  // Notifies each observer in `observers_` if the transition to `state_` is one
  // that is watched by the observer interface.
  void MaybeNotifyObserver();

  State state_ = State::kStopped;
  Error last_error_;
  Config config_;
  std::unique_ptr<Delegate> delegate_;
  std::vector<Observer*> observers_;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_SERVICE_PUBLISHER_H_
