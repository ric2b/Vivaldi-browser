// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/nsd/vivaldi_nsd_mediator.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/direct_match/direct_match_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_provider.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_config.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/direct_match/direct_match_service_factory.h"
#import "ios/most_visited_sites/vivaldi_most_visited_sites_manager.h"
#import "ios/ui/ntp/nsd/nsd_swift.h"
#import "ios/ui/ntp/nsd/vivaldi_nsd_consumer.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "url/gurl.h"

using bookmarks::BookmarkNode;

@interface VivaldiNSDMediator ()<VivaldiMostVisitedSitesConsumer> {
  ProfileIOS* _profile;
  BookmarkModel* _bookmarkModel;
  direct_match::DirectMatchService* _directMatchService;
  PrefService* _prefs;
}

// Favicon loaded to fetch favicons from cache.
@property(nonatomic,assign) FaviconLoader* faviconLoader;
// Manager that provides most visited sites
@property(nonatomic,strong)
    VivaldiMostVisitedSitesManager* mostVisitedSiteManager;
// Most visited items from the MostVisitedSites service currently displayed.
@property(nonatomic,strong) MostVisitedTilesConfig* mostVisitedConfig;
// The item that keeps track of the location where new items will be added.
// This only lives on this view and doesn't affect the initial folder incase
// user changes the location by folder selection option.
@property(nonatomic,strong) VivaldiSpeedDialItem* locationFolderItem;

@end

@implementation VivaldiNSDMediator

- (instancetype)initWithBookmarkModel:(BookmarkModel*)bookmarkModel
                              profile:(ProfileIOS*)profile
                   locationFolderItem:(VivaldiSpeedDialItem*)folderItem {
  self = [super init];
  if (self) {
    DCHECK(bookmarkModel);
    DCHECK(bookmarkModel->loaded());

    // Construct the folder item from the initial location item
    VivaldiSpeedDialItem* locationFolderItem =
        [[VivaldiSpeedDialItem alloc]
            initWithBookmark:folderItem.bookmarkNode];
    _locationFolderItem = locationFolderItem;

    _bookmarkModel = bookmarkModel;
    _profile = profile;
    _prefs = profile->GetPrefs();

    _directMatchService =
        direct_match::DirectMatchServiceFactory::GetForProfile(_profile);
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForProfile(profile);

    VivaldiMostVisitedSitesManager* mostVisitedSiteManager =
        [[VivaldiMostVisitedSitesManager alloc]
            initWithProfile:profile];
    mostVisitedSiteManager.consumer = self;
    _mostVisitedSiteManager = mostVisitedSiteManager;
    [_mostVisitedSiteManager start];
  }
  return self;
}

- (void)disconnect {
  _bookmarkModel = nullptr;
  _profile = nullptr;
  _directMatchService = nullptr;

  _faviconLoader = nil;

  [_mostVisitedSiteManager stop];
  _mostVisitedSiteManager.consumer = nil;
  _mostVisitedSiteManager = nil;
  _mostVisitedConfig = nil;
}

- (void)dealloc {
  DCHECK(!_bookmarkModel);
}

#pragma mark - Public

- (void)setConsumer:(id<VivaldiNSDConsumer>)consumer {
  _consumer = consumer;
  [self preparePopularSitesAndNotifyConsumersIfNeeded];
  [self prepareTopSitesAndNotifyConsumersIfNeeded];
  [self.consumer updateLocationFolder:self.locationFolderItem.title
                        folderIsGroup:self.locationFolderItem.isSpeedDial];
}

- (void)saveItemWithTitle:(NSString*)title
                      url:(NSString*)urlString {
  [self save:title url:urlString];
  [self.consumer editorWantsDismissal];
}

- (void)manuallyChangeFolder:(VivaldiSpeedDialItem*)folder {
  self.locationFolderItem = folder;
  [self.consumer updateLocationFolder:self.locationFolderItem.title
                        folderIsGroup:self.locationFolderItem.isSpeedDial];

  // Reload the popular and top sites collection if the location is changed.
  // This resets the selection check for the items.
  [self preparePopularSitesAndNotifyConsumersIfNeeded];
  [self prepareTopSitesAndNotifyConsumersIfNeeded];
}

#pragma mark - VivaldiMostVisitedSitesConsumer
- (void)setMostVisitedTilesConfig:(MostVisitedTilesConfig*)config {
  _mostVisitedConfig = config;
  [self prepareTopSitesAndNotifyConsumersIfNeeded];
}

#pragma mark - VivaldiNSDViewDelegate
- (void)didSelectItemWithTitle:(NSString*)title
                           url:(NSString*)url
                           add:(BOOL)add {
  if (add) {
    [self save:title url:url];
  } else {
    [self deleteItem:url];
  }
}

- (void)didSelectCategory:(VivaldiNSDDirectMatchCategory*)category {
  [self fetchDMUnitsFromCategory:category];
}

#pragma mark - Private
- (void)save:(NSString*)title
         url:(NSString*)urlString {
  GURL url = bookmark_utils_ios::ConvertUserDataToGURL(urlString);
  if (!url.is_valid())
    return;

  if (self.locationFolderItem == nil ||
      self.locationFolderItem.bookmarkNode == nil) {
    return;
  }

  std::u16string titleString = base::SysNSStringToUTF16(title);
  _bookmarkModel->AddURL(
        self.locationFolderItem.bookmarkNode,
        self.locationFolderItem.bookmarkNode->children().size(),
        titleString, url);
}

