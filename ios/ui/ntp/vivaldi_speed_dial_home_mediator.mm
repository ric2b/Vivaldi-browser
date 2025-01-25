// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_home_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "chromium/base/containers/stack.h"
#import "components/bookmarks/browser/bookmark_model_observer.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/features/vivaldi_features.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_config.h"
#import "ios/most_visited_sites/vivaldi_most_visited_sites_manager.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/ntp/top_toolbar/top_toolbar_swift.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs_helper.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"
#import "prefs/vivaldi_pref_names.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;
using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi_bookmark_kit::IsSeparator;
using l10n_util::GetNSString;

@interface VivaldiSpeedDialHomeMediator ()<BookmarkModelBridgeObserver,
                                           VivaldiMostVisitedSitesConsumer,
                                           PrefObserverDelegate,
                                           BooleanObserver> {
  // Preference service from the application context.
  PrefService* _prefs;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // The profile for this mediator.
  raw_ptr<ProfileIOS> _profile;
  // Observer for tab bar enabled/disabled state
  PrefBackedBoolean* _tabBarEnabled;
  // Observer for omnibox position
  PrefBackedBoolean* _bottomOmniboxEnabled;
  // Observer for frequently visited pages visibility state
  PrefBackedBoolean* _showFrequentlyVisited;
  // Observer for speed dials visibility state
  PrefBackedBoolean* _showSpeedDials;
  // Observer for start page customize button visibility state
  PrefBackedBoolean* _showCustomizeStartPageButton;
}

// Manager that provides most visited sites
@property(nonatomic,strong)
    VivaldiMostVisitedSitesManager* mostVisitedSiteManager;
// Most visited items from the MostVisitedSites service currently displayed.
@property(nonatomic,strong) MostVisitedTilesConfig* mostVisitedConfig;
// Speed dial folders collection
@property(nonatomic,strong) NSMutableArray* speedDialFolders;
// Bool to keep track the initial loading of the speed dial folders.
@property(nonatomic,assign) BOOL isFirstLoad;
// Bool to keep track of extensive changes.
@property(nonatomic,assign) BOOL runningExtensiveChanges;
// Bool to keep track if top sites result is ready. The results can be empty,
// this only checks if we got a response from backend for the query.
@property(nonatomic,assign) BOOL isTopSitesResultsAvailable;
@end

@implementation VivaldiSpeedDialHomeMediator {
  // The model holding bookmark data.
  base::WeakPtr<BookmarkModel> _bookmarkModel;
  // Bridge to register for bookmark changes in the bookmarkModel.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridge;
}

@synthesize consumer = _consumer;
@synthesize speedDialFolders = _speedDialFolders;
@synthesize isFirstLoad = _isFirstLoad;
@synthesize runningExtensiveChanges = _runningExtensiveChanges;

#pragma mark - INITIALIZERS
- (instancetype)initWithProfile:(ProfileIOS*)profile
                  bookmarkModel:(BookmarkModel*)bookmarkModel {
  if ((self = [super init])) {
    _profile = profile;
    _bookmarkModel = bookmarkModel->AsWeakPtr();
    _bookmarkModelBridge = std::make_unique<BookmarkModelBridge>(
          self, _bookmarkModel.get());

    VivaldiMostVisitedSitesManager* mostVisitedSiteManager =
        [[VivaldiMostVisitedSitesManager alloc]
            initWithProfile:profile];
    mostVisitedSiteManager.consumer = self;
    _mostVisitedSiteManager = mostVisitedSiteManager;

    _prefs = profile->GetPrefs();
    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));

    _prefObserverBridge->ObserveChangesForPreference(
        vivaldiprefs::kVivaldiStartPageLayoutStyle, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        vivaldiprefs::kVivaldiStartPageSDMaximumColumns, &_prefChangeRegistrar);

    _tabBarEnabled =
        [[PrefBackedBoolean alloc]
             initWithPrefService:_prefs
                prefName:vivaldiprefs::kVivaldiDesktopTabsEnabled];
        [_tabBarEnabled setObserver:self];
    [self booleanDidChange:_tabBarEnabled];

    _bottomOmniboxEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:GetApplicationContext()->GetLocalState()
                 prefName:prefs::kBottomOmnibox];
    [_bottomOmniboxEnabled setObserver:self];
    [self booleanDidChange:_bottomOmniboxEnabled];

    _showFrequentlyVisited =
        [[PrefBackedBoolean alloc]
            initWithPrefService:_prefs
                prefName:vivaldiprefs::kVivaldiStartPageShowFrequentlyVisited];
    [_showFrequentlyVisited setObserver:self];

    _showSpeedDials =
        [[PrefBackedBoolean alloc]
            initWithPrefService:_prefs
                       prefName:vivaldiprefs::kVivaldiStartPageShowSpeedDials];
    [_showSpeedDials setObserver:self];
    [self booleanDidChange:_showSpeedDials];

    _showCustomizeStartPageButton =
      [[PrefBackedBoolean alloc]
        initWithPrefService:_prefs
                   prefName:vivaldiprefs::kVivaldiStartPageShowCustomizeButton];
    [_showCustomizeStartPageButton setObserver:self];
    [self booleanDidChange:_showCustomizeStartPageButton];

    [VivaldiStartPagePrefs setPrefService:profile->GetPrefs()];

    self.isTopSitesResultsAvailable = NO;
  }
  return self;
}

