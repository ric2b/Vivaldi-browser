// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_COORDINATOR_H_
#define IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_consumer.h"
#import "ios/ui/feedback_prompt/vivaldi_feedback_view_entry_point.h"

class Browser;
@protocol VivaldiFeedbackViewDelegate;

/// Coordinator for Feedback Prompt
@interface VivaldiFeedbackViewCoordinator : ChromeCoordinator

// Designated initializers.
- (instancetype)initWithBaseNavigationController:
    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                      entryPoint:
                          (VivaldiFeedbackViewEntryPoint)entryPoint
                                    allowsCancel:(BOOL)allowsCancel;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entryPoint:
                          (VivaldiFeedbackViewEntryPoint)entryPoint
                              allowsCancel:(BOOL)allowsCancel
NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate
@property (nonatomic, weak) id<VivaldiFeedbackViewDelegate> delegate;

@end

#endif  // IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_COORDINATOR_H_
