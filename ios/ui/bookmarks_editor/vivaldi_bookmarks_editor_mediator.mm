// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_mediator.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_consumer.h"
#import "url/gurl.h"

using bookmarks::BookmarkNode;
using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi_bookmark_kit::SetNodeDescription;
using vivaldi_bookmark_kit::SetNodeNickname;
using vivaldi_bookmark_kit::SetNodeSpeeddial;

@interface VivaldiBookmarksEditorMediator () {
  ChromeBrowserState* _browserState;
  // If `_folderNode` is `nullptr`, the user is adding a new folder. Otherwise
  // the user is editing an existing folder.
  const BookmarkNode* _bookmarkNode;
  LegacyBookmarkModel* _bookmarkModel;
}
// Flag to ignore bookmark model changes notifications.
@property(nonatomic, assign) BOOL ignoresBookmarkModelChanges;

@end

@implementation VivaldiBookmarksEditorMediator

- (instancetype)initWithBookmarkModel:(LegacyBookmarkModel*)bookmarkModel
                      bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
                      browserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    DCHECK(bookmarkModel);
    DCHECK(bookmarkModel->loaded());

    DCHECK(bookmarkNode);
    _bookmarkModel = bookmarkModel;
    _bookmark = bookmarkNode;
    _bookmarkNode = bookmarkNode;
    _browserState = browserState;
  }
  return self;
}

- (void)disconnect {
  _bookmarkModel = nullptr;
  _browserState = nullptr;
}

- (void)dealloc {
  DCHECK(!_bookmarkModel);
}

#pragma mark - Public

- (void)saveBookmarkWithTitle:(NSString*)title
                          url:(NSString*)urlString
                     nickname:(NSString*)nickname
                  description:(NSString*)description
                   parentNode:(const bookmarks::BookmarkNode*)parentNode {

  if (!bookmark_utils_ios::ConvertUserDataToGURL(urlString).is_valid())
    return;

  GURL url = bookmark_utils_ios::ConvertUserDataToGURL(urlString);
  // If the URL was not valid, the `save` message shouldn't have been sent.
  DCHECK(url.is_valid());

  NSString* bookmarkName = title;
  if (bookmarkName.length == 0) {
    bookmarkName = urlString;
  }
  std::u16string titleString = base::SysNSStringToUTF16(title);

  // If editing an existing item.
  if (_isEditing && _bookmarkNode) {
    // Update title
    _bookmarkModel->SetTitle(_bookmarkNode,
                             titleString,
                             bookmarks::metrics::BookmarkEditSource::kUser);

    // Update URL
    _bookmarkModel->SetURL(_bookmarkNode,
                           url,
                           bookmarks::metrics::BookmarkEditSource::kUser);

    // Update description
    const std::string descriptionString = base::SysNSStringToUTF8(description);
    SetNodeDescription(_bookmarkModel, _bookmarkNode, descriptionString);

    // Update nickname
    const std::string nicknameString = base::SysNSStringToUTF8(nickname);
    SetNodeNickname(_bookmarkModel, _bookmarkNode, nicknameString);

    // Move
    BOOL isMovable = parentNode &&
        !parentNode->parent()->HasAncestor(_bookmarkNode) &&
            (_bookmarkNode->parent()->id() != parentNode->id());

    if (isMovable) {
      _bookmarkModel->Move(_bookmarkNode,
                           parentNode,
                           parentNode->children().size());
    }

  } else {
    // Save a new bookmark
    const BookmarkNode* node =
        _bookmarkModel->AddURL(parentNode,
                               parentNode->children().size(),
                               titleString,
                               url);

    // Update the description
    if (description.length > 0) {
      const std::string descriptionString = base::SysNSStringToUTF8(description);
      SetNodeDescription(_bookmarkModel, node, descriptionString);
    }

    // Update the nickname
    if (nickname.length > 0) {
      const std::string nicknameString = base::SysNSStringToUTF8(nickname);
      SetNodeNickname(_bookmarkModel, node, nicknameString);
    }
  }

  [self.consumer bookmarksEditorShouldClose];
}

- (void)saveBookmarkFolderWithTitle:(NSString*)title
                         useAsGroup:(BOOL)useAsGroup
                         parentNode:(const bookmarks::BookmarkNode*)parentNode {

  DCHECK(parentNode);
  LegacyBookmarkModel* modelForParentFolder =
      bookmark_utils_ios::GetBookmarkModelForNode(parentNode,
                                                  _bookmarkModel,
                                                  _bookmarkModel);
  DCHECK(title.length > 0);
  std::u16string folderTitle = base::SysNSStringToUTF16(title);

  if (_isEditing) {
    DCHECK(_bookmarkNode);
    LegacyBookmarkModel* modelForFolder =
        bookmark_utils_ios::GetBookmarkModelForNode(
            _bookmarkNode, _bookmarkModel, _bookmarkModel);

    // Update title if changed
    if (_bookmarkNode->GetTitle() != folderTitle) {
      modelForFolder->SetTitle(_bookmarkNode,
                               folderTitle,
                               bookmarks::metrics::BookmarkEditSource::kUser);
    }

    // Set speed dial status if changed
    if (GetSpeeddial(_bookmarkNode) != useAsGroup) {
      SetNodeSpeeddial(_bookmarkModel,
                       _bookmarkNode,
                       useAsGroup);
    }


    if (_bookmarkNode->parent() != parentNode) {
      std::vector<const BookmarkNode*> bookmarksVector{_bookmarkNode};
      [self.snackbarCommandsHandler
          showSnackbarMessage:bookmark_utils_ios::MoveBookmarksWithUndoToast(
                bookmarksVector, _bookmarkModel, _bookmarkModel,
                    parentNode, _browserState, /*authenticationService*/ nullptr,
                          /*syncService*/ nullptr)];
      // Move might change the pointer, grab the updated value.
      CHECK_EQ(bookmarksVector.size(), 1u);
      _bookmarkNode = bookmarksVector[0];
    }
  } else {
    DCHECK(!_bookmarkNode);
    _bookmarkNode = modelForParentFolder->AddFolder(
          parentNode, parentNode->children().size(), folderTitle);
    if (useAsGroup)
      SetNodeSpeeddial(_bookmarkModel, _bookmarkNode, YES);
  }

  [self.consumer didCreateNewFolder:_bookmarkNode];
  [self.consumer bookmarksEditorShouldClose];
}

- (void)deleteBookmark {
  DCHECK(_isEditing);
  DCHECK(_bookmarkNode);
  std::set<const BookmarkNode*> editedNodes;
  editedNodes.insert(_bookmarkNode);
  LegacyBookmarkModel* modelForBookmark =
      bookmark_utils_ios::GetBookmarkModelForNode(
            _bookmarkNode, _bookmarkModel,_bookmarkModel);
  [self.snackbarCommandsHandler
      showSnackbarMessage:bookmark_utils_ios::DeleteBookmarksWithUndoToast(
            editedNodes, {modelForBookmark}, _browserState)];
  [self.consumer bookmarksEditorShouldClose];
}

#pragma mark - Properties
- (LegacyBookmarkModel*)bookmarkModel {
  return bookmark_utils_ios::GetBookmarkModelForNode(
      self.bookmark, _bookmarkModel, _bookmarkModel);
}

@end