#pragma mark - PUBLIC METHODS

- (void)startMediating {
  DCHECK(self.consumer);
  self.isFirstLoad = YES;
  [self computeSpeedDialFolders];
}

- (void)disconnect {
  _profile = nil;
  _bookmarkModel = nullptr;
  _bookmarkModelBridge.reset();
  self.consumer = nil;

  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefs = nil;

  [_mostVisitedSiteManager stop];
  _mostVisitedSiteManager.consumer = nil;
  _mostVisitedSiteManager = nil;
  _mostVisitedConfig = nil;

  [_tabBarEnabled stop];
  [_tabBarEnabled setObserver:nil];
  _tabBarEnabled = nil;

  [_bottomOmniboxEnabled stop];
  [_bottomOmniboxEnabled setObserver:nil];
  _bottomOmniboxEnabled = nil;

  [_showFrequentlyVisited stop];
  [_showFrequentlyVisited setObserver:nil];
  _showFrequentlyVisited = nil;

  [_showSpeedDials stop];
  [_showSpeedDials setObserver:nil];
  _showSpeedDials = nil;

  [_showCustomizeStartPageButton stop];
  [_showCustomizeStartPageButton setObserver:nil];
  _showCustomizeStartPageButton = nil;
}

- (void)removeMostVisited:(VivaldiSpeedDialItem*)item {
  for (ContentSuggestionsMostVisitedItem *tile in
          _mostVisitedConfig.mostVisitedItems) {
    if (tile.URL == item.url) {
      [_mostVisitedSiteManager removeMostVisited:tile];
      break;
    }
  }
}

- (void)computeSpeedDialFolders {
  if (IsTopSitesEnabled()) {
    [_mostVisitedSiteManager start];
  }

  if (_bookmarkModel && _bookmarkModel->loaded())
    [self fetchSpeedDialFolders];
}

- (void)computeSpeedDialChildItems:(VivaldiSpeedDialItem*)item {
  // If an item is provided fetch the children of that item.
  // Otherwise fetch all children of all speed dial folder items.
  if (item) {
    NSArray* childItems =
      [self fetchSpeedDialChildrenOf:item];
    // Return the sorted items based on selected mode
    NSArray* sortedArray = [self sortSpeedDials:childItems
                                         byMode:self.currentSortingMode];
    [self.consumer refreshChildItems:sortedArray
                   topSitesAvailable:self.isTopSitesResultsAvailable];

  } else {

    NSMutableArray* childItemsCollection = [[NSMutableArray alloc] init];
    for (id folder in self.speedDialFolders) {
      NSArray* childItems =
        [self fetchSpeedDialChildrenOf:folder];
      [childItemsCollection addObject:childItems];
    }

    // Sort items if needed
    [self computeSortedItems:childItemsCollection
                      byMode:self.currentSortingMode];
  }

  self.isFirstLoad = NO;
}

