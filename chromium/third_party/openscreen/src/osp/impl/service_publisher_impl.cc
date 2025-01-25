// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/service_publisher_impl.h"

#include <algorithm>
#include <utility>

#include "util/osp_logging.h"

namespace openscreen::osp {
namespace {

bool IsTransitionValid(ServicePublisher::State from,
                       ServicePublisher::State to) {
  using State = ServicePublisher::State;
  switch (from) {
    case State::kStopped:
      return to == State::kStarting || to == State::kStopping;
    case State::kStarting:
      return to == State::kRunning || to == State::kStopping ||
             to == State::kSuspended;
    case State::kRunning:
      return to == State::kSuspended || to == State::kStopping;
    case State::kStopping:
      return to == State::kStopped;
    case State::kSuspended:
      return to == State::kRunning || to == State::kStopping;
    default:
      OSP_CHECK(false) << "unknown State value: " << static_cast<int>(from);
      break;
  }
  return false;
}

}  // namespace

ServicePublisherImpl::Delegate::Delegate() = default;
ServicePublisherImpl::Delegate::~Delegate() = default;

void ServicePublisherImpl::Delegate::SetPublisherImpl(
    ServicePublisherImpl* publisher) {
  OSP_CHECK(!publisher_);
  publisher_ = publisher;
}

ServicePublisherImpl::ServicePublisherImpl(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  delegate_->SetPublisherImpl(this);
}

ServicePublisherImpl::~ServicePublisherImpl() = default;

void ServicePublisherImpl::OnError(const Error& error) {
  last_error_ = error;
  for (auto* observer : observers_) {
    observer->OnError(error);
  }
}

bool ServicePublisherImpl::Start() {
  if (state_ != State::kStopped) {
    return false;
  }

  state_ = State::kStarting;
  delegate_->StartPublisher(config_);
  return true;
}

bool ServicePublisherImpl::StartAndSuspend() {
  if (state_ != State::kStopped) {
    return false;
  }

  state_ = State::kStarting;
  delegate_->StartAndSuspendPublisher(config_);
  return true;
}

bool ServicePublisherImpl::Stop() {
  if (state_ == State::kStopped || state_ == State::kStopping) {
    return false;
  }

  state_ = State::kStopping;
  delegate_->StopPublisher();
  return true;
}

bool ServicePublisherImpl::Suspend() {
  if (state_ != State::kRunning && state_ != State::kStarting) {
    return false;
  }

  delegate_->SuspendPublisher();
  return true;
}

bool ServicePublisherImpl::Resume() {
  if (state_ != State::kSuspended) {
    return false;
  }

  delegate_->ResumePublisher(config_);
  return true;
}

void ServicePublisherImpl::AddObserver(Observer& observer) {
  observers_.push_back(&observer);
}

void ServicePublisherImpl::RemoveObserver(Observer& observer) {
  observers_.erase(std::remove(observers_.begin(), observers_.end(), &observer),
                   observers_.end());
}

void ServicePublisherImpl::OnFatalError(const Error& error) {
  OnError(error);
}

void ServicePublisherImpl::OnRecoverableError(const Error& error) {
  OnError(error);
}

void ServicePublisherImpl::SetState(State state) {
  OSP_CHECK(IsTransitionValid(state_, state));
  state_ = state;
  MaybeNotifyObserver();
}

void ServicePublisherImpl::MaybeNotifyObserver() {
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

    default:
      break;
  }
}

}  // namespace openscreen::osp
