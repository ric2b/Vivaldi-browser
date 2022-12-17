// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARK_TEXT_FIELD_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARK_TEXT_FIELD_VIEW_H_

#import <UIKit/UIKit.h>

// A view to hold textfield for the bookmark editors
@interface VivaldiBookmarkTextFieldView : UIView

- (instancetype)initWithPlaceholder:(NSString*)placeholder
  NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// SETTERS
/// Sets the texts on the textfield
- (void)setText:(NSString*)text;
/// Sets the textfield into URL mode. This means change the
/// keyboard layout to URL also auto caps is turned off.
- (void)setURLMode;

// GETTERS
- (BOOL)hasText;
- (NSString*)getText;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARK_TEXT_FIELD_VIEW_H_
