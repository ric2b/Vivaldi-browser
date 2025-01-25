// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_DELEGATE_H_
#define IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_DELEGATE_H_

#import <Foundation/Foundation.h>

@protocol VivaldiFeedbackViewDelegate<NSObject>

@optional
- (void)feedbackViewWillDismiss;
- (void)feedbackViewDidDismiss;

@end

#endif  // IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_DELEGATE_H_
