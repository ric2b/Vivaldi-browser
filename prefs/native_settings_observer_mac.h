// Copyright (c) 2015 Vivaldi Technologies

#ifndef VIVALDI_NATIVE_SETTINGS_OBSERVER_MAC_H_
#define VIVALDI_NATIVE_SETTINGS_OBSERVER_MAC_H_

#include "prefs/native_settings_observer.h"
#include "prefs/native_settings_observer_delegate.h"

namespace vivaldi {

// A class receiving the callback notification when a registered
// native setting has changed.
class NativeSettingsObserverMac : public NativeSettingsObserver {
 public:
  explicit NativeSettingsObserverMac(NativeSettingsObserverDelegate* delegate);
  ~NativeSettingsObserverMac() override;

 private:
  NativeSettingsObserverDelegate* delegate_;
};

}  // namespace vivaldi

#endif  // VIVALDI_NATIVE_SETTINGS_OBSERVER_MAC_H_
