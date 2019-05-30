// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "importer/chromium_profile_lock.h"

#include <stack>
#include <string>

#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"

void ChromiumProfileLock::Init() {

}

void ChromiumProfileLock::Lock() {

}

void ChromiumProfileLock::Unlock() {

}

bool ChromiumProfileLock::HasAcquired() {
  NSString* lockfile = base::mac::FilePathToNSString(lock_file_);
  if (![[NSFileManager defaultManager] destinationOfSymbolicLinkAtPath:lockfile
                                                                 error:NULL])
    return true;

  return false;
}
