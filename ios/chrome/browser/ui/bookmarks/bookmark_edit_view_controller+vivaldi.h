// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDIT_VIEW_CONTROLLER_VIVALDI_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDIT_VIEW_CONTROLLER_VIVALDI_

#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_text_field_item.h"

@interface BookmarkEditViewController(Vivaldi)

@property(nonatomic, strong) BookmarkTextFieldItem* descriptionItem;

- (void)vivaldiAddDescriptionItem:(NSInteger)type
                        toSection:(NSInteger)section
                  descriptionText:(NSString*)text
                         delegate:(id<BookmarkTextFieldItemDelegate>)delegate;
- (NSString*)inputDescription;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDIT_VIEW_CONTROLLER_VIVALDI_
