// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "importer/chromium_profile_lock.h"

#include <stack>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/files/file_util.h"

void ChromiumProfileLock::Init() {

}

void ChromiumProfileLock::Lock() {

}

void ChromiumProfileLock::Unlock() {

}

bool ChromiumProfileLock::HasAcquired() {
  NSString* lockfile = base::apple::FilePathToNSString(lock_file_);
  if (![[NSFileManager defaultManager] destinationOfSymbolicLinkAtPath:lockfile
                                                                 error:NULL])
    return true;

  return false;
}
