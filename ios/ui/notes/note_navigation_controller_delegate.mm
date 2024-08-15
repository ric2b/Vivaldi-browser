// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_navigation_controller_delegate.h"

#import "base/apple/foundation_util.h"


@implementation NoteNavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {

      UIViewController<UIAdaptivePresentationControllerDelegate>*
          adaptiveViewController = base::apple::ObjCCast<
              UIViewController<UIAdaptivePresentationControllerDelegate>>(
              viewController);
      navigationController.presentationController.delegate =
          adaptiveViewController;
}

@end
