// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "importer/chromium_profile_lock.h"

#include <windows.h>

void ChromiumProfileLock::Init() {
  lock_handle_ = INVALID_HANDLE_VALUE;
}

void ChromiumProfileLock::Lock() {
  if (HasAcquired())
    return;
  lock_handle_ =
      CreateFile(lock_file_.value().c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                 NULL, OPEN_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
}

void ChromiumProfileLock::Unlock() {
  if (!HasAcquired())
    return;
  CloseHandle(lock_handle_);
  lock_handle_ = INVALID_HANDLE_VALUE;
}

bool ChromiumProfileLock::HasAcquired() {
  return (lock_handle_ != INVALID_HANDLE_VALUE);
}
