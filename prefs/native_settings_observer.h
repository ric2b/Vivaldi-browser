// Copyright (c) 2016 Vivaldi Technologies. All Rights Reserved.

#ifndef PREFS_NATIVE_SETTINGS_OBSERVER_H_
#define PREFS_NATIVE_SETTINGS_OBSERVER_H_

#include "prefs/native_settings_observer_delegate.h"

namespace vivaldi {

// A class receiving the callback notification when a registered
// native setting has changed.
class NativeSettingsObserver {
 public:
  virtual ~NativeSettingsObserver();

  static NativeSettingsObserver* Create(
      NativeSettingsObserverDelegate* delegate);
};

}  // namespace vivaldi

#endif  // PREFS_NATIVE_SETTINGS_OBSERVER_H_
