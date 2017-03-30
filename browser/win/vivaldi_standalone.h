// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

namespace vivaldi {

#if defined(OS_WIN)
// Returns true if we are standalone. The path to the corresponding user data
// folder is returned in |result|.
bool GetVivaldiStandaloneUserDataDirectory(base::FilePath* result);

// Returns true if we are standalone.
bool IsStandalone();
#endif

}  // namespace vivaldi
