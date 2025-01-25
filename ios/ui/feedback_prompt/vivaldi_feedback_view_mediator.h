// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_MEDIATOR_H_
#define IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/ui/feedback_prompt/vivaldi_feedback_prompt_swift.h"

@protocol VivaldiFeedbackViewConsumer;

// The mediator for feedback prompt view.
@interface VivaldiFeedbackViewMediator: NSObject

- (instancetype)init NS_DESIGNATED_INITIALIZER;

// The consumer of the mediator.
@property(nonatomic, weak) id<VivaldiFeedbackViewConsumer> consumer;

// Method to call remote server with submitted data
- (void)handleSubmitTapWithParentIssue:(VivaldiFeedbackIssue*)parentIssue
                            childIssue:(VivaldiFeedbackIssue*)childIssue
                               message:(NSString*)message;

// Disconnects settings and observation.
- (void)disconnect;

@end

#endif  // IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_MEDIATOR_H_
