 // Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_ACCOUNT_SYNC_MANAGER_CONSUMER_H_
#define IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_ACCOUNT_SYNC_MANAGER_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/sync/manager/vivaldi_session_simplified_state.h"

// VivaldiAccountSyncManagerConsumer provides all methods required to handle
// the UI related to auth to vivaldi account and sync.
// The methods are all optional and all methods may or may not need
// implementation on each screen and should only implement the required ones
// for the UI.
// New states or methods related to sync and account manager should be added
// here.
@protocol VivaldiAccountSyncManagerConsumer<NSObject>

#pragma mark:- Consumer observer
@optional
- (void)onVivaldiSessionUpdated:(VivaldiSessionSimplifiedState)sessionState;

#pragma mark:- Vivaldi Account Manager Observers
@optional
- (void)onVivaldiAccountUpdated;
@optional
- (void)onTokenFetchSucceeded;
@optional
- (void)onTokenFetchFailed;

#pragma mark:- Vivaldi Sync Service Observers
@optional
- (void)onVivaldiSyncStateChanged;
@optional
- (void)onVivaldiSyncCycleCompleted;

 @end

 #endif  // IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_ACCOUNT_SYNC_MANAGER_CONSUMER_H_
