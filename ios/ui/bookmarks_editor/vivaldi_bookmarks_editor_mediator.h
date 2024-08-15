// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_MEDIATOR_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol VivaldiBookmarksEditorConsumer;
class ChromeBrowserState;
@protocol SnackbarCommands;

class LegacyBookmarkModel;
namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Mediator for the bookmark editor
@interface VivaldiBookmarksEditorMediator : NSObject

// BookmarkNode to edit.
@property(nonatomic, readonly) const bookmarks::BookmarkNode* bookmark;
// Consumer to reflect user’s change in the model.
@property(nonatomic, weak) id<VivaldiBookmarksEditorConsumer> consumer;
// Handler for snackbar commands.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;
@property(nonatomic, assign) BOOL isEditing;

- (instancetype)
    initWithBookmarkModel:(LegacyBookmarkModel*)bookmarkModel
             bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
             browserState:(ChromeBrowserState*)browserState
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

@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_MEDIATOR_H_
