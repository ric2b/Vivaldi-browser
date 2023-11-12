// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_navigation_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/notes/note_ui_constants.h"
#import "ios/ui/notes/note_utils_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NoteNavigationController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];
  self.navigationBar.accessibilityIdentifier = kNoteNavigationBarIdentifier;
}

- (BOOL)disablesAutomaticKeyboardDismissal {
  // This allows us to hide the keyboard when controllers are being displayed in
  // a modal form sheet on the iPad.
  return NO;
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

@end
