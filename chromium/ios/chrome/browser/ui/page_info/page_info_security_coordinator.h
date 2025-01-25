// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SECURITY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SECURITY_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Vivaldi
@protocol PageInfoCommands;
// End Vivaldi

@class PageInfoSiteSecurityDescription;
@protocol PageInfoPresentationCommands;

// The coordinator for page info's security subpage.
@interface PageInfoSecurityCoordinator : ChromeCoordinator

// Handler for actions within the Page Info UI.
@property(nonatomic, weak) id<PageInfoPresentationCommands>
    pageInfoPresentationHandler;

// Vivaldi
// Handler for actions related to the entire Page Info UI such as showing or
// dismissing the entire UI.
@property(nonatomic, weak) id<PageInfoCommands> pageInfoCommandsHandler;
@property(nonatomic, assign) BOOL openedViaSiteTrackerPrefModal;
// End Vivaldi

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                         siteSecurityDescription:
                             (PageInfoSiteSecurityDescription*)
                                 siteSecurityDescription
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SECURITY_COORDINATOR_H_
