//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include <Cocoa/Cocoa.h>

namespace {
NSString* vivaldi_uuid_key = @"vivaldi_user_id";
}  // anonymous namespace

namespace extensions {
bool UtilitiesGetUniqueUserIdFunction::ReadUserIdFromOSProfile(
    std::string* user_id) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  NSString* value = [userDefaults stringForKey:vivaldi_uuid_key];

  if (value == nil)
    return false;

  user_id->assign([value UTF8String]);
  return true;
}

void UtilitiesGetUniqueUserIdFunction::WriteUserIdToOSProfile(
    const std::string& user_id) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  NSString* value = [NSString stringWithUTF8String:user_id.c_str()];
  [userDefaults setObject:value forKey:vivaldi_uuid_key];
  [userDefaults synchronize];
}

bool UtilitiesGetSystemDateFormatFunction::ReadDateFormats(
    vivaldi::utilities::DateFormats* date_formats) {
  return false;
}
}  // namespace extensions
