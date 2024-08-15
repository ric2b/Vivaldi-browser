// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_CONSTANTS_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_CONSTANTS_H_

#import "UIKit/UIKit.h"
#import <Foundation/Foundation.h>

#pragma mark - ICONS
// Image name for the bookmark folder icon.
extern NSString* vBookmarksFolderIcon;
// Image name for the speed dial folder icon.
extern NSString* vSpeedDialFolderIcon;
// Image name for trash folder icon
extern NSString* vBookmarkTrashFolder;
// Image name for the folder selection chevron.
extern NSString* vBookmarkFolderSelectionChevron;
// Image name for the folder selection checkmark.
extern NSString* vBookmarkFolderSelectionCheckmark;
// Image name for add folder
extern NSString* vBookmarkAddFolder;
// Image name for add group illustration
extern NSString* vBookmarkAddGroupIllustration;

#pragma mark - COLOR
// Underline color for bookmark text field underline
extern NSString* vBookmarkTextFieldUnderlineColor;

#pragma mark - SIZE
// Corner radius for the bookmark body containr view
extern CGFloat vBookmarkBodyCornerRadius;
// Bookmark editor text field height
extern CGFloat vBookmarkTextViewHeight;
// Bookmark folder selection header view height
extern CGFloat vBookmarkFolderSelectionHeaderViewHeight;

#pragma mark - OTHERS
// Maximum number of entries to fetch when searching on bookmarks folder page.
extern const int vMaxBookmarkFolderSearchResults;

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_CONSTANTS_H_
