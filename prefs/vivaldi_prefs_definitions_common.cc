// Copyright (c) 2023 Vivaldi Technologies

#include "prefs/vivaldi_prefs_definitions.h"

namespace vivaldi {

base::Value VivaldiPrefsDefinitions::GetPlatformComputedDefault(
    const std::string& path) {
  return base::Value();
}
}  // namespace vivaldi