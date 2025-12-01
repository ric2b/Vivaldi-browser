// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SEARCH_ENGINE_EDITOR_VIVALDI_SEARCH_ENGINE_EDITOR_COORDINATOR_H_
#define IOS_UI_SETTINGS_SEARCH_ENGINE_EDITOR_VIVALDI_SEARCH_ENGINE_EDITOR_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
class TemplateURL;
@class VivaldiSearchEngineEditorCoordinator;

@protocol VivaldiSearchEngineEditorCoordinatorDelegate
- (void)searchEngineEditorShouldDismiss:
    (VivaldiSearchEngineEditorCoordinator*)coordinator;
@end

@interface VivaldiSearchEngineEditorCoordinator : ChromeCoordinator

// Designated initializer.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                       isEditing:(BOOL)isEditing
                                     editingItem:(const TemplateURL*)editingItem
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, weak) id<VivaldiSearchEngineEditorCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_UI_SETTINGS_SEARCH_ENGINE_EDITOR_VIVALDI_SEARCH_ENGINE_EDITOR_COORDINATOR_H_
