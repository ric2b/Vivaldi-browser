// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_COORDINATOR_H_
#define IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/panel/panel_interaction_controller.h"
#import "ios/ui/translate/vivaldi_translate_entry_point.h"

class Browser;

@protocol VivaldiTranslateCoordinatorDelegate<NSObject>
- (void)translateViewDidDismiss;
@end

// This class is the coordinator for the translate.
@interface VivaldiTranslateCoordinator: ChromeCoordinator

// Creates a coordinator configured to open translate view using the
// base `viewController`, a `browser`, `selectedText` which can be nil,
// `entryPoint` to decide the view config
// and `originView` and `originRect` from which the scenario was triggered.
// This initializer also uses the `originView`'s bounds and
// `originRect` to position the activity view popover on iPad.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                  presentingViewController:
        (UIViewController*)presentingViewController
                                 browser:(Browser*)browser
                              entryPoint:(VivaldiTranslateEntryPoint)entryPoint
                            selectedText:(NSString*)selectedText
                              originView:(UIView*)originView
                              originRect:(CGRect)originRect
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                  presentingViewController:
        (UIViewController*)presentingViewController
                                   browser:(Browser*)browser
                              entryPoint:(VivaldiTranslateEntryPoint)entryPoint
                              selectedText:(NSString*)selectedText;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate
@property (nonatomic, weak) id<VivaldiTranslateCoordinatorDelegate> delegate;

// Delegate for panel
@property(nonatomic, weak) PanelInteractionController* panelDelegate;

// The navigation controller displaying the controller.
@property(nonatomic, strong) UINavigationController* navigationController;

/// Returns whether presenting view controller should open
/// in full size. Returns `YES` for the case when texts are too long
/// and not meaningfully visible on half sheet.
- (BOOL)shouldOpenFullSheet;

@end

#endif  // IOS_UI_TRANSLATE_VIVALDI_TRANSLATE_COORDINATOR_H_
