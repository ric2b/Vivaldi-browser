// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_ADD_EDIT_FOLDER_VIEW_CONTROLLER_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_ADD_EDIT_FOLDER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmark_add_edit_controller_delegate.h"
#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

// The controller responsible for adding and editing bookmark FOLDER.
@interface VivaldiBookmarkAddEditFolderViewController
    : UIViewController

// INITIALIZER
+ (instancetype)initWithBrowser:(Browser*)browser
                           item:(VivaldiSpeedDialItem*)item
                         parent:(VivaldiSpeedDialItem*)parent
                      isEditing:(BOOL)isEditing
                   allowsCancel:(BOOL)allowsCancel;

// DELEGATE
@property (nonatomic, weak)
  id<VivaldiBookmarkAddEditControllerDelegate> delegate;

// Will provide the necessary UI to create a folder. `YES` by default.
// Should be set before calling `start`.
@property(nonatomic) BOOL allowsNewFolders;

@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_ADD_EDIT_FOLDER_VIEW_CONTROLLER_H_
