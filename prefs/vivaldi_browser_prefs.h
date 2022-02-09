// Copyright (c) 2015 Vivaldi Technologies

#ifndef PREFS_VIVALDI_BROWSER_PREFS_H_
#define PREFS_VIVALDI_BROWSER_PREFS_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace vivaldi {

struct EnumPrefProperties {
  EnumPrefProperties();
  ~EnumPrefProperties();
  EnumPrefProperties(EnumPrefProperties&&);

  absl::optional<int> FindValue(base::StringPiece name) const;
  const std::string* FindName(int value) const;

  std::vector<std::pair<std::string, int>> name_value_pairs;
  DISALLOW_COPY_AND_ASSIGN(EnumPrefProperties);
};

bool IsMergeableListPreference(base::StringPiece name);

#ifdef OS_ANDROID

// Declare an empty struct to minimize the amount of Android-specific ifdefs.
struct PrefPropertiesMap {};

#else

// Preference properties to use in our JS bindings
class PrefProperties {
 public:
  PrefProperties();
  PrefProperties(PrefProperties&&);
  ~PrefProperties();

  bool is_local() const { return local_pref_; }
  const EnumPrefProperties* enum_properties() const {
    return enum_properties_.get();
  }

  void SetLocal(bool local_pref);
  void SetEnumProperties(EnumPrefProperties enum_properties);

 private:
  std::unique_ptr<EnumPrefProperties> enum_properties_;
  bool local_pref_ = false;
  DISALLOW_COPY_AND_ASSIGN(PrefProperties);
};

using PrefPropertiesMap = std::unordered_map<std::string, PrefProperties>;

// Workaround to pass to our extension API registered preference properties
// for a new profile. Chromium does not provide a way to access just created
// profile from RegisterProfilePrefs.
PrefPropertiesMap ExtractLastRegisteredPrefsProperties();

#endif  // ifdef OS_ANDROID

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

void MigrateOldPrefs(PrefService* prefs);

// Vivaldi: Register preferences to the local state
void RegisterLocalState(PrefRegistrySimple* registry);

}  // namespace vivaldi

#endif  // PREFS_VIVALDI_BROWSER_PREFS_H_
