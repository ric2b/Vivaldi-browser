// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "importer/chromium_profile_lock.h"

#include "base/files/file_path.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
const base::FilePath::CharType* ChromiumProfileLock::kLockFileName =
    FILE_PATH_LITERAL("SingletonLock");
#else
const base::FilePath::CharType* ChromiumProfileLock::kLockFileName =
    FILE_PATH_LITERAL("lockfile");
#endif

ChromiumProfileLock::ChromiumProfileLock(const base::FilePath& path) {
  Init();
  lock_file_ = path.Append(kLockFileName);
  Lock();
}

ChromiumProfileLock::~ChromiumProfileLock() {
  // Because this destructor happens in first run on the profile import thread,
  // with no UI to jank, it's ok to allow deletion of the lock here.
  base::VivaldiScopedAllowBlocking allow_blocking;
  Unlock();
}
