// Copyright (c) 2014 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_KEYSTORE_CHECKER_H_
#define EXTRAPARTS_VIVALDI_KEYSTORE_CHECKER_H_

namespace vivaldi {

// Validates profile's secure key storage status.
// true == keystore is locked or has other problems, and user requested to quit
// profile loading operation.
bool HasLockedKeystore(Profile *prof);


/** Initializes oscrypt on windows and returns true if init went ok.
 * If the keystore encryption key changed and user requested to safely
 * exit, should_exit will be set to true and result will be false.
 *
 * Otherwise, this call will have the same result as a call to
 *    OSCrypt::Init(local_state)
 * ...which it replaces.
 *
 * For non-windows platforms, this function will just return true and
 * do nothing.
 */
bool InitOSCrypt(PrefService *local_state, bool *should_exit);

} // namespace vivaldi

#endif  // EXTRAPARTS_VIVALDI_BROWSER_MAIN_EXTRA_PARTS_LINUX_H_
