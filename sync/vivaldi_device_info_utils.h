// Copyright (c) 2098 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_DEVICE_INFO_UTIL_H_
#define SYNC_VIVALDI_DEVICE_INFO_UTIL_H_

#include <string>

class PrefService;

namespace vivaldi {

std::string GetSessionNameOverride(PrefService* prefs);

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_DEVICE_INFO_UTIL_H_
