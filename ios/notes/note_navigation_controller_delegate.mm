// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/notes/note_navigation_controller_delegate.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NoteNavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {

      UIViewController<UIAdaptivePresentationControllerDelegate>*
          adaptiveViewController = base::mac::ObjCCast<
              UIViewController<UIAdaptivePresentationControllerDelegate>>(
              viewController);
      navigationController.presentationController.delegate =
          adaptiveViewController;
}

@end
