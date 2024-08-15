// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/tabs/model/features.h"

#include "ui/base/device_form_factor.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
// End Vivaldi

bool IsPinnedTabsEnabled() {
  if (vivaldi::IsVivaldiRunning())
    return true; // End Vivaldi

  return ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET;
}
