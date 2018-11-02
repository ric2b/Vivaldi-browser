// Copyright (c) 2015 Vivaldi Technologies.

#include <string>

#include "app/vivaldi_version_info.h"
#include "components/version_info/version_info_values.h"

namespace vivaldi {

std::string GetVivaldiVersionString() {
  return VIVALDI_VERSION;
}

std::string VivaldiLastChange() {
  return LAST_CHANGE_VIVALDI;
}

}  // namespace vivaldi
