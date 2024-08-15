// Copyright (c) 2014 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_KEYSTORE_CHECKER_H_
#define EXTRAPARTS_VIVALDI_KEYSTORE_CHECKER_H_

namespace vivaldi {

// Validates profile's secure key storage status.
// true == keystore is locked or has other problems, and user requested to quit
// profile loading operation.
bool HasLockedKeystore(Profile *prof);

} // namespace vivaldi

#endif  // EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_LINUX_H_
