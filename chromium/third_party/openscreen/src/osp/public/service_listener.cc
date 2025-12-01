// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/service_listener.h"

#include <utility>

#include "util/osp_logging.h"
#include "util/std_util.h"

namespace openscreen::osp {

namespace {

bool IsTransitionValid(ServiceListener::State from, ServiceListener::State to) {
  switch (from) {
    case ServiceListener::State::kStopped:
      return to == ServiceListener::State::kStarting ||
             to == ServiceListener::State::kStopping;
    case ServiceListener::State::kStarting:
      return to == ServiceListener::State::kRunning ||
             to == ServiceListener::State::kStopping ||
             to == ServiceListener::State::kSuspended;
    case ServiceListener::State::kRunning:
      return to == ServiceListener::State::kSuspended ||
             to == ServiceListener::State::kSearching ||
             to == ServiceListener::State::kStopping;
    case ServiceListener::State::kStopping:
      return to == ServiceListener::State::kStopped;
    case ServiceListener::State::kSearching:
      return to == ServiceListener::State::kRunning ||
             to == ServiceListener::State::kSuspended ||
             to == ServiceListener::State::kStopping;
    case ServiceListener::State::kSuspended:
      return to == ServiceListener::State::kRunning ||
             to == ServiceListener::State::kSearching ||
             to == ServiceListener::State::kStopping;
    default:
      OSP_CHECK(false) << "unknown ServiceListener::State value: "
                       << static_cast<int>(from);
      break;
  }
  return false;
}

}  // namespace

ServiceListener::Observer::Observer() = default;
ServiceListener::Observer::~Observer() = default;

bool ServiceListener::Config::IsValid() const {
  return !network_interfaces.empty();
}

ServiceListener::Delegate::Delegate() = default;
ServiceListener::Delegate::~Delegate() = default;

void ServiceListener::Delegate::SetListener(ServiceListener* listener) {
  OSP_CHECK(!listener_);
  listener_ = listener;
}

void ServiceListener::Delegate::SetState(State state) {
  OSP_CHECK(listener_);
  listener_->SetState(state);
}

ServiceListener::ServiceListener(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  delegate_->SetListener(this);
}

ServiceListener::~ServiceListener() = default;

void ServiceListener::SetConfig(const Config& config) {
  config_ = config;
}

bool ServiceListener::Start() {
  if (state_ != State::kStopped) {
    return false;
  }

  state_ = State::kStarting;
  delegate_->StartListener(config_);
  return true;
}

bool ServiceListener::StartAndSuspend() {
  if (state_ != State::kStopped) {
    return false;
  }

  state_ = State::kStarting;
  delegate_->StartAndSuspendListener(config_);
  return true;
}

bool ServiceListener::Stop() {
  if (state_ == State::kStopped || state_ == State::kStopping) {
    return false;
  }

  state_ = State::kStopping;
  delegate_->StopListener();
  return true;
}

bool ServiceListener::Suspend() {
  if (state_ != State::kRunning && state_ != State::kSearching &&
      state_ != State::kStarting) {
    return false;
  }

  delegate_->SuspendListener();
  return true;
}

bool ServiceListener::Resume() {
  if (state_ != State::kSuspended && state_ != State::kSearching) {
    return false;
  }

  delegate_->ResumeListener();
  return true;
}

bool ServiceListener::SearchNow() {
  if (state_ != State::kRunning && state_ != State::kSuspended) {
    return false;
  }

  delegate_->SearchNow(state_);
  return true;
}

void ServiceListener::AddObserver(Observer& observer) {
  observers_.push_back(&observer);
}

void ServiceListener::RemoveObserver(Observer& observer) {
  observers_.erase(std::remove(observers_.begin(), observers_.end(), &observer),
                   observers_.end());
}

void ServiceListener::SetState(State state) {
  OSP_CHECK(IsTransitionValid(state_, state));
  state_ = state;
  MaybeNotifyObservers();
}

void ServiceListener::OnReceiverUpdated(
    const std::vector<ServiceInfo>& new_receivers) {
  // All receivers are removed.
  if (new_receivers.empty()) {
    OnAllReceiversRemoved();
  }

  const auto& old_receivers = GetReceivers();
  if (new_receivers.size() < old_receivers.size()) {
    // A receiver is removed.
    for (const auto& receiver : old_receivers) {
      if (Contains(new_receivers, receiver)) {
        continue;
      }

      OnReceiverRemoved(receiver);
      return;
    }
  } else {
    // A receiver is added or updated.
    for (const auto& receiver : new_receivers) {
      if (Contains(old_receivers, receiver)) {
        continue;
      }

      new_receivers.size() > old_receivers.size() ? OnReceiverAdded(receiver)
                                                  : OnReceiverChanged(receiver);
      return;
    }
  }
}

void ServiceListener::OnError(const Error& error) {
  last_error_ = error;
  for (auto* observer : observers_) {
    observer->OnError(error);
  }
}

void ServiceListener::OnReceiverAdded(const ServiceInfo& info) {
  OSP_VLOG << __func__ << ": new receiver added=" << info.ToString();
  receiver_list_.OnReceiverAdded(info);
  for (auto* observer : observers_) {
    observer->OnReceiverAdded(info);
  }
}

void ServiceListener::OnReceiverChanged(const ServiceInfo& info) {
  OSP_VLOG << __func__ << ": receiver changed=" << info.ToString();
  const Error changed_error = receiver_list_.OnReceiverChanged(info);
  if (changed_error.ok()) {
    for (auto* observer : observers_) {
      observer->OnReceiverChanged(info);
    }
  }
}

void ServiceListener::OnReceiverRemoved(const ServiceInfo& info) {
  OSP_VLOG << __func__ << ": receiver removed=" << info.ToString();
  const ErrorOr<ServiceInfo> removed_or_error =
      receiver_list_.OnReceiverRemoved(info);
  if (removed_or_error.is_value()) {
    for (auto* observer : observers_) {
      observer->OnReceiverRemoved(removed_or_error.value());
    }
  }
}

void ServiceListener::OnAllReceiversRemoved() {
  OSP_VLOG << __func__ << ": all receivers removed.";
  const Error removed_all_error = receiver_list_.OnAllReceiversRemoved();
  if (removed_all_error.ok()) {
    for (auto* observer : observers_) {
      observer->OnAllReceiversRemoved();
    }
  }
}

void ServiceListener::MaybeNotifyObservers() {
  switch (state_) {
    case State::kRunning: {
      for (auto* observer : observers_) {
        observer->OnStarted();
      }
      break;
    }

    case State::kStopped: {
      for (auto* observer : observers_) {
        observer->OnStopped();
      }
      break;
    }

    case State::kSuspended: {
      for (auto* observer : observers_) {
        observer->OnSuspended();
      }
      break;
    }

    case State::kSearching: {
      for (auto* observer : observers_) {
        observer->OnSearching();
      }
      break;
    }

    default:
      break;
  }
}

}  // namespace openscreen::osp
