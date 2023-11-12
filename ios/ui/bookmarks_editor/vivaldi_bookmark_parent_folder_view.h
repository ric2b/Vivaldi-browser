// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_PARENT_FOLDER_VIEW_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_PARENT_FOLDER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

@protocol VivaldiBookmarkParentFolderViewDelegate
- (void) didTapParentFolder;
@end

// A view to hold folder selection view for the bookmark editor
@interface VivaldiBookmarkParentFolderView : UIView

- (instancetype)initWithTitle:(NSString*)title NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// DELEGATE
@property (nonatomic, weak) id<VivaldiBookmarkParentFolderViewDelegate> delegate;

// SETTERS
/// Hides the 'Use as Speed Dial Folder' section when set to true. When a
/// bookmark URL is created this section should not be visible. This section
/// should only be visible when a bookmark FOLDER is being created.
- (void)setSpeedDialSelectionHidden:(BOOL)hide;
/// Updates the folder name and icon with selected item's parent.
- (void)setParentFolderAttributesWithItem:(const VivaldiSpeedDialItem*)item;
/// Updates speed dial state for the opened item.
- (void)setChildrenAttributesWithItem:(const VivaldiSpeedDialItem*)item;

// GETTERS
/// Returns whether new folder should be created as a speed dial folder
- (bool)isUseAsSpeedDialFolder;

@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_PARENT_FOLDER_VIEW_H_
