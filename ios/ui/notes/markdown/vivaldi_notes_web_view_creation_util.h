// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NOTES_MARKDOWN_VIVALDI_NOTES_WEB_VIEW_CREATION_UTIL_H_
#define IOS_UI_NOTES_MARKDOWN_VIVALDI_NOTES_WEB_VIEW_CREATION_UTIL_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

@class WKWebView;
@protocol MarkdownInputViewProvider;

namespace web {
class BrowserState;

WKWebView* BuildNotesWKWebView(
    CGRect frame,
    BrowserState* browser_state,
    id<MarkdownInputViewProvider> input_view_provider);

}  // namespace web

#endif  // IOS_UI_NOTES_MARKDOWN_VIVALDI_NOTES_WEB_VIEW_CREATION_UTIL_H_
