// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/notes/markdown/vivaldi_notes_web_view.h"

#import <UIKit/UIKit.h>

#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/notes/markdown/markdown_webview_input_view_protocols.h"

@interface VivaldiNotesWebView ()

@end

@implementation VivaldiNotesWebView

#pragma mark - UIResponder

// The markdown keyboard
- (UIView*)inputView {
  id<MarkdownResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(inputView)]) {
    UIView* view = [responderInputView inputView];
    if (view) {
      return view;
    }
  }
  return [super inputView];
}

// The toolbar (phones)
- (UIView*)inputAccessoryView {
  id<MarkdownResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(inputAccessoryView)]) {
    UIView* view = [responderInputView inputAccessoryView];
    if (view) {
      return view;
    }
  }

  return [super inputAccessoryView];
}

// The toolbar (tablets)
- (UITextInputAssistantItem*)inputAssistantItem {
  UITextInputAssistantItem* inputAssistantItem = [super inputAssistantItem];

  if (![VivaldiGlobalHelpers isDeviceTablet]) {
    return inputAssistantItem;
  }

  id<MarkdownResponderInputView> responderInputView =
      self.inputViewProvider.responderInputView;
  if ([responderInputView respondsToSelector:@selector(getToolbarLeadGroup)]) {
    UIBarButtonItemGroup* leadGroup = [responderInputView getToolbarLeadGroup];
    inputAssistantItem.leadingBarButtonGroups = @[ leadGroup ];
  }
  if ([responderInputView respondsToSelector:@selector(getToolbarTrailGroup)]) {
    UIBarButtonItemGroup* trailGroup =
        [responderInputView getToolbarTrailGroup];
    inputAssistantItem.trailingBarButtonGroups = @[ trailGroup ];
  }
  inputAssistantItem.allowsHidingShortcuts = NO;

  return inputAssistantItem;
}

@end
