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
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
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
#import "ios/ui/ntp/vivaldi_speed_dial_page_type.h"
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
// Collection of toolbar items
@property(nonatomic, strong)
    NSMutableArray<VivaldiNTPTopToolbarItem*>* toolbarItems;
// Collection of cached toolbar items. This is used to compare whether toolbar
// items count is changed due to CRUD operation either initiated by user or sync.
@property(nonatomic, strong)
    NSMutableArray<VivaldiNTPTopToolbarItem*>* cachedToolbarItems;
// Bool to keep track of extensive changes.
@property(nonatomic,assign) BOOL runningExtensiveChanges;
@end

@implementation VivaldiSpeedDialHomeMediator {
  // The model holding bookmark data.
  base::WeakPtr<BookmarkModel> _bookmarkModel;
  // Bridge to register for bookmark changes in the bookmarkModel.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridge;
}

@synthesize consumer = _consumer;
@synthesize toolbarItems = _toolbarItems;
@synthesize cachedToolbarItems = _cachedToolbarItems;
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

    self.toolbarItems = [[NSMutableArray alloc] init];
    self.cachedToolbarItems = [[NSMutableArray alloc] init];
  }
  return self;
}

#pragma mark - PUBLIC METHODS

- (void)startMediating {
  DCHECK(self.consumer);
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

- (void)setConsumer:(id<SpeedDialHomeConsumer>)consumer {
  _consumer = consumer;
  if (!self.consumer) {
    return;
  }

  [self.consumer refreshMenuItems:self.toolbarItems];
  if (self.toolbarItems.count > 0) {
    [self.consumer selectToolbarItemWithIndex:self.selectedMenuItemIndex];
  }
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
    [self computeTopToolbarItems];
}

- (void)computeSpeedDialChildItems:(VivaldiSpeedDialItem*)item {
  // If an item is provided fetch the children of that item.
  // Otherwise fetch all children of all speed dial folder items and notify
  // consumers to update them.
  if (item && item.bookmarkNode) {
    [self reloadChildrenForBookmarkNode:item.bookmarkNode];
  } else {

    for (VivaldiNTPTopToolbarItem* group in self.toolbarItems) {

      if ([group.uuid length] <= 0)
        continue;

      std::string uuidString =
          base::SysNSStringToUTF8([group.uuid lowercaseString]);
      base::Uuid uuid = base::Uuid::ParseLowercase(uuidString);

      const bookmarks::BookmarkNode* node =
          _bookmarkModel.get()->GetNodeByUuid(uuid,
              bookmarks::BookmarkModel::
                  NodeTypeForUuidLookup::kLocalOrSyncableNodes);

      [self reloadChildrenForBookmarkNode:node];
    }
  }
}

- (void)moveSpeedDialItem:(VivaldiSpeedDialItem*)item
                 position:(NSInteger)position {
  _bookmarkModel.get()->Move(item.bookmarkNode,
                             item.parent,
                             position);
}

- (void)deleteSpeedDialItem:(VivaldiSpeedDialItem*)item {
  if (_bookmarkModel.get()->loaded() && item.bookmarkNode) {
    std::vector<const bookmarks::BookmarkNode*> nodes;
    nodes.push_back(item.bookmarkNode);
    const BookmarkNode* trashFolder = _bookmarkModel.get()->trash_node();
    bookmark_utils_ios::MoveBookmarks(nodes,
                                      _bookmarkModel.get(),
                                      trashFolder);
  }
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

/// Fetches speed dial folders and their children, notifies consumers.
- (void)computeTopToolbarItems {

  // Clear old data so we don’t retain stale groups.
  [self.toolbarItems removeAllObjects];

  if ([self showSpeedDials]) {

    std::vector<const BookmarkNode*> root_nodes;

    // Push top-level nodes if they have children
    if (!_bookmarkModel->bookmark_bar_node()->children().empty()) {
      root_nodes.push_back(_bookmarkModel.get()->bookmark_bar_node());
    }
    if (!_bookmarkModel->mobile_node()->children().empty()) {
      root_nodes.push_back(_bookmarkModel.get()->mobile_node());
    }
    if (!_bookmarkModel->other_node()->children().empty()) {
      root_nodes.push_back(_bookmarkModel.get()->other_node());
    }

    bookmarks::ManagedBookmarkService* managedBookmarkService =
        ManagedBookmarkServiceFactory::GetForProfile(_profile.get());

    // Stack for Depth-First Search of bookmark model.
    base::stack<const BookmarkNode*> stack;

    for (std::vector<const BookmarkNode*>::reverse_iterator it =
        root_nodes.rbegin();
         it != root_nodes.rend();
         ++it) {
      stack.push(*it);
    }

    while (!stack.empty()) {
      const BookmarkNode* node = stack.top();
      stack.pop();

      if (GetSpeeddial(node) && !IsSeparator(node)) {
        VivaldiNTPTopToolbarItem* toolbarItem = [self buildGroupForNode:node];
        [self.toolbarItems addObject:toolbarItem];
      }

      root_nodes.clear();

      for (const auto& child : node->children()) {
        if (child->is_folder() &&
            !managedBookmarkService->IsNodeManaged(child.get())) {
          root_nodes.push_back(child.get());
        }
      }

      for (auto it = root_nodes.rbegin();
           it != root_nodes.rend();
           ++it) {
        stack.push(*it);
      }
    }
  }

  // If top sites is enabled
  if (IsTopSitesEnabled() && [self showFrequentlyVisited]) {
    VivaldiNTPTopToolbarItem* toolbarItem =
        [[VivaldiNTPTopToolbarItem alloc]
             initWithPrimaryId:nil
                          uuid:nil
                title:GetNSString(IDS_IOS_START_PAGE_FREQUENTLY_VISITED_TITLE)
                      pageType:VivaldiSpeedDialPageTypeTopSites];
    toolbarItem.children = [self listTopSiteItems];;
    [self.toolbarItems insertObject:toolbarItem
                            atIndex:0];
  }

  // Add extra "Add Group" item
  [self.toolbarItems addObject:
     [[VivaldiNTPTopToolbarItem alloc]
          initWithPrimaryId:nil
                       uuid:nil
                      title:@""
                   pageType:VivaldiSpeedDialPageTypeAddGroup]];

  // Refresh the UI
  [self.consumer refreshMenuItems:self.toolbarItems];

  // Update the toolbar index only when toolbar items are changed which may
  // happen after sync or if user adds/removes any group.
  // This prevents reloading the toolbar and index for every sync cycle when
  // this function is called but nothing is changed.
  BOOL areEqual =
      [VivaldiNTPTopToolbarItemHelper
          compareEqualityForFirst:self.toolbarItems
                           second:self.cachedToolbarItems];
  if (self.toolbarItems.count > 0 && !areEqual) {
    self.cachedToolbarItems = [self.toolbarItems copy];
    [self.consumer selectToolbarItemWithIndex:self.selectedMenuItemIndex];
  }
}

// Create and return a ToolbarItem from provided BookmarkNode computing the
// children of that node.
- (VivaldiNTPTopToolbarItem*)buildGroupForNode:(const BookmarkNode*)node {

  NSString* uuidString;
  if (node->uuid().is_valid()) {
    uuidString = base::SysUTF8ToNSString(node->uuid().AsLowercaseString());
  }

  VivaldiNTPTopToolbarItem* groupItem =
      [[VivaldiNTPTopToolbarItem alloc]
           initWithPrimaryId:@(node->id())
                        uuid:uuidString
                       title:bookmark_utils_ios::TitleForBookmarkNode(node)
                    pageType:VivaldiSpeedDialPageTypeSpeedDial];

  NSMutableArray* childrens = [[NSMutableArray alloc] init];
  if (node->is_folder()) {
    for (const auto& child : node->children()) {
      const BookmarkNode* childNode = child.get();
      if (IsSeparator(childNode))
        continue;
      [childrens addObject:
          [[VivaldiSpeedDialItem alloc] initWithBookmark:childNode]];
    }

    groupItem.children = [self sortSpeedDials:childrens
                                       byMode:self.currentSortingMode];
  }
  return groupItem;
}

// Reload only the children of the provided BookmarkNode and notify the
// consumers.
- (void)reloadChildrenForBookmarkNode:(const BookmarkNode*)bookmarkNode {
  if (!bookmarkNode)
    return;
  VivaldiNTPTopToolbarItem* groupItem = [self buildGroupForNode:bookmarkNode];
  [self.consumer refreshChildItems:groupItem.children parent:groupItem];
}

// Reloads only the top site items and notify the consumers.
- (void)reloadChildrenForTopSite {

  if (!IsTopSitesEnabled() || ![self showFrequentlyVisited]) {
    return;
  }

  VivaldiNTPTopToolbarItem* toolbarItem;
  NSMutableArray<VivaldiSpeedDialItem*>* topSites = [self listTopSiteItems];

  // Find the toolbar item with matching page type
  for (NSUInteger i = 0; i < self.toolbarItems.count; i++) {
    VivaldiNTPTopToolbarItem *item = self.toolbarItems[i];
    if (item.pageType == VivaldiSpeedDialPageTypeTopSites) {
      toolbarItem = item;
      toolbarItem.children = topSites;
      break;
    }
  }

  [self.consumer refreshChildItems:topSites parent:toolbarItem];
}

// Returns the mapped item for top site items loaded into the model.
// If the model is not loaded, it returns an empty array.
- (NSMutableArray<VivaldiSpeedDialItem*>*)listTopSiteItems {
  NSMutableArray<VivaldiSpeedDialItem*>* topSites =
      [[NSMutableArray alloc] init];
  for (ContentSuggestionsMostVisitedItem* tile in
            _mostVisitedConfig.mostVisitedItems) {
    VivaldiSpeedDialItem* item =
        [[VivaldiSpeedDialItem alloc] initWithTitle:tile.title url:tile.URL];
    item.imageDataSource = _mostVisitedConfig.imageDataSource;
    [topSites addObject:item];
  }
  return topSites;
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
  if (self.runningExtensiveChanges)
    return;
  [self computeSpeedDialFolders];
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

/// Returns the intended selected index for toolbar items
/// depending on user pref and current toolbar items count.
- (NSInteger)selectedMenuItemIndex {

  if (_runningExtensiveChanges) {
    return [self indexForLastVisitedGroup];
  }

  // Retrieve the user's preference for the start page opening item.
  VivaldiStartPageStartItemType openWith =
      [VivaldiStartPagePrefs getReopenStartPageWithItem];

  NSInteger index = 0; // Default index

  switch (openWith) {
    // Case when the user prefers to open with the first group or
    // default preference.
    case VivaldiStartPageStartItemTypeFirstGroup:
    default: {
      // If "Frequently Visited" is not shown, select the first
      // menu item (index 0).
      if (!self.showFrequentlyVisited) {
        index = 0;
      } else {
        // If there are items in toolbar,
        // select the second menu item (index 1); otherwise, select the first.
        index = self.toolbarItems.count > 0 ? 1 : 0;
      }
      break;
    }

    // Case when the user prefers to open with "Top Sites".
    case VivaldiStartPageStartItemTypeTopSites:
      // Always select the first menu item (index 0).
      index = 0;
      break;

    // Case when the user prefers to open with the last visited group.
    case VivaldiStartPageStartItemTypeLastVisited: {
      index = [self indexForLastVisitedGroup];
      break;
    }
  }

  // Safety Check: Ensure that the index is within
  // the bounds of speedDialMenuItems.
  if (self.toolbarItems.count == 0) {
    // No menu items are available;
    // return 0 to indicate no valid selection.
    return 0;
  } else if (index >= (NSInteger)self.toolbarItems.count) {
    // Adjust the index to the last valid index if it's out of bounds.
    index = self.toolbarItems.count - 1;
  }
  return index;
}

/// Returns the selected item index for last visiter group
- (NSUInteger)indexForLastVisitedGroup {
  return [VivaldiStartPagePrefs getStartPageLastVisitedGroupIndex];
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
  // If the node is a group reload the toolbar beceause it can be
  // that a Group is renamed. Otherwise, refresh the node.
  if (bookmarkNode->is_folder() && GetSpeeddial(bookmarkNode)) {
    [self computeTopToolbarItems];
  } else {
    [self.consumer refreshNode:bookmarkNode];
  }
}

- (void)didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (_runningExtensiveChanges)
    return;
  // This method gets called when any item added/removed/or reordered.
  // TODO: @prio: When reordered by user we should skip observing this method.
  if (bookmarkNode->is_folder()) {
    [self reloadChildrenForBookmarkNode:bookmarkNode];
  }
}

- (void)didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
         fromParent:(const bookmarks::BookmarkNode*)oldParent
           toParent:(const bookmarks::BookmarkNode*)newParent {
  // No need to do a full refresh when movement happened within same folder.
  if (oldParent == newParent) {
    return;
  }

  // If the node that is moved is a group reload the toolbar beceause it can be
  // that a Group is removed. Otherwise, refresh the old and new parent.
  if (bookmarkNode->is_folder() && GetSpeeddial(bookmarkNode)) {
    [self computeTopToolbarItems];
  } else {
    [self reloadChildrenForBookmarkNode:oldParent];
    [self reloadChildrenForBookmarkNode:newParent];
  }
}

- (void)didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  // No op since this is only called for us when items removed from trash
  // which has no UX with StartPage.
}

- (void)didChangeFaviconForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (_runningExtensiveChanges)
    return;

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
  if (_runningExtensiveChanges)
    return;

  if (bookmarkNode->is_folder()) {
    [self refreshContents];
  }
}

- (void)extensiveBookmarkChangesBeginning {
  _runningExtensiveChanges = YES;
}

- (void)extensiveBookmarkChangesEnded {
  _runningExtensiveChanges = NO;
  [self computeTopToolbarItems];
}

#pragma mark - VivaldiMostVisitedSitesConsumer
- (void)setMostVisitedTilesConfig:(MostVisitedTilesConfig*)config {
  _mostVisitedConfig = config;
  [self.consumer topSitesModelDidLoad];
  [self reloadChildrenForTopSite];
}

@end
