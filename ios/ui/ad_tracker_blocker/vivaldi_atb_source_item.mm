// Copyright 2023 Vivaldi Technologies. All rights reserved.
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_item.h"

#import "UIKit/UIKit.h"

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;
using l10n_util::GetNSStringF;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface VivaldiATBSourceItem()
// Date formatter for last updated time.
@property(nonatomic, strong) NSDateFormatter* formatter;
@end

@implementation VivaldiATBSourceItem
@synthesize formatter = _formatter;

- (instancetype)init {
  self = [super init];
  if (self) {
    // TODO: @prio - Create a DateFormatter class to use across app.
    self.formatter = [[NSDateFormatter alloc] init];
    [self.formatter setDateFormat:@"MMM dd, yyyy hh:mm a"];
  }
  return self;
}

/// Returns the subtitle for the source. It can have different strings
/// based on different state.
/// 1: Return last updated time if the source is enabled and source is fetched.
/// 2: Return 'Fetching' state when source is enabled and a fetch is ongoing.
/// 3: Return 'Not Fetched' when source is enabled fetch is failed.
/// 4: Return 'Disabled' when source is disabled.
- (NSString*)subtitle {
  NSString* checksum = [NSString
                          stringWithUTF8String:_rules_list_checksum.c_str()];
  if ([checksum length] != 0) {
    NSString* updatedAt = [self.formatter
                            stringFromDate:_last_update.ToNSDate()];
    return GetNSStringF(IDS_VIVALDI_IOS_BLOCKER_LIST_DATE_UPDATED,
                        base::SysNSStringToUTF16(updatedAt));
  } else {
    if (_is_fetching) {
      return GetNSString(IDS_VIVALDI_IOS_BLOCKER_LIST_FETCHING);
    }

    return _is_loaded ?
      GetNSString(IDS_VIVALDI_IOS_BLOCKER_LIST_NOT_FETCHED) :
      GetNSString(IDS_VIVALDI_IOS_BLOCKER_LIST_DISABLED);
  }
}

@end
