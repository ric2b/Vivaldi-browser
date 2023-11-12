 // Copyright 2022 Vivaldi Technologies. All rights reserved.

 #ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_CONSUMER_H_
 #define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "components/ad_blocker/adblock_rule_manager.h"
#import "components/ad_blocker/adblock_rule_service.h"

typedef NS_ENUM(NSInteger, ATBFetchResult) {
    FetchResultSuccess,
    FetchResultDownloadFailed,
    FetchResultFileNotFound,
    FetchResultFileReadError,
    FetchResultFileParseError,
    FetchResultFileUnsupported,
    FetchResultFailedSavingParsedRules,
    FetchResultUnknown
};

using adblock_filter::RuleGroup;
using adblock_filter::RuleManager;
using adblock_filter::KnownRuleSource;

/// VivaldiATBConsumer provides all methods required to handle
/// the UI related to ad and tracker blocker.
/// The methods are all optional since not all methos are required on all
/// screens.
/// Each screen should only implement the required ones for the UI.
@protocol VivaldiATBConsumer<NSObject>

@optional
- (void)didRefreshSettingOptions:(NSArray*)options;
@optional
- (void)didRefreshExceptionsList:(NSArray*)exceptions;
@optional
- (void)didRefreshSourcesList:(NSArray*)sources;

#pragma mark:- AdBlocker Backend Model Observer methods
@optional
- (void)ruleServiceStateDidLoad;
@optional
- (void)rulesListDidStartApplying:(RuleGroup)group;
@optional
- (void)rulesListDidEndApplying:(RuleGroup)group;

@optional
- (void)ruleSourceDidUpdate:(uint32_t)key
                      group:(RuleGroup)group
                fetchResult:(ATBFetchResult)fetchResult;
@optional
- (void)ruleSourceDidRemove:(uint32_t)key
                      group:(RuleGroup)group;
@optional
- (void)exceptionListStateDidChange:(RuleGroup)group;
@optional
- (void)exceptionListDidChange:(RuleGroup)group
                          list:(RuleManager::ExceptionsList)list;
@optional
- (void)knownSourceDidAdd:(RuleGroup)group
                      key:(uint32_t)key;
@optional
- (void)knownSourceDidRemove:(RuleGroup)group
                      key:(uint32_t)key;
@optional
- (void)knownSourceDidEnable:(RuleGroup)group
                      key:(uint32_t)key;
@optional
- (void)knownSourceDidDisable:(RuleGroup)group
                      key:(uint32_t)key;

 @end

 #endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_CONSUMER_H_
