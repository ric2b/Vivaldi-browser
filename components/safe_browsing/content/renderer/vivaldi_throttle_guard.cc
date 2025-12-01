// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/safe_browsing/content/renderer/vivaldi_throttle_guard.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace vivaldi {

ThrottleGuard::ThrottleGuard() {}

ThrottleGuard::~ThrottleGuard() {
  for (auto& observer : observers_) {
    observer.OnURLLoaderThrottleProviderDestroyed();
  }
}

base::WeakPtr<ThrottleGuard> ThrottleGuard::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ThrottleGuard::AddObserver(blink::URLLoaderThrottle *observer) {
  observers_.AddObserver(observer);
}

void ThrottleGuard::RemoveObserver(blink::URLLoaderThrottle *observer) {
  observers_.RemoveObserver(observer);
}

} // namespace vivaldi
