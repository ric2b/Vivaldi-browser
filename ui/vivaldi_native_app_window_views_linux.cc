// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "ui/vivaldi_native_app_window_views_aura.h"

// static
std::unique_ptr<VivaldiNativeAppWindowViews>
VivaldiNativeAppWindowViews::Create() {
  return std::make_unique<VivaldiNativeAppWindowViewsAura>();
}
