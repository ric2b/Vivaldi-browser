// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_MODAL_PAGE_MODAL_PAGE_COORDINATOR_H_
#define IOS_UI_MODAL_PAGE_MODAL_PAGE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;

// Coordinator to present a webpage in a modal view.
@interface ModalPageCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                   pageURL:(NSURL*)url
                                     title:(NSString*)title
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;


@end

#endif  // IOS_UI_MODAL_PAGE_MODAL_PAGE_COORDINATOR_H_
