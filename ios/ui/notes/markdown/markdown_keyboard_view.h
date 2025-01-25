// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NOTES_MARKDOWN_MARKDOWN_KEYBOARD_VIEW_H_
#define IOS_UI_NOTES_MARKDOWN_MARKDOWN_KEYBOARD_VIEW_H_

#import <UIKit/UIKit.h>

@protocol MarkdownCommandDelegate;

@interface MarkdownKeyboardView : UIView

- (instancetype)initWithFrame:(CGRect)frame
              commandDelegate:(id<MarkdownCommandDelegate>)commandDelegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@property(nonatomic, weak) id<MarkdownCommandDelegate> commandDelegate;

@end

#endif  // IOS_UI_NOTES_MARKDOWN_MARKDOWN_KEYBOARD_VIEW_H_
