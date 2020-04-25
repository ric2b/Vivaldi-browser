// Copyright (c) 2016-2019 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_WIN_VIVALDI_UTILS_H_
#define BROWSER_WIN_VIVALDI_UTILS_H_

namespace base {
class FilePath;
}

namespace vivaldi {

// Returns true if we are standalone. The path to the corresponding user data
// folder is returned in |result|.
bool GetVivaldiStandaloneUserDataDirectory(base::FilePath* result);

// Returns true if we are standalone.
bool IsStandalone();

// Returns true if Vivaldi is in the process of exiting.
bool IsVivaldiExiting();

// Creates the global mutex for this instance of Vivaldi when exiting.
void SetVivaldiExiting();

// Called when shutdown has been started.
void OnShutdownStarted();

// Attempts to kill lingering Vivaldi processes.
void AttemptToKillTheUndead();

}  // namespace vivaldi

#endif  // BROWSER_WIN_VIVALDI_UTILS_H_
