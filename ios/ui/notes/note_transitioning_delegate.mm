// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_transitioning_delegate.h"

#import "base/apple/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NoteTransitioningDelegate

- (UIPresentationController*)
presentationControllerForPresentedViewController:(UIViewController*)presented
                        presentingViewController:(UIViewController*)presenting
                            sourceViewController:(UIViewController*)source {
  /*TableViewPresentationController* controller =
      [[TableViewPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  controller.modalDelegate = self.presentationControllerModalDelegate;
  return controller;*/
  return nil; // TODO
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  /*if (self.presentationControllerModalDelegate) {
    TableViewPresentationController* controller =
        base::mac::ObjCCast<TableViewPresentationController>(
            presented.presentationController);
    // If the expected presentation cannot be dismissed by a touch outside the
    // table view, then use the default UIKit transition.
    if (controller &&
        ![self.presentationControllerModalDelegate
            presentationControllerShouldDismissOnTouchOutside:controller]) {
      return nil;
    }
  }*/

  UITraitCollection* traitCollection = presenting.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  return nil;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  /*if (self.presentationControllerModalDelegate) {
    TableViewPresentationController* controller =
        base::mac::ObjCCast<TableViewPresentationController>(
            dismissed.presentationController);
    // If the current presentation cannot be dismissed by a touch outside the
    // table view, then use the default UIKit transition.
    if (controller &&
        ![self.presentationControllerModalDelegate
            presentationControllerShouldDismissOnTouchOutside:controller]) {
      return nil;
    }
  }*/

  UITraitCollection* traitCollection = dismissed.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  return nil;
}

@end