- (void)computeSortedItems:(NSMutableArray*)items
                    byMode:(SpeedDialSortingMode)mode {

  NSMutableArray* sortedItems = [[NSMutableArray alloc] initWithArray:@[]];

  if (IsTopSitesEnabled() && [self showFrequentlyVisited]) {
    NSMutableArray* frequentlyVisitedPages = [[NSMutableArray alloc] init];
    for (ContentSuggestionsMostVisitedItem* tile in
              _mostVisitedConfig.mostVisitedItems) {
      VivaldiSpeedDialItem* item =
          [[VivaldiSpeedDialItem alloc] initWithTitle:tile.title url:tile.URL];
      item.imageDataSource = _mostVisitedConfig.imageDataSource;
      [frequentlyVisitedPages addObject:item];
    }
    // Add frequently visited pages at the front.
    [sortedItems addObject:frequentlyVisitedPages];
  }

  for (id childItems in items) {
    NSArray* sortedArray = [self sortSpeedDials:childItems byMode:mode];
    [sortedItems addObject:sortedArray];
  }

  [self.consumer refreshChildItems:sortedItems
                 topSitesAvailable:self.isTopSitesResultsAvailable];
}

#pragma mark - PRIVATE METHODS

/// Layout style change handler
- (void)handleLayoutChangeNotification {
  [self.consumer reloadLayout];
}

/// Returns current sorting mode
- (SpeedDialSortingMode)currentSortingMode {
  return [VivaldiStartPagePrefsHelper getSDSortingMode];
}

/// Returns current sorting order
- (SpeedDialSortingOrder)currentSortingOrder {
  return [VivaldiStartPagePrefsHelper getSDSortingOrder];
}

/// Fetches speed dial folders and their children and notifies the consumers.
- (void)fetchSpeedDialFolders {

  bookmarks::ManagedBookmarkService* managedBookmarkService =
      ManagedBookmarkServiceFactory::GetForProfile(_profile.get());

  _speedDialFolders = [[NSMutableArray alloc] init];
  NSMutableArray* speedDialFolderChildren = [[NSMutableArray alloc] init];

  std::vector<const BookmarkNode*> bookmarkList;

  // Stack for Depth-First Search of bookmark model.
  base::stack<const BookmarkNode*> stk;

  if (!_bookmarkModel.get()->bookmark_bar_node()->children().empty()) {
    bookmarkList.push_back(_bookmarkModel.get()->bookmark_bar_node());
  }

  if (!_bookmarkModel.get()->mobile_node()->children().empty()) {
    bookmarkList.push_back(_bookmarkModel.get()->mobile_node());
  }

  if (!_bookmarkModel.get()->other_node()->children().empty()) {
    bookmarkList.push_back(_bookmarkModel.get()->other_node());
  }

  // Push all top folders in stack and give them depth of 0.
  for (std::vector<const BookmarkNode*>::reverse_iterator it =
           bookmarkList.rbegin();
       it != bookmarkList.rend();
       ++it) {
    stk.push(*it);
  }

  while (!stk.empty()) {
    const BookmarkNode* node = stk.top();
    stk.pop();

    if (GetSpeeddial(node) && !IsSeparator(node)) {
      VivaldiSpeedDialItem* item =
          [[VivaldiSpeedDialItem alloc] initWithBookmark:node];
      [self.speedDialFolders addObject:item];

      NSMutableArray *items = [[NSMutableArray alloc] init];

      if (node->children().size() > 0)
        for (const auto& child : node->children()) {
          const BookmarkNode* childNode = child.get();
          [items addObject:[[VivaldiSpeedDialItem alloc]
                            initWithBookmark:childNode]];
        }
      [speedDialFolderChildren addObject:items];
    }

    bookmarkList.clear();
    for (const auto& child : node->children()) {
      if (child->is_folder() &&
          !managedBookmarkService->IsNodeManaged(child.get())) {
        bookmarkList.push_back(child.get());
      }
    }

    for (auto it = bookmarkList.rbegin();
         it != bookmarkList.rend();
         ++it) {
      stk.push(*it);
    }
  }

  // Create a collection of array with speed dial folder item title.
  NSMutableArray *menuItems = [[NSMutableArray alloc] init];

  if (IsTopSitesEnabled() && [self showFrequentlyVisited]) {
    [menuItems addObject:
        [self toolbarItemWithId:nil
            title:GetNSString(IDS_IOS_START_PAGE_FREQUENTLY_VISITED_TITLE)]];
  }

  // Only proceed to add dynamic items if they are visible
  if ([self showSpeedDials]) {
    for (VivaldiSpeedDialItem *item in self.speedDialFolders) {
      [menuItems addObject:[self toolbarItemWithId:item.idValue
                                             title:item.title]];
    }
  }
  // Add an extra menu item with empty string at the end for 'Add Group' page.
  // We do not need the title since the item will not have any title.
  [menuItems addObject:[self toolbarItemWithId:nil
                                         title:@""]];

  // Refresh menu items
  [self.consumer refreshMenuItems:menuItems
                        SDFolders:self.speedDialFolders];
  // Refresh child items
  [self computeSortedItems:speedDialFolderChildren
                    byMode:self.currentSortingMode];

  self.isFirstLoad = NO;
}

