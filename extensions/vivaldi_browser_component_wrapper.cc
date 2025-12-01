// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "base/logging.h"
#include "extensions/vivaldi_browser_component_wrapper.h"

static VivaldiBrowserComponentWrapper* wrapper_impl_ = nullptr;

// static
VivaldiBrowserComponentWrapper* VivaldiBrowserComponentWrapper::GetInstance() {
  if (!wrapper_impl_) {
    LOG(ERROR) << "VivaldiBrowserComponentWrapper::SetInstance must be called "
                  "prior to "
                  "this.";
  }
  return wrapper_impl_;
}

// static
void VivaldiBrowserComponentWrapper::SetInstance(
    VivaldiBrowserComponentWrapper* wrapper) {
  wrapper_impl_ = wrapper;
}
