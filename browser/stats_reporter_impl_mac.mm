// Copyright (c) 2019 Vivaldi. All rights reserved.

#include "browser/stats_reporter_impl.h"

#include <Cocoa/Cocoa.h>

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/files/file_path.h"

namespace {
NSString* vivaldi_uuid_key = @"vivaldi_user_id";
}  // anonymous namespace

namespace vivaldi {

// static
std::string StatsReporterImpl::GetUserIdFromLegacyStorage() {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  NSString* value = [userDefaults stringForKey:vivaldi_uuid_key];

  if (value == nil)
    return "";

  return [value UTF8String];
}

// static
base::FilePath StatsReporterImpl::GetReportingDataFileDir() {
  base::FilePath dir;
  base::PathService::Get(base::DIR_HOME, &dir);
  return dir.Append(".local/share");
}

}  // namespace vivaldi