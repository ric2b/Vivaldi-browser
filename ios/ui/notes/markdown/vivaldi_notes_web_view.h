// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NOTES_MARKDOWN_VIVALDI_NOTES_WEB_VIEW_H_
#define IOS_UI_NOTES_MARKDOWN_VIVALDI_NOTES_WEB_VIEW_H_

#import <WebKit/WebKit.h>

@protocol MarkdownInputViewProvider;

// Webview for showing custom keyboards & toolbars in markdown editor
@interface VivaldiNotesWebView : WKWebView

@property(nonatomic, weak) id<MarkdownInputViewProvider> inputViewProvider;

@end

#endif  // IOS_UI_NOTES_MARKDOWN_VIVALDI_NOTES_WEB_VIEW_H_
