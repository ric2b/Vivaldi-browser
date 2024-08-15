// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_home_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "chromium/base/containers/stack.h"
#import "components/bookmarks/browser/bookmark_model_observer.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs_helper.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"
#import "prefs/vivaldi_pref_names.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi_bookmark_kit::SetNodeSpeeddial;
using vivaldi_bookmark_kit::IsSeparator;
using bookmarks::ManagedBookmarkService;


@interface VivaldiSpeedDialHomeMediator ()<BookmarkModelBridgeObserver,
                                           PrefObserverDelegate,
                                           BooleanObserver> {
  // Bridge to register for bookmark changes.
  std::unique_ptr<BookmarkModelBridge> _model_bridge;

  // Preference service from the application context.
  PrefService* _prefs;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Observer for speed dials visibility state
  PrefBackedBoolean* _showSpeedDials;
  // Observer for start page customize button visibility state
  PrefBackedBoolean* _showCustomizeStartPageButton;
}

// The browser state for this mediator.
@property(nonatomic,assign) ChromeBrowserState* browserState;
// The model holding bookmark data.
@property(nonatomic,assign) LegacyBookmarkModel* bookmarkModel;
// Speed dial folders collection
@property(nonatomic,strong) NSMutableArray* speedDialFolders;
// Bool to keep track the initial loading of the speed dial folders.
@property(nonatomic,assign) BOOL isFirstLoad;
// Bool to keep track of extensive changes.
@property(nonatomic,assign) BOOL runningExtensiveChanges;
@end

@implementation VivaldiSpeedDialHomeMediator
@synthesize browserState = _browserState;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize consumer = _consumer;
@synthesize speedDialFolders = _speedDialFolders;
@synthesize isFirstLoad = _isFirstLoad;
@synthesize runningExtensiveChanges = _runningExtensiveChanges;

#pragma mark - INITIALIZERS
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                       bookmarkModel:(LegacyBookmarkModel*)bookmarkModel {
  if ((self = [super init])) {
    _browserState = browserState;
    _bookmarkModel = bookmarkModel;
    _model_bridge.reset(new BookmarkModelBridge(self,_bookmarkModel));

    _prefs = browserState->GetPrefs();
    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));

    _prefObserverBridge->ObserveChangesForPreference(
        vivaldiprefs::kVivaldiStartPageLayoutStyle, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        vivaldiprefs::kVivaldiStartPageSDMaximumColumns, &_prefChangeRegistrar);

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

    [VivaldiStartPagePrefs setPrefService:browserState->GetPrefs()];
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
  self.browserState = nullptr;
  self.bookmarkModel = nullptr;
  self.consumer = nil;

  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefs = nil;

  [_showSpeedDials stop];
  [_showSpeedDials setObserver:nil];
  _showSpeedDials = nil;

  [_showCustomizeStartPageButton stop];
  [_showCustomizeStartPageButton setObserver:nil];
  _showCustomizeStartPageButton = nil;

  _model_bridge = nullptr;
}

- (void)computeSpeedDialFolders {
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
    [self.consumer refreshChildItems:sortedArray];

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

  for (id childItems in items) {
    NSArray* sortedArray = [self sortSpeedDials:childItems byMode:mode];
    [sortedItems addObject:sortedArray];
  }

  [self.consumer refreshChildItems:sortedItems];
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
      ManagedBookmarkServiceFactory::GetForBrowserState(self.browserState);

  _speedDialFolders = [[NSMutableArray alloc] init];
  NSMutableArray* speedDialFolderChildren = [[NSMutableArray alloc] init];

  std::vector<const BookmarkNode*> bookmarkList;

  // Stack for Depth-First Search of bookmark model.
  base::stack<const BookmarkNode*> stk;

  bookmarkList.push_back(self.bookmarkModel->mobile_node());
  bookmarkList.push_back(self.bookmarkModel->bookmark_bar_node());
  bookmarkList.push_back(self.bookmarkModel->other_node());

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

  // Only proceed to add dynamic items if they are visible
  if ([self showSpeedDials]) {
    for (VivaldiSpeedDialItem *item in self.speedDialFolders) {
      [menuItems addObject:item.title];
    }
  }
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

  bookmarkList.push_back(self.bookmarkModel->mobile_node());
  bookmarkList.push_back(self.bookmarkModel->bookmark_bar_node());
  bookmarkList.push_back(self.bookmarkModel->other_node());

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

- (BOOL)showSpeedDials {
  if (!_showSpeedDials)
    return YES;
  return [_showSpeedDials value];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _showSpeedDials) {
    [self.consumer setSpeedDialsEnabled:[observableBoolean value]];
  } else if (observableBoolean == _showCustomizeStartPageButton) {
    [self.consumer
        setShowCustomizeStartPageButtonEnabled:[observableBoolean value]];
    return;
  }
  [self refreshContents];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == vivaldiprefs::kVivaldiStartPageLayoutStyle ||
      preferenceName == vivaldiprefs::kVivaldiStartPageSDMaximumColumns) {
    [self handleLayoutChangeNotification];
  }
}

#pragma mark - BOOKMARK MODEL OBSERVER

- (void)bookmarkModelLoaded:(LegacyBookmarkModel*)model {
  [self.consumer bookmarkModelLoaded];
  [self startMediating];
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
        didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self.consumer refreshNode:bookmarkNode];
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
    didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self refreshContents];
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
          didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
           fromParent:(const bookmarks::BookmarkNode*)oldParent
             toParent:(const bookmarks::BookmarkNode*)newParent {
  [self refreshContents];
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
        didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  [self refreshContents];
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
    didChangeFaviconForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  // Only urls have favicons.
  if (!bookmarkNode->is_url())
    return;

  if (ShouldPresentFirstRunExperience()) {
    [self refreshContents];
  } else {
    [self.consumer refreshNode:bookmarkNode];
  }
}

- (void)bookmarkModelRemovedAllNodes:(LegacyBookmarkModel*)model {
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

@end
