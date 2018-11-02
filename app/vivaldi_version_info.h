// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef APP_VIVALDI_VERSION_INFO_H_
#define APP_VIVALDI_VERSION_INFO_H_

namespace vivaldi {

std::string VivaldiLastChange();

// Returns a version string to be displayed in "About Vivaldi" dialog.
std::string GetVivaldiVersionString();

}  // namespace vivaldi

#endif  // APP_VIVALDI_VERSION_INFO_H_
