// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_mediator.h"

#import "base/check.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_item.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

@implementation VivaldiATBMediator
@synthesize consumer = _consumer;

#pragma mark - INITIALIZERS
- (instancetype)init {
  if ((self = [super init])) {

  }
  return self;
}

#pragma mark - PUBLIC METHODS

- (void)startMediating {
  DCHECK(self.consumer);
  [self computeATBSettingOptions];
}

- (void)disconnect {
  self.consumer = nil;
}

- (void)computeATBSettingOptions {

  // The available setting options are fixed:
  // [No Blocking, Block Trackers, Block Ads and Trackers].

  // No blocking option
  NSString* noBlockingTitleString =
    l10n_util::GetNSString(IDS_LEVEL_NO_BLOCKING);
  NSString* noBlockingDescriptionString =
    l10n_util::GetNSString(IDS_LEVEL_NO_BLOCKING_DESCRIPTION);

  VivaldiATBItem* noBlockingOption =
    [[VivaldiATBItem alloc] initWithTitle:noBlockingTitleString
                                            subtitle:noBlockingDescriptionString
                                              option:ATBSettingNoBlocking];

  // Block trackers option
  NSString* blockTrackersTitleString =
    l10n_util::GetNSString(IDS_LEVEL_BLOCK_TRACKERS);
  NSString* blockTrackersDescriptionString =
    l10n_util::GetNSString(IDS_LEVEL_BLOCK_TRACKERS_DESCRIPTION);

  VivaldiATBItem* blockTrackersOption =
    [[VivaldiATBItem alloc]
      initWithTitle:blockTrackersTitleString
           subtitle:blockTrackersDescriptionString
             option:ATBSettingBlockTrackers];

  // Block trackers and ads option
  NSString* blockTrackersAndAdsTitleString =
    l10n_util::GetNSString(IDS_LEVEL_BLOCK_TRACKERS_AND_ADS);
  NSString* blockTrackersAndAdsDescriptionString =
    l10n_util::GetNSString(IDS_LEVEL_BLOCK_TRACKERS_AND_ADS_DESCRIPTION);

  VivaldiATBItem* blockTrackersAndAdsOption =
    [[VivaldiATBItem alloc]
      initWithTitle:blockTrackersAndAdsTitleString
           subtitle:blockTrackersAndAdsDescriptionString
             option:ATBSettingBlockTrackersAndAds];

  NSMutableArray* options =
    [[NSMutableArray alloc] initWithArray:@[noBlockingOption,
                                            blockTrackersOption,
                                            blockTrackersAndAdsOption]];
  [self.consumer refreshSettingOptions:options];
}

@end