/// Returns the children of a node.
- (NSArray*)fetchSpeedDialChildrenOf:(VivaldiSpeedDialItem*)item {

  NSMutableArray *items = [[NSMutableArray alloc] init];

  std::vector<const BookmarkNode*> bookmarkList;

  // Stack for Depth-First Search of bookmark model.
  base::stack<const BookmarkNode*> stk;

  if (!_bookmarkModel.get()->bookmark_bar_node()->children().empty()) {
    bookmarkList.push_back(_bookmarkModel.get()->bookmark_bar_node());
  }

  if (!_bookmarkModel.get()->mobile_node()->children().empty()) {
    bookmarkList.push_back(_bookmarkModel.get()->mobile_node());
  }

  if (!_bookmarkModel.get()->other_node()->children().empty()) {
    bookmarkList.push_back(_bookmarkModel.get()->other_node());
  }

  // Push all top folders in stack and give them depth of 0.
  for (std::vector<const BookmarkNode*>::reverse_iterator it =
           bookmarkList.rbegin();
       it != bookmarkList.rend();
       ++it) {
    stk.push(*it);
  }

  while (!stk.empty()) {
    const BookmarkNode* node = stk.top();
    stk.pop();
    // Match
    if (node->id() == item.id) {
      if (node->children().size() > 0)
        for (const auto& child : node->children()) {
          const BookmarkNode* childNode = child.get();
          if (IsSeparator(childNode))
            continue;
          [items addObject:[[VivaldiSpeedDialItem alloc]
                              initWithBookmark:childNode]];
        }
    } else {
      if (node->children().size() > 0)
        for (const auto& child : node->children()) {
          if (child->is_folder()) {
            stk.push(child.get());
          }
        }
    }
  }

  return items;
}

- (VivaldiNTPTopToolbarItem*)toolbarItemWithId:(NSNumber*)idValue
                                         title:(NSString*)title {
  return [[VivaldiNTPTopToolbarItem alloc] initWithId:idValue title:title];
}

/// Sort and return children of a speed dial folder
- (NSArray*)sortSpeedDials:(NSArray*)items
                    byMode:(SpeedDialSortingMode)mode  {

  NSArray* sortedArray = [items sortedArrayUsingComparator:
                          ^NSComparisonResult(VivaldiSpeedDialItem *first,
                                              VivaldiSpeedDialItem *second) {
    switch (mode) {
      case SpeedDialSortingManual:
        // Return as it is coming from bookmark model by default
        return NSOrderedAscending;
      case SpeedDialSortingByTitle:
        // Sort by title
        return [first.title compare:second.title
                            options:NSCaseInsensitiveSearch];
      case SpeedDialSortingByAddress:
        // Sort by address
        return [self compare:first.urlString
                     second:second.urlString];
      case SpeedDialSortingByNickname:
        // Sort by nickname
        return [self compare:first.nickname
                     second:second.nickname];
      case SpeedDialSortingByDescription:
        // Sort by description
        return [self compare:first.description
                     second:second.description];
      case SpeedDialSortingByDate:
        // Sort by date
        return [first.createdAt compare:second.createdAt];
      case SpeedDialSortingByKind:
        // Sort by kind
        return [self compare:first.isFolder
                      second:second.isFolder
                foldersFirst:YES];
      default:
        // Return as it is coming from bookmark model by default
        return NSOrderedAscending;
    }
  }];

  // If the current sorting order is descending
  // Reverse the array & check it is not sort by SpeedDialSortingManual
  if (self.currentSortingOrder == SpeedDialSortingOrderDescending &&
      self.currentSortingMode != SpeedDialSortingManual) {
    sortedArray =
        [[[sortedArray reverseObjectEnumerator] allObjects] mutableCopy];
  }

  return sortedArray;
}