- (void)deleteItem:(NSString*)urlString {
  std::set<const bookmarks::BookmarkNode*> nodesToDelete =
      [self nodesForURLString:urlString];

  if (!nodesToDelete.empty()) {
    bookmark_utils_ios::DeleteBookmarks(
        nodesToDelete, _bookmarkModel, FROM_HERE);
  }
}

- (void)fetchDMUnitsFromCategory:(VivaldiNSDDirectMatchCategory*)category {
  std::vector<const direct_match::DirectMatchService::DirectMatchUnit*> units =
      _directMatchService->GetDirectMatchesForCategory(category.idValue);

  NSMutableArray<VivaldiNSDDirectMatchItem*> *categoryItems =
      [self matchItemsFromUnits:units];

  [self.consumer directMatchCategoryItemsDidLoad:categoryItems
                                     forCategory:category];
}

- (void)preparePopularSitesAndNotifyConsumersIfNeeded {

  std::vector<const direct_match::DirectMatchService::DirectMatchUnit*> units =
      _directMatchService->GetPopularSites();

  NSMutableArray<VivaldiNSDDirectMatchItem*> *popularSites =
      [self matchItemsFromUnits:units];

  [self.consumer popularSitesDidLoad:popularSites];
}

- (void)prepareTopSitesAndNotifyConsumersIfNeeded {
  NSMutableArray<VivaldiNSDTopSiteItem*>* topSites =
      [[NSMutableArray alloc] init];

  for (ContentSuggestionsMostVisitedItem* tile in
          _mostVisitedConfig.mostVisitedItems) {
    NSString* urlString = base::SysUTF8ToNSString(tile.URL.spec());
    BOOL added = [self isURLAddedToCurrentLocation:urlString];

    VivaldiNSDTopSiteItem* item =
        [[VivaldiNSDTopSiteItem alloc] initWithTitle:tile.title
                                           urlString:urlString
                                               added:added];
    // Fetch favicon asynchronously and notify consumer
    [self loadFaviconForItem:item];
    [topSites addObject:item];
  }
  [self.consumer topSitesDidLoad:topSites];
}

#pragma mark - Private Helpers

/// Returns the Bookmark Nodes for where provided URL is added.
- (std::set<const BookmarkNode*>)nodesForURLString:(NSString*)urlString {
  if (!(_bookmarkModel && _bookmarkModel->loaded())) {
    return {};
  }

  // Convert the URL string to a GURL
  GURL url = bookmark_utils_ios::ConvertUserDataToGURL(urlString);
  if (!url.is_valid()) {
    return {};
  }

  // Get all nodes where the URL is added as a bookmark
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodesVector =
      _bookmarkModel->GetNodesByURL(url);

  // Filter nodes to include only those with the desired parent node
  const bookmarks::BookmarkNode* targetParentNode =
      self.locationFolderItem.bookmarkNode;
  std::set<const bookmarks::BookmarkNode*> nodesUnderParent;

  for (const auto& node : nodesVector) {
    if (node->parent() == targetParentNode) {
      nodesUnderParent.insert(node);
    }
  }

  return nodesUnderParent;
}

/// Returns mapped `VivaldiNSDDirectMatchItem` from the fetched
/// `DirectMatchUnit` from the service.
- (NSMutableArray<VivaldiNSDDirectMatchItem*>*)matchItemsFromUnits:(
  std::vector<const direct_match::DirectMatchService::DirectMatchUnit*>)units {

  NSMutableArray<VivaldiNSDDirectMatchItem*> *items = [NSMutableArray array];

  for (size_t i = 0; i < units.size(); ++i) {
    const auto* unit = units[i];
    if (unit) {
      NSString* title = base::SysUTF8ToNSString(unit->name);
      NSString* urlString = base::SysUTF8ToNSString(unit->redirect_url);
      NSString* faviconPath = base::SysUTF8ToNSString(unit->image_path);
      NSString* name = base::SysUTF8ToNSString(unit->name);
      NSInteger categoryId = unit->category;
      NSInteger position = unit->position;
      BOOL added = [self isURLAddedToCurrentLocation:urlString];

      VivaldiNSDDirectMatchItem* item = [[VivaldiNSDDirectMatchItem alloc]
                                            initWithTitle:title
                                                urlString:urlString
                                                     name:name
                                              faviconPath:faviconPath
                                                 category:categoryId
                                                 position:position
                                                    added:added];
      [items addObject:item];
    }
  }

  return items;
}

/// Returns whether provided URL is added to current location.
- (BOOL)isURLAddedToCurrentLocation:(NSString*)urlString {
  std::set<const bookmarks::BookmarkNode*> nodes =
      [self nodesForURLString:urlString];
  return !nodes.empty();
}

// Asynchronously loads favicon for given item.
- (void)loadFaviconForItem:(VivaldiNSDTopSiteItem*)item {
  __weak VivaldiNSDMediator* weakSelf = self;
  const GURL url = bookmark_utils_ios::ConvertUserDataToGURL(item.url);
  GURL blockURL(url);
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes) {
    VivaldiNSDMediator* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    if (attributes && attributes.faviconImage) {
      // Return nil for fallback SD icon since we want show first character of
      // the title instead of fallback icon in this dialog.
      if (attributes.faviconImage ==
          [UIImage imageNamed:vNTPSDFallbackFavicon]) {
        [item updateFaviconWithImage:nil];
      } else {
        [item updateFaviconWithImage:attributes.faviconImage];
      }
      [strongSelf.consumer topSiteDidUpdate:item];
    }
  };

  self.faviconLoader->FaviconForPageUrlOrHost(
        blockURL, kDesiredMediumFaviconSizePt, faviconLoadedBlock);
}

@end
