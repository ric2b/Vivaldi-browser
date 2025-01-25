// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SITE_TRACKER_PREFS_VIVALDI_SITE_TRACKER_PREFS_MEDIATOR_H_
#define IOS_UI_SITE_TRACKER_PREFS_VIVALDI_SITE_TRACKER_PREFS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/ui/site_tracker_prefs/vivaldi_site_tracker_prefs_view_delegate.h"

class Browser;
@protocol VivaldiATBConsumer;
@protocol VivaldiSiteTrackerPrefsConsumer;

@protocol VivaldiSiteTrackerPrefsViewPresentationDelegate <NSObject>
- (void)viewControllerWantsDismissal;
@end

// The mediator for tracker blocker and site pref settings.
@interface VivaldiSiteTrackerPrefsMediator:
    NSObject<VivaldiSiteTrackerPrefsViewDelegate>

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The consumer of the mediator.
@property(nonatomic, weak) id<VivaldiSiteTrackerPrefsConsumer> consumer;
// Presentation delegate to dismiss view when rules applying is finished for
// the current session.
@property(nonatomic, weak)
    id<VivaldiSiteTrackerPrefsViewPresentationDelegate> presentationDelegate;

// Disconnects settings and observation.
- (void)disconnect;

@end

#endif  // IOS_UI_SITE_TRACKER_PREFS_VIVALDI_SITE_TRACKER_PREFS_MEDIATOR_H_
