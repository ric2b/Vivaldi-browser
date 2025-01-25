// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_mediator.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_provider.h"
#import "ios/chrome/browser/features/vivaldi_features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_config.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/most_visited_sites/vivaldi_most_visited_sites_manager.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_consumer.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_swift.h"
#import "prefs/vivaldi_pref_names.h"
#import "url/gurl.h"

using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi_bookmark_kit::SetNodeDescription;
using vivaldi_bookmark_kit::SetNodeNickname;
using vivaldi_bookmark_kit::SetNodeSpeeddial;

// Namespace
namespace {
// Converts NSString entered by the user to a GURL.
GURL ConvertUserDataToGURL(NSString* urlString) {
  if (urlString) {
    return url_formatter::FixupURL(base::SysNSStringToUTF8(urlString),
                                   std::string());
  } else {
    return GURL();
  }
}
}

@interface VivaldiBookmarksEditorMediator ()<BooleanObserver,
                                             VivaldiMostVisitedSitesConsumer> {
  ProfileIOS* _profile;
  // If `_folderNode` is `nullptr`, the user is adding a new folder. Otherwise
  // the user is editing an existing folder.
  const BookmarkNode* _bookmarkNode;
  BookmarkModel* _bookmarkModel;
  // Preference service from the application context.
  PrefService* _prefs;
  // Observer for speed dials visibility state
  PrefBackedBoolean* _showSpeedDials;
  // Whether the user manually changed the folder. In which case it must be
  // saved as last used folder on "save".
  BOOL _manuallyChangedTheFolder;
}

// Favicon loaded to fetch favicons from cache.
@property(nonatomic,assign) FaviconLoader* faviconLoader;
// Manager that provides most visited sites
@property(nonatomic,strong)
    VivaldiMostVisitedSitesManager* mostVisitedSiteManager;
// Most visited items from the MostVisitedSites service currently displayed.
@property(nonatomic,strong) MostVisitedTilesConfig* mostVisitedConfig;

@end

@implementation VivaldiBookmarksEditorMediator

- (instancetype)initWithBookmarkModel:(BookmarkModel*)bookmarkModel
                      bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
                      profile:(ProfileIOS*)profile {
  self = [super init];
  if (self) {
    DCHECK(bookmarkModel);
    DCHECK(bookmarkModel->loaded());

    _bookmarkModel = bookmarkModel;
    _bookmark = bookmarkNode;
    _bookmarkNode = bookmarkNode;
    _profile = profile;

    _prefs = profile->GetPrefs();

    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForProfile(_profile);

    VivaldiMostVisitedSitesManager* mostVisitedSiteManager =
        [[VivaldiMostVisitedSitesManager alloc]
            initWithProfile:profile];
    mostVisitedSiteManager.consumer = self;
    _mostVisitedSiteManager = mostVisitedSiteManager;
    [_mostVisitedSiteManager start];

    _showSpeedDials =
        [[PrefBackedBoolean alloc]
            initWithPrefService:_prefs
                prefName:vivaldiprefs::kVivaldiStartPageShowSpeedDials];
    [_showSpeedDials setObserver:self];
    [self booleanDidChange:_showSpeedDials];
  }
  return self;
}

- (void)disconnect {
  _bookmarkModel = nullptr;
  _profile = nullptr;

  _faviconLoader = nil;

  [_mostVisitedSiteManager stop];
  _mostVisitedSiteManager.consumer = nil;
  _mostVisitedSiteManager = nil;
  _mostVisitedConfig = nil;

  [_showSpeedDials stop];
  [_showSpeedDials setObserver:nil];
  _showSpeedDials = nil;
}

- (void)dealloc {
  DCHECK(!_bookmarkModel);
}

#pragma mark - Public

- (void)setConsumer:(id<VivaldiBookmarksEditorConsumer>)consumer {
  _consumer = consumer;
  [self prepareTopSitesAndNotifyConsumersIfNeeded];
  [self.consumer setPreferenceShowSpeedDials:[_showSpeedDials value]];
}

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
    SetNodeDescription(_bookmarkModel,
                       _bookmarkNode, descriptionString);

    // Update nickname
    const std::string nicknameString = base::SysNSStringToUTF8(nickname);
    SetNodeNickname(_bookmarkModel,
                    _bookmarkNode, nicknameString);

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
      const std::string descriptionString =
          base::SysNSStringToUTF8(description);
      SetNodeDescription(_bookmarkModel,
                         node, descriptionString);
    }

    // Update the nickname
    if (nickname.length > 0) {
      const std::string nicknameString = base::SysNSStringToUTF8(nickname);
      SetNodeNickname(_bookmarkModel,
                      node, nicknameString);
    }
  }

  [self saveLastUsedFolder:parentNode];
  [self.consumer bookmarksEditorShouldClose];
}

