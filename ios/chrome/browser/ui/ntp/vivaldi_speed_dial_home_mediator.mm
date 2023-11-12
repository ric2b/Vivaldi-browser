// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_home_mediator.h"

#import "base/check.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "chromium/base/containers/stack.h"
#import "components/bookmarks/browser/bookmark_model_observer.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_start_page_prefs.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi_bookmark_kit::SetNodeSpeeddial;
using bookmarks::ManagedBookmarkService;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface VivaldiSpeedDialHomeMediator ()<BookmarkModelBridgeObserver> {
  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _model_bridge;
}

// The browser state for this mediator.
@property(nonatomic,assign) ChromeBrowserState* browserState;
// The model holding bookmark data.
@property(nonatomic,assign) BookmarkModel* bookmarkModel;
// Speed dial folders collection
@property(nonatomic,strong) NSMutableArray* speedDialFolders;
// Bool to keep track the initial loading of the speed dial folders.
@property(nonatomic,assign) BOOL isFirstLoad;
@end

@implementation VivaldiSpeedDialHomeMediator
@synthesize browserState = _browserState;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize consumer = _consumer;
@synthesize speedDialFolders = _speedDialFolders;
@synthesize isFirstLoad = _isFirstLoad;

#pragma mark - INITIALIZERS
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                       bookmarkModel:(BookmarkModel*)bookmarkModel {
  if ((self = [super init])) {
    _browserState = browserState;
    _bookmarkModel = bookmarkModel;
    _model_bridge.reset(new bookmarks::BookmarkModelBridge(self,
                                                           _bookmarkModel));

    [self setUpStartPageLayoutChangeListener];
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
  _model_bridge = nullptr;
  [self removeStartPageLayoutListener];
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
    NSArray* sortedArray = [self sortSpeedDialChildren:childItems
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
    NSArray* sortedArray = [self sortSpeedDialChildren:childItems byMode:mode];
    [sortedItems addObject:sortedArray];
  }

  [self.consumer refreshChildItems:sortedItems];
}

#pragma mark - PRIVATE METHODS

/// Set up observer to notify whenever layout style is changed.
- (void)setUpStartPageLayoutChangeListener {
  [self removeStartPageLayoutListener];
  [[NSNotificationCenter defaultCenter]
    addObserver:self
       selector:@selector(handleLayoutChangeNotification)
           name:vStartPageLayoutChangeDidChange
         object:nil];
}

- (void)removeStartPageLayoutListener {
  [[NSNotificationCenter defaultCenter]
     removeObserver:self
               name:vStartPageLayoutChangeDidChange
             object:nil];
}

/// Layout style change handler
- (void)handleLayoutChangeNotification {
  [self.consumer reloadLayout];
}

/// Returns current sorting mode
- (SpeedDialSortingMode)currentSortingMode {
  return [VivaldiStartPagePrefs
            getSDSortingModeWithPrefService:self.browserState->GetPrefs()];
}

/// Fetches speed dial folders and their children and notifies the consumers.
- (void)fetchSpeedDialFolders {

  bookmarks::ManagedBookmarkService* managedBookmarkService =
      ManagedBookmarkServiceFactory::GetForBrowserState(self.browserState);

  _speedDialFolders = [[NSMutableArray alloc] init];
  NSMutableArray* speedDialFolderChildren = [[NSMutableArray alloc] init];

  std::vector<const BookmarkNode*> bookmarkList;
  std::vector<const BookmarkNode*> speedDials;

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

    if (GetSpeeddial(node)) {
      speedDials.push_back(node);
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

    if (node->children().size() > 0)
      for (const auto& child : node->children()) {
        if (child->is_folder() &&
            managedBookmarkService->CanBeEditedByUser(child.get())) {
          stk.push(child.get());
        }
      }
  }

  // Refresh menu items
  [self.consumer refreshMenuItems:self.speedDialFolders];
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
- (NSArray*)sortSpeedDialChildren:(NSArray*)items
                           byMode:(SpeedDialSortingMode)mode  {

  NSArray* sortedArray = [items sortedArrayUsingComparator:
                          ^NSComparisonResult(VivaldiSpeedDialItem *first,
                                              VivaldiSpeedDialItem *second) {
    switch (mode) {
      case SpeedDialSortingManual:
        // Return as it is coming from bookmark model when manual is selected
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
      default:
        // Return as it is coming from bookmark model by default
        return NSOrderedAscending;
    }
  }];
  return sortedArray;
}

/// Returns sorted result from two provided NSString keys.
- (NSComparisonResult)compare:(NSString*)first
                       second:(NSString*)second {
  NSComparisonResult result = NSOrderedSame;
  if ([first length] && [second length]) {
    result = [first caseInsensitiveCompare:second];
  } else if ([first length]) {
    result = NSOrderedAscending;
  } else if ([second length]) {
    result = NSOrderedDescending;
  }

  if (result == NSOrderedSame) {
    if ([first length] && [second length]) {
      result = [first caseInsensitiveCompare:second];
    } else if ([first length]) {
      result = NSOrderedAscending;
    } else if ([second length]) {
      result = NSOrderedDescending;
    }
  }
  return result;
}

- (void)refreshContents {
  if (self.isFirstLoad)
    return;
  [self.consumer refreshContents];
}

#pragma mark - BOOKMARK MODEL OBSERVER

- (void)bookmarkModelLoaded {
  // No-op when loaded.
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)node {
  [self refreshContents];
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  [self refreshContents];
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  [self refreshContents];
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)folder {
  [self refreshContents];
}

- (void)bookmarkModelRemovedAllNodes {
  // No-op.
}

- (void)bookmarkMetaInfoChanged:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self refreshContents];
}

- (void)extensiveBookmarkChangesBeginning {
  // No op when sync started. We would be interested to refresh the UI when
  // bookmark changes ended.
}

- (void)extensiveBookmarkChangesEnded {
  [self refreshContents];
}

@end
