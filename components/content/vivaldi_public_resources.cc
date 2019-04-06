// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_throttle.h"

namespace content {

void ResourceThrottle::Delegate::SetDelegateOpenFlags(bool open_when_done,
                                                      bool ask_for_target) {}

void ResourceThrottle::SetOpenFlags(bool open_when_done, bool ask_for_target) {
  delegate_->SetDelegateOpenFlags(open_when_done, ask_for_target);
}

}  // namespace content
