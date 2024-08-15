// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_COORDINATOR_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_entry_point.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_consumer.h"
#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

class Browser;

@interface VivaldiBookmarksEditorCoordinator : ChromeCoordinator

// Designated initializers.
- (instancetype)initWithBaseNavigationController:
    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                            item:(VivaldiSpeedDialItem*)item
                                          parent:(VivaldiSpeedDialItem*)parent
                                      entryPoint:
    (VivaldiBookmarksEditorEntryPoint)entryPoint
                                       isEditing:(BOOL)isEditing
                                    allowsCancel:(BOOL)allowsCancel;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      item:(VivaldiSpeedDialItem*)item
                                    parent:(VivaldiSpeedDialItem*)parent
                                entryPoint:
    (VivaldiBookmarksEditorEntryPoint)entryPoint
                                 isEditing:(BOOL)isEditing
                              allowsCancel:(BOOL)allowsCancel
NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;
//// DELEGATE
@property (nonatomic, weak) id<VivaldiBookmarksEditorConsumer> consumer;

// Will provide the necessary UI to create a folder. `YES` by default.
// Should be set before calling `start`.
@property(nonatomic) BOOL allowsNewFolders;

@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_COORDINATOR_H_
