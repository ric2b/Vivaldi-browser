// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NOTES_NOTE_FOLDER_SELECTION_HEADER_VIEW_H_
#define IOS_UI_NOTES_NOTE_FOLDER_SELECTION_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

@protocol NoteFolderSelectionHeaderViewDelegate
- (void)searchBarTextDidChange:(NSString*)searchText;
@end

// A view to hold the folder selection view for the note editor
@interface NoteFolderSelectionHeaderView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

//// DELEGATE
@property (nonatomic,weak)
  id<NoteFolderSelectionHeaderViewDelegate> delegate;

@end

#endif  // IOS_UI_NOTES_NOTE_FOLDER_SELECTION_HEADER_VIEW_H_
