// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_CUSTOM_VIEWS_VIVALDI_TEXT_FIELD_VIEW_H_
#define IOS_UI_CUSTOM_VIEWS_VIVALDI_TEXT_FIELD_VIEW_H_

#import <UIKit/UIKit.h>

// Enum for URL validation type. Generic URL and Domain has different
// rule for validation.
typedef NS_ENUM(NSUInteger, URLValidationType) {
  URLTypeDomain = 0,
  URLTypeGeneric = 1,
};

// A view to hold textfield with Vivaldi style
@interface VivaldiTextFieldView : UIView

- (instancetype)initWithPlaceholder:(NSString*)placeholder
  NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Boolean to keep track whether to validate scheme in URL mode.
@property(nonatomic,assign) BOOL validateScheme;

// SETTERS
/// Sets the placeholder after initialization.
- (void)setPlaceholder:(NSString*)placeholder;
/// Sets the texts on the textfield
- (void)setText:(NSString*)text;
/// Sets the textfield into URL mode. This means change the
/// keyboard layout to URL also auto caps is turned off.
- (void)setURLMode;
/// Set URL Validation type. e.g. Domain or Generic URL.
- (void)setURLValidationType:(URLValidationType)type;
/// Set focus and opens the keyboard
- (void)setFocus;
/// Set auto correct disabled.
/// Preferred in certain places like adding domain or exceptions
/// in Adblocker settings.
- (void)setAutoCorrectDisabled:(BOOL)disable;

// GETTERS
- (BOOL)hasText;
- (NSString*)getText;

@end

#endif  // IOS_UI_CUSTOM_VIEWS_VIVALDI_TEXT_FIELD_VIEW_H_
