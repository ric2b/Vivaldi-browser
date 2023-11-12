// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_MANAGER_VIVALDI_ATB_MANAGER_H_
#define IOS_UI_AD_TRACKER_BLOCKER_MANAGER_VIVALDI_ATB_MANAGER_H_

#import "Foundation/Foundation.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_consumer.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_setting_type.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_item.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_type.h"

/// VivaldiATBManager handles the communication between UI and the backend of AdBlocker.
@interface VivaldiATBManager : NSObject

@property(nonatomic, weak) id<VivaldiATBConsumer> consumer;

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Stops manager.
- (void)disconnect;

#pragma mark - GETTERS
/// Returns the global setting.
- (ATBSettingType)globalBlockingSetting;
/// Returns setting for the give domain.
- (ATBSettingType)blockingSettingForDomain:(NSString*)domain;
/// Returns true if trackers blocking enabled globally.
-(BOOL)isTrackerBlockingEnabled;
/// Returns true if ads blocking enabled globally.
-(BOOL)isAdBlockingEnabled;
/// Compute and returns setting options through consumer.
- (void)getSettingOptions;
/// Returns the exceptions list through consumer.
- (void)getAllExceptionsList;
/// Returns the blocker sources for given type
/// through consumer.
- (void)getBlockerSourcesForSourceType:(ATBSourceType)sourceType;
/// Returns computed source item from key and type
/// through consumer.
- (VivaldiATBSourceItem*)getBlockerSourceForSourceId:(uint32_t)key
                                          sourceType:(ATBSourceType)sourceType;
- (BOOL)isApplyingExceptionRules;

#pragma mark - SETTERS
/// Updates the global ads and tracker blocking settings
/// from given type.
- (void)setExceptionFromBlockingType:(ATBSettingType)blockingType;
/// Adds exception for a given domain.
- (void)setExceptionForDomain:(NSString*)domain
                 blockingType:(ATBSettingType)blockingType;
/// Removes exception for a given domain.
- (void)removeExceptionForDomain:(NSString*)domain;
/// Add rule source for the given blocking type.
- (void)addRuleSource:(NSString*)source
           sourceType:(ATBSourceType)sourceType;
/// Remove rule source of the given key and blocking type.
- (void)removeRuleSourceForKey:(uint32_t)key
                    sourceType:(ATBSourceType)sourceType;
/// Enable rule source of the given key and blocking type.
- (void)enableRuleSourceForKey:(uint32_t)key
                    sourceType:(ATBSourceType)sourceType;
/// Disable rule source of the given key and blocking type.
- (void)disableRuleSourceForKey:(uint32_t)key
                   sourceType:(ATBSourceType)sourceType;
/// Restore preset rule source of the given blocking type.
- (void)restoreRuleSourceForType:(ATBSourceType)sourceType;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_MANAGER_VIVALDI_ATB_MANAGER_H_
