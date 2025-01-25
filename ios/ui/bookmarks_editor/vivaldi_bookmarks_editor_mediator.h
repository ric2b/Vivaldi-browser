// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_MEDIATOR_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_consumer.h"

@protocol VivaldiBookmarksEditorConsumer;
class ProfileIOS;
@protocol SnackbarCommands;

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

// Mediator for the bookmark editor
@interface VivaldiBookmarksEditorMediator :
    NSObject<VivaldiBookmarksEditorConsumer>

// BookmarkNode to edit.
@property(nonatomic, readonly) const bookmarks::BookmarkNode* bookmark;
// Consumer to reflect user’s change in the model.
@property(nonatomic, weak) id<VivaldiBookmarksEditorConsumer> consumer;
// Handler for snackbar commands.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;
@property(nonatomic, assign) BOOL isEditing;

- (instancetype)
    initWithBookmarkModel:(BookmarkModel*)bookmarkModel
             bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
             profile:(ProfileIOS*)profile
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// Public
- (void)saveBookmarkWithTitle:(NSString*)title
                          url:(NSString*)urlString
                     nickname:(NSString*)nickname
                  description:(NSString*)description
                   parentNode:(const bookmarks::BookmarkNode*)parentNode;

- (void)saveBookmarkFolderWithTitle:(NSString*)title
                         useAsGroup:(BOOL)useAsGroup
                         parentNode:(const bookmarks::BookmarkNode*)parentNode;

- (void)deleteBookmark;

- (void)setPreferenceShowSpeedDials:(BOOL)showSpeedDials;

// Changes `self.folder`, updates the UI accordingly.
// The change is not committed until the user taps the Save button.
// Save this folder as last used by user in preferences
// kIosBookmarkLastUsedFolderReceivingBookmarks and
// kIosBookmarkLastUsedStorageReceivingBookmarks on Save.
- (void)manuallyChangeFolder:(const bookmarks::BookmarkNode*)folder;

@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_MEDIATOR_H_
