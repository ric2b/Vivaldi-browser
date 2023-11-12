// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_CUSTOM_VIEWS_VIVALDI_TEXT_VIEW_H_
#define IOS_UI_CUSTOM_VIEWS_VIVALDI_TEXT_VIEW_H_

#import <UIKit/UIKit.h>

// A view to hold textview with Vivaldi style
@interface VivaldiTextView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// SETTERS
/// Sets the texts on the textview
- (void)setText:(NSString*)text;
- (void)setFocus;

// GETTERS
- (BOOL)hasText;
- (NSString*)getText;

@end

#endif  // IOS_UI_CUSTOM_VIEWS_VIVALDI_TEXT_VIEW_H_
