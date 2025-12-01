// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/notes/markdown/vivaldi_notes_web_view.h"

#import <objc/runtime.h>
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

// NOTE(tomas@vivaldi): VIB-1113
// This is to mitigate a bug in webkit
// https://bugs.webkit.org/show_bug.cgi?id=229600
+ (void)allowDisplayingKeyboardWithoutUserAction {
  Class clss = NSClassFromString(@"WKContentView");
  const char* methodSignature =
      "_elementDidFocus:userIsInteracting:blurPreviousNode:"
      "activityStateChanges:userObject:";

  SEL selector = sel_getUid(methodSignature);
  Method method = class_getInstanceMethod(clss, selector);
  IMP original = method_getImplementation(method);
  IMP override = imp_implementationWithBlock(
      ^void(id me, void* arg0, BOOL arg1, BOOL arg2, BOOL arg3, id arg4) {
        ((void (*)(id, SEL, void*, BOOL, BOOL, BOOL, id))original)(
            me, selector, arg0, TRUE, arg2, arg3, arg4);
      });
  method_setImplementation(method, override);
}

@end
