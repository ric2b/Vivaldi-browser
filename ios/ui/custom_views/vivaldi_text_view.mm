// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/custom_views/vivaldi_text_view.h"

#import "UIKit/UIKit.h"

#import "ios/ui/custom_views/custom_view_constants.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding for the delete button
// In order - Top, Leading, Bottom, Trailing
const UIEdgeInsets deleteTextButtonPadding =
    UIEdgeInsetsMake(0.0, 8.0, 12.0, 12.0);
}

@interface VivaldiTextView()<UITextViewDelegate>
// The textview that backs the view
@property(nonatomic,weak) UITextView* textView;
// Textview text clear button
@property(nonatomic,weak) UIButton* deleteTextButton;

@end

@implementation VivaldiTextView

@synthesize textView = _textView;
@synthesize deleteTextButton = _deleteTextButton;

#pragma mark - INITIALIZER
- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.backgroundColor = UIColor.clearColor;
    [self setUpUI];
  }
  return self;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {
  // Textview
  UITextView* textView = [UITextView new];
  _textView = textView;
  [textView setDelegate: self];
  textView.adjustsFontForContentSizeCategory = YES;
  textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  textView.backgroundColor = UIColor.clearColor;
  textView.textColor = UIColor.labelColor;
  [self addSubview:textView];
  [textView anchorTop:self.topAnchor
              leading:self.leadingAnchor
               bottom:self.bottomAnchor
             trailing:self.trailingAnchor];

  // Delete Text Button
  UIButton* deleteTextButton = [UIButton new];
  _deleteTextButton = deleteTextButton;
  [deleteTextButton setImage:[UIImage
                              systemImageNamed:vTextfieldDeleteIcon]
                    forState:UIControlStateNormal];
  [deleteTextButton setTintColor:UIColor.vSystemGray];
  [deleteTextButton addTarget:self
                       action:@selector(handleDeleteTextButtonTap)
             forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:deleteTextButton];
  [deleteTextButton anchorTop:nil
                      leading:nil
                       bottom:self.bottomAnchor
                     trailing:self.trailingAnchor
                      padding:deleteTextButtonPadding];
  // Hide the delete text button initially
  [deleteTextButton setAlpha:0];
}

#pragma mark - ACTIONS
/// Remove the text from the textview and hide the delete button when delete button is tapped.
- (void)handleDeleteTextButtonTap {
  self.textView.text = nil;
  [self hideDeleteTextButton];
}

-(void)textViewDidBeginEditing:(UITextView *)textView {
  if ([self.textView hasText] && self.deleteTextButton.alpha == 0) {
    [self showDeleteTextButton];
  }
}

-(void)textViewDidEndEditing:(UITextView *)textView {
  [self hideDeleteTextButton];
}

#pragma mark - PRIVATE

/// Triggers the method when textview is changed.
- (void)textViewDidChange {
  if ([self.textView hasText]) {
    [self showDeleteTextButton];
  } else {
    [self hideDeleteTextButton];
  }
}

/// Make delete button visible.
- (void)showDeleteTextButton {
  [UIView animateWithDuration:0.3 animations:^{
    [self.deleteTextButton setAlpha:1.0];
  }];
}

/// Make delete button hidden.
- (void)hideDeleteTextButton {
  [UIView animateWithDuration:0.3 animations:^{
    [self.deleteTextButton setAlpha:0];
  }];
}

#pragma mark - SETTERS
- (void)setText:(NSString*)text {
  self.textView.text = text;
  self.textView.accessibilityLabel = text;
}

- (void)setFocus {
  [self.textView becomeFirstResponder];
}

#pragma mark - GETTERS
- (BOOL)hasText {
  return [self.textView hasText];
}

- (NSString*)getText {
  return self.textView.text;
}

@end
