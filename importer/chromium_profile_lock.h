// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef  VIVALDI_CHROMIUM_PROFILE_LOCK_H_
#define VIVALDI_CHROMIUM_PROFILE_LOCK_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"

class ChromiumProfileLock {
 public:
   explicit ChromiumProfileLock(const base::FilePath& path);
   ~ChromiumProfileLock();

  // Locks and releases the profile.
  void Lock();
  void Unlock();

  // Returns true if we lock the profile successfully.
  bool HasAcquired();

 private:

  static const base::FilePath::CharType* kLockFileName;

  void Init();

  // Full path of the lock file in the profile folder.
  base::FilePath lock_file_;

  // The handle of the lock file.
#if defined(OS_WIN)
  HANDLE lock_handle_;
#elif defined(OS_POSIX)
  int lock_fd_;

  // Method that tries to put a fcntl lock on file specified by |lock_file_|.
  // Returns false if lock is already held by another process. true in all
  // other cases.
  bool LockWithFcntl();
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromiumProfileLock);
};

#endif  // VIVALDI_CHROMIUM_PROFILE_LOCK_H_