- (void)saveBookmarkFolderWithTitle:(NSString*)title
                         useAsGroup:(BOOL)useAsGroup
                         parentNode:(const bookmarks::BookmarkNode*)parentNode {

  DCHECK(parentNode);
  DCHECK(title.length > 0);
  std::u16string folderTitle = base::SysNSStringToUTF16(title);

  if (_isEditing) {
    DCHECK(_bookmarkNode);
    // Update title if changed
    if (_bookmarkNode->GetTitle() != folderTitle) {
      _bookmarkModel->SetTitle(_bookmarkNode,
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
      // TODO: @prio - Make snackbar works here when item moved.
      bookmark_utils_ios::MoveBookmarks(bookmarksVector,
                                        _bookmarkModel,
                                        parentNode);
      // Move might change the pointer, grab the updated value.
      CHECK_EQ(bookmarksVector.size(), 1u);
      _bookmarkNode = bookmarksVector[0];
    }
  } else {
    DCHECK(!_bookmarkNode);
    _bookmarkNode = _bookmarkModel->AddFolder(
          parentNode, parentNode->children().size(), folderTitle);
    if (useAsGroup)
      SetNodeSpeeddial(_bookmarkModel,
                       _bookmarkNode, YES);
  }

  [self saveLastUsedFolder:parentNode];
  [self.consumer didCreateNewFolder:_bookmarkNode];
  [self.consumer bookmarksEditorShouldClose];
}

- (void)deleteBookmark {
  DCHECK(_isEditing);
  DCHECK(_bookmarkNode);
  std::set<const BookmarkNode*> editedNodes;
  editedNodes.insert(_bookmarkNode);
  [self.snackbarCommandsHandler
      showSnackbarMessage:bookmark_utils_ios::DeleteBookmarksWithUndoToast(
            editedNodes, _bookmarkModel, _profile, FROM_HERE)];
  [self.consumer bookmarksEditorShouldClose];
}

- (void)setPreferenceShowSpeedDials:(BOOL)showSpeedDials {
  if (showSpeedDials != [_showSpeedDials value])
    [_showSpeedDials setValue:showSpeedDials];
}

- (void)manuallyChangeFolder:(const bookmarks::BookmarkNode*)folder {
  _manuallyChangedTheFolder = YES;
}

#pragma mark - VivaldiMostVisitedSitesConsumer
- (void)setMostVisitedTilesConfig:(MostVisitedTilesConfig*)config {
  _mostVisitedConfig = config;
  [self prepareTopSitesAndNotifyConsumersIfNeeded];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _showSpeedDials) {
    [self.consumer
        setPreferenceShowSpeedDials:[observableBoolean value]];
  }
}

#pragma mark - Private
- (void)prepareTopSitesAndNotifyConsumersIfNeeded {
  if (IsTopSitesEnabled()) {
    NSMutableArray<VivaldiBookmarksEditorTopSitesItem*>* topSites =
        [[NSMutableArray alloc] init];
    for (ContentSuggestionsMostVisitedItem* tile in
         _mostVisitedConfig.mostVisitedItems) {
      NSString* urlString = base::SysUTF8ToNSString(tile.URL.spec());
      VivaldiBookmarksEditorTopSitesItem* item =
          [[VivaldiBookmarksEditorTopSitesItem alloc] initWithTitle:tile.title
                                                          urlString:urlString];
      // Fetch favicon asynchronously and notify consumer
      [self loadFaviconForItem:item];
      [topSites addObject:item];
    }
    [self.consumer bookmarksEditorTopSitesDidUpdate:topSites];
  }
}

// Asynchronously loads favicon for given item.
- (void)loadFaviconForItem:(VivaldiBookmarksEditorTopSitesItem*)item {
  // Start loading a favicon.
  __weak VivaldiBookmarksEditorMediator* weakSelf = self;
  const GURL url = ConvertUserDataToGURL(item.url);
  GURL blockURL(url);
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes) {
    VivaldiBookmarksEditorMediator* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    if (attributes && attributes.faviconImage) {
      [item updateFaviconWithImage:attributes.faviconImage];
      [strongSelf.consumer bookmarksEditorTopSiteDidUpdate:item];
    }
  };

  self.faviconLoader->FaviconForPageUrlOrHost(
        blockURL, kDesiredMediumFaviconSizePt, faviconLoadedBlock);
}

- (void)saveLastUsedFolder:(const bookmarks::BookmarkNode*)folder {
  DCHECK(folder);
  DCHECK(folder->is_folder());
  if (_manuallyChangedTheFolder) {
    BookmarkStorageType type = bookmark_utils_ios::GetBookmarkStorageType(
        folder, _bookmarkModel);
    SetLastUsedBookmarkFolder(_prefs, folder, type);
  }
}

@end
