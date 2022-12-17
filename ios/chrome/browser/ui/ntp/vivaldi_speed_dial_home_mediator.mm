// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_home_mediator.h"

#import "ios/chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_prefs.h"
#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chromium/base/containers/stack.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "components/url_formatter/url_fixer.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi_bookmark_kit::SetNodeSpeeddial;
using bookmarks::ManagedBookmarkService;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

}  // namespace

@interface VivaldiSpeedDialHomeMediator () {}

// The browser state for this mediator.
@property(nonatomic,assign) ChromeBrowserState* browserState;
// The model holding bookmark data.
@property(nonatomic,assign) BookmarkModel* bookmarkModel;
// Speed dial folders collection
@property(nonatomic,strong) NSMutableArray* speedDialFolders;
@end

@implementation VivaldiSpeedDialHomeMediator
@synthesize browserState = _browserState;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize consumer = _consumer;
@synthesize speedDialFolders = _speedDialFolders;

#pragma mark - INITIALIZERS
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                       bookmarkModel:(BookmarkModel*)bookmarkModel {
  if ((self = [super init])) {
    _browserState = browserState;
    _bookmarkModel = bookmarkModel;
  }
  return self;
}

#pragma mark - PUBLIC METHODS

- (void)startMediating {
  DCHECK(self.consumer);
  [self computeSpeedDialFolders];
}

- (void)disconnect {
  self.browserState = nullptr;
  self.bookmarkModel = nullptr;
  self.consumer = nil;
}

- (void)computeSpeedDialFolders {
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

/// Returns current sorting mode
- (SpeedDialSortingMode)currentSortingMode {
  return [VivaldiSpeedDialPrefs
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

  // MARK: - TEST DATA INJECTION STARTS
  // TODO: - @prio@vivaldi.com - REMOVE THIS BLOCK WHEN DEFAULT ITEMS ARE SET
  // IMPORTANT!!!: THIS IS TEMPORARY UNTIL THE DEFAULT ITEMS ARE SET
  // If there's no item then create two folders under bookmarks bar node
  const BookmarkNode* bbNode = self.bookmarkModel->bookmark_bar_node();
  if (bbNode->children().size() == 0) {

    // Speed dial
    NSString *speedDialfolderString = @"Speed Dial";
    std::u16string speedDialFolderTitle =
      base::SysNSStringToUTF16(speedDialfolderString);

    const BookmarkNode* speedDial =
      self.bookmarkModel->AddFolder(bbNode,
                                bbNode->children().size(),
                                speedDialFolderTitle);
    SetNodeSpeeddial(self.bookmarkModel, speedDial, YES);

    [self addAmazon:speedDial];
    [self addBooking:speedDial];
    [self addYelp:speedDial];
    [self addYoutube:speedDial];
    [self addeBay:speedDial];
    [self addWalmart:speedDial];
    [self addHotels:speedDial];
    [self addAliex:speedDial];
    [self addDisney:speedDial];
    [self addExpedia:speedDial];
    [self addYahoo:speedDial];
    [self addEneba:speedDial];

    // Vivaldi folder
    NSString *vivaldiFolderString = @"Vivaldi";
    std::u16string vivaldiFolderTitle =
      base::SysNSStringToUTF16(vivaldiFolderString);

    // Store the folder
    const BookmarkNode* vivaldi =
      self.bookmarkModel->AddFolder(speedDial,
                                speedDial->children().size(),
                                vivaldiFolderTitle);
    SetNodeSpeeddial(self.bookmarkModel, vivaldi, NO);

    [self addVivaldiCom:vivaldi];

    // Bookmarks
    NSString *bookmarkfolderString = @"Bookmarks";
    std::u16string bookmarkFolderTitle =
      base::SysNSStringToUTF16(bookmarkfolderString);

    // Store the folder
    const BookmarkNode* bookmark =
      self.bookmarkModel->AddFolder(bbNode,
                                bbNode->children().size(),
                                bookmarkFolderTitle);
    SetNodeSpeeddial(self.bookmarkModel, bookmark, NO);
  }
  // MARK: - TEST DATA INJECTION ENDS

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

#pragma mark - TEST DATA STARTS

// TODO: - @prio@vivaldi.com - REMOVE THESE STATIC BOOOKMARKS
// IMPORTANT!!!
// THESE SHOULD BE DELETED AS SOON AS DEFAULT BOOKMARK SETTING IS IMPLEMENTED

- (void) addAmazon:(const BookmarkNode*)parent {

  NSString* urlString = @"https://amazon.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"Amazon";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addBooking:(const BookmarkNode*)parent {

  NSString* urlString = @"https://booking.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"Booking.com";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addYelp:(const BookmarkNode*)parent {

  NSString* urlString = @"https://yelp.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"Yelp";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addYoutube:(const BookmarkNode*)parent {

  NSString* urlString = @"https://youtube.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"YouTube";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addeBay:(const BookmarkNode*)parent {

  NSString* urlString = @"https://ebay.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"eBay";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addWalmart:(const BookmarkNode*)parent {

  NSString* urlString = @"https://walmart.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"Walmart";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addHotels:(const BookmarkNode*)parent {

  NSString* urlString = @"https://hotels.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"Hotels";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addAliex:(const BookmarkNode*)parent {

  NSString* urlString = @"https://aliexpress.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"AliExpress";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addDisney:(const BookmarkNode*)parent {

  NSString* urlString = @"https://disneyplus.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"Disney+";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addExpedia:(const BookmarkNode*)parent {

  NSString* urlString = @"https://expedia.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"Expedia";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addYahoo:(const BookmarkNode*)parent {

  NSString* urlString = @"https://yahoo.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"Yahoo!";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addEneba:(const BookmarkNode*)parent {

  NSString* urlString = @"https://eneba.com";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"Eneba";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

- (void) addVivaldiCom:(const BookmarkNode*)parent {

  NSString* urlString = @"https://vivaldi.net";
  GURL url = ConvertUserDataToGURL(urlString);

  NSString* bookmarkName = @"Vivaldi Community";
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Save the bookmark information.
  self.bookmarkModel->AddURL(parent,
                         parent->children().size(),
                         titleString,
                         url);
}

#pragma mark - TEST DATA ENDS

@end
