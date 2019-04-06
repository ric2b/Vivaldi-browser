// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "base/files/file_util.h"
#include "importer/chromium_profile_lock.h"

void ChromiumProfileLock::Init() {}

void ChromiumProfileLock::Lock() {}

void ChromiumProfileLock::Unlock() {}

bool ChromiumProfileLock::HasAcquired() {
  return !base::IsLink(lock_file_);
}
