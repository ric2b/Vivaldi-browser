// Copyright (c) 2015 Vivaldi Technologies

#ifndef VIVALDI_NATIVE_SETTINGS_OBSERVER_LINUX_H_
#define VIVALDI_NATIVE_SETTINGS_OBSERVER_LINUX_H_

#include "prefs/native_settings_observer.h"
#include "prefs/native_settings_observer_delegate.h"

namespace vivaldi {

class NativeSettingsObserverLinux : public NativeSettingsObserver {
 public:
  explicit NativeSettingsObserverLinux(
      NativeSettingsObserverDelegate* delegate);
  ~NativeSettingsObserverLinux() override;

 private:
  NativeSettingsObserverDelegate* delegate_;
};

}  // namespace vivaldi

#endif  // VIVALDI_NATIVE_SETTINGS_OBSERVER_LINUX_H_
