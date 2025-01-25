// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_CONSUMER_H_
#define IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_CONSUMER_H_

/// A protocol implemented by consumers to handle changes feedback view.
@protocol VivaldiFeedbackViewConsumer

- (void)issueDidSubmitSuccessfully:(BOOL)success;
- (void)issueSubmissionDidFail;

@end

#endif  // IOS_UI_FEEDBACK_PROMPT_VIVALDI_FEEDBACK_VIEW_CONSUMER_H_
