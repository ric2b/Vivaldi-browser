// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NOTES_MARKDOWN_MARKDOWN_WEBVIEW_INPUT_VIEW_PROTOCOLS_H_
#define IOS_UI_NOTES_MARKDOWN_MARKDOWN_WEBVIEW_INPUT_VIEW_PROTOCOLS_H_

#import <UIKit/UIKit.h>

@protocol MarkdownResponderInputView <NSObject>

@optional
// Used to show markdown input and toolbar views.
- (UIView*)inputView;
- (UIInputViewController*)inputViewController;
- (UIView*)inputAccessoryView;
- (UIInputViewController*)inputAccessoryViewController;

- (UIBarButtonItemGroup*)getToolbarLeadGroup;
- (UIBarButtonItemGroup*)getToolbarTrailGroup;

@end

@protocol MarkdownInputViewProvider <NSObject>

// The actual object implementing the input view methods.
@property(nonatomic, readonly) id<MarkdownResponderInputView>
    responderInputView;

@end

#endif  // IOS_UI_NOTES_MARKDOWN_MARKDOWN_WEBVIEW_INPUT_VIEW_PROTOCOLS_H_