/// Returns sorted result from two provided NSString keys.
- (NSComparisonResult)compare:(NSString*)first
                       second:(NSString*)second {
  return [VivaldiGlobalHelpers compare:first
                                second:second];
}

/// Returns sorted result from two provided BOOL keys, and sorting order.
- (NSComparisonResult)compare:(BOOL)first
                       second:(BOOL)second
                 foldersFirst:(BOOL)foldersFirst {
  return [VivaldiGlobalHelpers compare:first
                                second:second
                          foldersFirst:foldersFirst];
}

- (void)refreshContents {
  if (self.isFirstLoad || self.runningExtensiveChanges)
    return;
  [self.consumer refreshContents];
}

- (BOOL)showFrequentlyVisited {
  if (!_showFrequentlyVisited)
    return NO;
  return [_showFrequentlyVisited value];
}

- (BOOL)showSpeedDials {
  if (!_showSpeedDials)
    return YES;
  return [_showSpeedDials value];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _showSpeedDials) {
    [self.consumer setSpeedDialsEnabled:[observableBoolean value]];
    [self refreshContents];
  } else if (observableBoolean == _showFrequentlyVisited) {
    [self.consumer setFrequentlyVisitedPagesEnabled:[observableBoolean value]];
    [self refreshContents];
  } else if (observableBoolean == _showCustomizeStartPageButton) {
    [self.consumer
        setShowCustomizeStartPageButtonEnabled:[observableBoolean value]];
  } else if (observableBoolean == _tabBarEnabled ||
             observableBoolean == _bottomOmniboxEnabled) {
    [self handleLayoutChangeNotification];
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == vivaldiprefs::kVivaldiStartPageLayoutStyle ||
      preferenceName == vivaldiprefs::kVivaldiStartPageSDMaximumColumns) {
    [self handleLayoutChangeNotification];
  }
}

#pragma mark - BOOKMARK MODEL OBSERVER

- (void)bookmarkModelLoaded {
  [self.consumer bookmarkModelLoaded];
  [self startMediating];
}

- (void)didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self.consumer refreshNode:bookmarkNode];
}

- (void)didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self refreshContents];
}

- (void)didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
         fromParent:(const bookmarks::BookmarkNode*)oldParent
           toParent:(const bookmarks::BookmarkNode*)newParent {
  [self refreshContents];
}

- (void)didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  [self refreshContents];
}

- (void)didChangeFaviconForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  // Only urls have favicons.
  if (!bookmarkNode->is_url())
    return;

  if (ShouldPresentFirstRunExperience()) {
    [self refreshContents];
  } else {
    [self.consumer refreshNode:bookmarkNode];
  }
}

- (void)bookmarkModelRemovedAllNodes {
  // No-op.
}

- (void)bookmarkMetaInfoChanged:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (bookmarkNode->is_folder()) {
    [self refreshContents];
  }
}

- (void)extensiveBookmarkChangesBeginning {
  _runningExtensiveChanges = YES;
}

- (void)extensiveBookmarkChangesEnded {
  _runningExtensiveChanges = NO;
  [self refreshContents];
}

#pragma mark - VivaldiMostVisitedSitesConsumer
- (void)setMostVisitedTilesConfig:(MostVisitedTilesConfig*)config {
  _mostVisitedConfig = config;
  self.isTopSitesResultsAvailable = YES;
  [self refreshContents];
}

@end
