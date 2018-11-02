// Copyright (c) 2015 Vivaldi Technologies

#ifndef PREFS_VIVALDI_BROWSER_PREFS_H_
#define PREFS_VIVALDI_BROWSER_PREFS_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "chrome/browser/prefs/browser_prefs.h"

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace vivaldi {

struct PrefProperties {
 public:
  struct EnumProperties {
   public:
    EnumProperties();
    ~EnumProperties();
    EnumProperties(EnumProperties&&);
    std::unordered_map<int, std::string> value_to_string;
    std::unordered_map<std::string, int> string_to_value;
  };
  PrefProperties();
  PrefProperties(std::unordered_map<std::string, int> string_to_value,
                 std::unordered_map<int, std::string> value_to_string);
  PrefProperties(PrefProperties&&);
  ~PrefProperties();
  std::unique_ptr<EnumProperties> enum_properties;
};
typedef std::unordered_map<std::string, PrefProperties> PrefsProperties;

// Vivaldi: Register platform specific preferences
void RegisterPlatformPrefs(user_prefs::PrefRegistrySyncable* registry);

PrefsProperties RegisterBrowserPrefs(
    user_prefs::PrefRegistrySyncable* registry);

// Vivaldi: Register preferences to the local state
void RegisterLocalState(PrefRegistrySimple* registry);

}  // namespace vivaldi

#endif  // PREFS_VIVALDI_BROWSER_PREFS_H_
