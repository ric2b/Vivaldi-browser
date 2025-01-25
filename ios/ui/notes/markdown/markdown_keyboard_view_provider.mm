// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/markdown/markdown_keyboard_view_provider.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/notes/markdown/markdown_toolbar_view.h"

@interface MarkdownKeyboardViewProvider () <MarkdownToolbarViewDelegate> {
}

@property(nonatomic, strong) UIView* keyboard;
@property(nonatomic, strong) MarkdownToolbarView* toolbar;
@property(nonatomic, assign) BOOL keyboardIsVisible;

@end

@implementation MarkdownKeyboardViewProvider

@synthesize keyboard = _keyboard;
@synthesize toolbar = _toolbar;
@synthesize keyboardIsVisible = _keyboardIsVisible;
@synthesize showMarkdownKeyboard = _showMarkdownKeyboard;

- (instancetype)initWithInputView:(UIView*)keyboard
                    accessoryView:(MarkdownToolbarView*)toolbar {
  self = [super init];
  if (self) {
    _responderInputView = self;
    _keyboard = keyboard;
    _toolbar = toolbar;
    self.toolbar.markdownDelegate = self;
    _keyboardIsVisible = NO;
    _showMarkdownKeyboard = NO;

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillShow:)
               name:UIKeyboardWillShowNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillHide:)
               name:UIKeyboardWillHideNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  self.toolbar.markdownDelegate = nil;
  _keyboard = nil;
  _toolbar = nil;

  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIKeyboardWillHideNotification
              object:nil];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIKeyboardWillShowNotification
              object:nil];
}

#pragma mark - MarkdownResponderInputView

- (UIView*)inputView {
  if (self.showMarkdownKeyboard) {
    return self.keyboard;
  }

  return nil;
}

- (UIInputViewController*)inputViewController {
  return nil;
}

- (UIView*)inputAccessoryView {
  if ([VivaldiGlobalHelpers isDeviceTablet]) {
    return nil;
  }
  return self.toolbar;
}

- (UIInputViewController*)inputAccessoryViewController {
  return nil;
}

- (UIBarButtonItemGroup*)getToolbarLeadGroup {
  return self.toolbar.leadGroup;
}

- (UIBarButtonItemGroup*)getToolbarTrailGroup {
  return self.toolbar.trailGroup;
}

#pragma mark - Keyboard Notifications

- (void)keyboardWillShow:(NSNotification*)notification {
  NSValue* FrameEnd =
      [[notification userInfo] valueForKey:UIKeyboardFrameEndUserInfoKey];
  CGRect frameEndRect = [FrameEnd CGRectValue];

  if (frameEndRect.size.height > 0 &&
      self.keyboard.frame.size.height !=
          frameEndRect.size.height - self.toolbar.frame.size.height) {
    self.keyboard.frame =
        CGRectMake(0, self.toolbar.frame.size.height, frameEndRect.size.width,
                   frameEndRect.size.height - self.toolbar.frame.size.height);
  }

  if (self.keyboardIsVisible)
    return;
  self.keyboardIsVisible = YES;
  [GetFirstResponder() reloadInputViews];
}

- (void)keyboardWillHide:(NSNotification*)notification {
  if (!self.keyboardIsVisible)
    return;
  self.keyboardIsVisible = NO;
  [GetFirstResponder() reloadInputViews];
}

#pragma mark - MarkdownToolbarViewDelegate

- (void)openMarkdownInputView {
  if (self.showMarkdownKeyboard)
    return;
  self.showMarkdownKeyboard = YES;
  [GetFirstResponder() reloadInputViews];
}

- (void)closeMarkdownInputView {
  if (!self.showMarkdownKeyboard)
    return;
  self.showMarkdownKeyboard = NO;
  [GetFirstResponder() reloadInputViews];
}

- (void)closeKeyboard {
  [GetFirstResponder() resignFirstResponder];
}

@end
