// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_MARKDOWN_MARKDOWN_KEYBOARD_VIEW_PROVIDER_H_
#define IOS_UI_NOTES_MARKDOWN_MARKDOWN_KEYBOARD_VIEW_PROVIDER_H_

#import "ios/ui/notes/markdown/markdown_webview_input_view_protocols.h"

@interface MarkdownKeyboardViewProvider
    : UIView <MarkdownResponderInputView, MarkdownInputViewProvider>

- (instancetype)initWithInputView:(UIView*)keyboard
                    accessoryView:(UIToolbar*)toolbar;

@property(nonatomic, assign) BOOL showMarkdownKeyboard;

@property(nonatomic, readonly) id<MarkdownResponderInputView>
    responderInputView;

@end

#endif  // IOS_UI_NOTES_MARKDOWN_MARKDOWN_KEYBOARD_VIEW_PROVIDER_H_