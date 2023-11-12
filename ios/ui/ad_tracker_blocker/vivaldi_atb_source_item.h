// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SOURCE_ITEM_H_
#define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SOURCE_ITEM_H_

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"

// The item model for the ad blocker rule source.
@interface VivaldiATBSourceItem : NSObject

@property(nonatomic, assign) uint32_t key;
@property(nonatomic, strong) NSString* title;
@property(nonatomic, strong) NSString* source_url;
@property(nonatomic, assign) BOOL is_from_url;
@property(nonatomic, assign) base::Time last_update;
@property(nonatomic, assign) std::string rules_list_checksum;
@property(nonatomic, assign) BOOL is_fetching;
@property(nonatomic, assign) BOOL is_enabled;
@property(nonatomic, assign) BOOL is_default;
@property(nonatomic, assign) BOOL is_loaded;
@property(nonatomic, assign) NSInteger list_priority;

#pragma mark: GETTERS
- (NSString*)subtitle;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SOURCE_ITEM_H_
