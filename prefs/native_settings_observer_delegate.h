// Copyright (c) 2015 Vivaldi Technologies

#ifndef PREFS_NATIVE_SETTINGS_OBSERVER_DELEGATE_H_
#define PREFS_NATIVE_SETTINGS_OBSERVER_DELEGATE_H_

#include <string>

#include "extensions/schema/settings.h"

namespace vivaldi {

class NativeSettingsObserverDelegate {
 public:
  virtual ~NativeSettingsObserverDelegate();

  virtual void SetPref(const char* name, const int value) = 0;
  virtual void SetPref(const char* name, const std::string& value) = 0;
  virtual void SetPref(const char* name, const bool value) = 0;
};

}  // namespace vivaldi

#endif  // PREFS_NATIVE_SETTINGS_OBSERVER_DELEGATE_H_
