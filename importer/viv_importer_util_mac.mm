// Copyright (c) 2014 Vivaldi Technologies AS. All rights reserved

#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "importer/viv_importer_utils.h"

NSString* kOperaProfileDirectory = @"Opera";
NSString* kOperaMailDirectory = @"Opera/mail";
NSString* kThunderbirdMailDirectory = @"Thunderbird";

base::FilePath GetDirectory(NSString* directoryName) {
  base::mac::ScopedNSAutoreleasePool pool;
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory,
                                                       NSUserDomainMask, YES);
  for (NSString* path in paths) {
    NSString* operaPath = [path stringByAppendingPathComponent:directoryName];
    BOOL folder;
    if ([[NSFileManager defaultManager] fileExistsAtPath:operaPath
                                             isDirectory:&folder] &&
        folder)
      return base::FilePath([operaPath fileSystemRepresentation]);
  }
  return base::FilePath();
}

base::FilePath GetProfileDir() {
  return GetDirectory(kOperaProfileDirectory);
}

base::FilePath GetMailDirectory() {
  return GetDirectory(kOperaMailDirectory);
}

base::FilePath GetThunderbirdMailDirectory() {
  return GetDirectory(kThunderbirdMailDirectory);
}
