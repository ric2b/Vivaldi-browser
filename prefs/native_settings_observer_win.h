// Copyright (c) 2017-2021 Vivaldi Technologies AS. All Rights Reserved.

#ifndef PREFS_NATIVE_SETTINGS_OBSERVER_WIN_H_
#define PREFS_NATIVE_SETTINGS_OBSERVER_WIN_H_

#include "base/callback_list.h"
#include "base/win/registry.h"
#include "prefs/native_settings_observer.h"

#include "app/vivaldi_apptools.h"

class Profile;

namespace vivaldi {

class NativeSettingsObserverWin : public NativeSettingsObserver {
 public:
  explicit NativeSettingsObserverWin(Profile* profile);
  ~NativeSettingsObserverWin() override;

  void OnThemeColorUpdated();
  void OnSysColorChange();

 private:
  std::unique_ptr<base::win::RegKey> theme_key_;
  // Unretained is safe as the subscription will remove the callback
  // when it goes out of scope.
  base::CallbackListSubscription callback_subscription_ =
      SystemColorsUpdatedCallback(
          base::BindRepeating(&NativeSettingsObserverWin::OnSysColorChange,
                              base::Unretained(this)));
};

}  // namespace vivaldi

#endif  // PREFS_NATIVE_SETTINGS_OBSERVER_WIN_H_
