// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_CHROMIUM_PROFILE_LOCK_H_
#define IMPORTER_CHROMIUM_PROFILE_LOCK_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

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
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromiumProfileLock);
};

#endif  // IMPORTER_CHROMIUM_PROFILE_LOCK_H_
