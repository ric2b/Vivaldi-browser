// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_throttle.h"

namespace content {

/* NOTE(yngve): Risk of infinite loop, should only be a problem for us,
* if we add new subclasses, chromium will still use the abstract definition.
*/
void ResourceThrottle::Delegate::Resume() {
  Resume(false, false);
}

void ResourceThrottle::Delegate::Resume(bool open_when_done,
  bool ask_for_target) {
  Resume();
};

bool ResourceThrottle::MustProcessResponseBeforeReadingBody() {
  return false;
}

void ResourceThrottle::Cancel() {
  delegate_->Cancel();
}

void ResourceThrottle::CancelAndIgnore() {
  delegate_->CancelAndIgnore();
}

void ResourceThrottle::CancelWithError(int error_code) {
  delegate_->CancelWithError(error_code);
}

void ResourceThrottle::Resume() {
  delegate_->Resume();
}

void ResourceThrottle::Resume(bool open_when_done, bool ask_for_target) {
  delegate_->Resume(open_when_done, ask_for_target);
}

}  // namespace content
