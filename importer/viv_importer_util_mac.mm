// Copyright (c) 2014 Vivaldi Technologies AS. All rights reserved


#include "base/files/file_util.h"
#include "importer/viv_importer_utils.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"

base::FilePath GetProfileDir() {
  base::mac::ScopedNSAutoreleasePool pool;
  NSArray* paths = NSSearchPathForDirectoriesInDomains( NSLibraryDirectory, NSUserDomainMask, YES);
  for (NSString* path in paths) {
    NSString * operaPath = [path stringByAppendingPathComponent:@"Opera"];
    BOOL folder;
    if ([[NSFileManager defaultManager] fileExistsAtPath:operaPath isDirectory:&folder] && folder)
      return base::FilePath([operaPath fileSystemRepresentation]);
  }
  return base::FilePath();
}
