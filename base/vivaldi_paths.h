// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef BASE_VIVALDI_PATHS_H_
#define BASE_VIVALDI_PATHS_H_

namespace vivaldi {

enum VivaldiPathKey {
  PATH_START = 100000,  // to be out from underfoot of the chromium ranges

  DIR_VIVALDI_TEST_DATA,  // Used only for testing.

  PATH_END,
};

void RegisterVivaldiPaths();
}

#endif  // BASE_VIVALDI_PATHS_H_
