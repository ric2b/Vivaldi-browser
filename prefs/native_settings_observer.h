// Copyright (c) 2017 Vivaldi Technologies. All Rights Reserved.

#ifndef PREFS_NATIVE_SETTINGS_OBSERVER_H_
#define PREFS_NATIVE_SETTINGS_OBSERVER_H_

#include <string>

#include "base/memory/raw_ptr.h"

class Profile;

namespace vivaldi {

class NativeSettingsObserver {
 public:
  explicit NativeSettingsObserver(Profile* profile);
  virtual ~NativeSettingsObserver() = default;

  static NativeSettingsObserver* Create(Profile* profile);

  void SetPref(const char* name, const int value);
  void SetPref(const char* name, const std::string& value);
  void SetPref(const char* name, const bool value);

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace vivaldi

#endif  // PREFS_NATIVE_SETTINGS_OBSERVER_H_
