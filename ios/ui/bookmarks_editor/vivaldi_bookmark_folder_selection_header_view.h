// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_FOLDER_SELECTION_HEADER_VIEW_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_FOLDER_SELECTION_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

@protocol VivaldiBookmarkFolderSelectionHeaderViewDelegate
- (void)didChangeShowOnlySpeedDialFoldersState:(BOOL)show;
- (void)searchBarTextDidChange:(NSString*)searchText;
@end

// A view to hold folder selection view for the bookmark editor
@interface VivaldiBookmarkFolderSelectionHeaderView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

//// DELEGATE
@property (nonatomic,weak)
  id<VivaldiBookmarkFolderSelectionHeaderViewDelegate> delegate;

// SETTERS
/// Sets the show only speed dial toggle state
- (void)setShowOnlySpeedDialFolder:(BOOL)show;

// GETTERS
/// Returns whether only speed dial folders should be visible.
- (bool)showOnlySpeedDialFolder;

@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_FOLDER_SELECTION_HEADER_VIEW_H_
