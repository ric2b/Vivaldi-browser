// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_ADD_EDIT_URL_VIEW_CONTROLLER_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_ADD_EDIT_URL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmark_add_edit_controller_delegate.h"
#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

// The controller responsible for adding and editing bookmark URL.
@interface VivaldiBookmarkAddEditURLViewController
    : UIViewController

// INITIALIZER
+ (instancetype)initWithBrowser:(Browser*)browser
                           item:(VivaldiSpeedDialItem*)item
                         parent:(VivaldiSpeedDialItem*)parent
                      isEditing:(BOOL)isEditing
                   allowsCancel:(BOOL)allowsCancel;

// DELEGATE
@property (nonatomic,weak)
    id<VivaldiBookmarkAddEditControllerDelegate> delegate;

@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_ADD_EDIT_URL_VIEW_CONTROLLER_H_
