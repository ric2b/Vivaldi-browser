// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_base_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "chromium/base/containers/stack.h"
#import "components/bookmarks/browser/base_bookmark_model_observer.h"
#import "components/bookmarks/browser/bookmark_model_observer.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_storage.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/bookmarks/browser/titled_url_index.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/vivaldi_bookmark_add_edit_folder_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/vivaldi_bookmark_add_edit_url_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/vivaldi_bookmarks_constants.h"
#import "ios/chrome/browser/ui/ntp/cells/vivaldi_speed_dial_container_cell.h"
#import "ios/chrome/browser/ui/ntp/top_menu/vivaldi_ntp_top_menu.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_empty_view.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_home_mediator.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_shared_state.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_sorting_mode.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_view_controller.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_start_page_prefs.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/helpers/vivaldi_snapshot_store_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiviewcontroller_helper.h"
#import "prefs/vivaldi_pref_names.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"


#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Namespace
namespace {
// Cell Identifier for the container cell
NSString* cellId = @"cellId";
// Padding for the top scroll menu
UIEdgeInsets topScrollMenuPadding = UIEdgeInsetsMake(0.f, 16.f, 0.f, 0.f);
// Padding for the speed dial sorting button
UIEdgeInsets sortButtonPadding = UIEdgeInsetsMake(6.f, 0.f, 0.f, 12.f);
// Size for the speed dial sorting button
CGSize sortButtonSize = CGSizeMake(30.f, 30.f);
}

@interface VivaldiSpeedDialBaseController ()<UICollectionViewDataSource,
                                             UICollectionViewDelegate,
                                        UICollectionViewDelegateFlowLayout,
                                             VivaldiNTPTopMenuViewDelegate,
                                        VivaldiSpeedDialContainerDelegate,
                                  VivaldiBookmarkAddEditControllerDelegate,
                                              BookmarkModelBridgeObserver,
                                                    SpeedDialHomeConsumer> {
  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _bridge;
}
// Collection view that holds the children of speed dial folder.
@property (weak, nonatomic) UICollectionView *collectionView;
// Container view for holding the menu items and sort button.
@property(nonatomic, weak) UIView* topScrollMenuContainer;
// Top scroll menu view
@property(nonatomic, weak) VivaldiNTPTopMenuView* topScrollMenu;
// Sort button
@property(nonatomic, weak) UIButton* sortButton;
// The view to show when there's no speed dial folder available
@property(nonatomic, weak) VivaldiSpeedDialEmptyView* emptySpeedDialView;
// The view controller to present when pushing to new view controller
@property(nonatomic, strong)
  VivaldiSpeedDialViewController* speedDialViewController;
// The view controller to present when pushing to add or edit folder
@property(nonatomic, strong)
  VivaldiBookmarkAddEditFolderViewController* bookmarkFolderAddEditorController;
// The view controller to present when pushing to add or edit item
@property(nonatomic, strong)
  VivaldiBookmarkAddEditURLViewController* bookmarkURLAddEditorController;

// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic, assign) FaviconLoader* faviconLoader;
// The Browser in which bookmarks are presented
@property(nonatomic, assign) Browser* browser;
// Bookmarks model that holds all the bookmarks data
@property (assign,nonatomic) BookmarkModel* bookmarks;
// Shared state for the speed dial item
@property(nonatomic, strong)
  VivaldiSpeedDialSharedState* speedDialSharedState;
// The user's browser state model used.
@property(nonatomic, assign) ChromeBrowserState* browserState;
// The mediator that provides data for this view controller.
@property(nonatomic, strong) VivaldiSpeedDialHomeMediator* mediator;

// Array to hold the speed dial menu items
@property (strong, nonatomic) NSMutableArray *speedDialMenuItems;
// Array to hold the speed dial folders children
@property (strong, nonatomic) NSMutableArray *speedDialChildItems;
// A boolean to keep track when the scrolling is taking place due to tap in the
// top menu item. There are two types of scroll even. One happens when user
// swipes the collection view left or right by pan/swipe gesture. The other one
// is when taps on the menu item.
@property (assign, nonatomic) BOOL scrollFromMenuTap;
// Height constraint of the top menu container
@property (assign, nonatomic) NSLayoutConstraint* menuContainerHeight;

// Array to hold the sort button context menu options
@property (strong, nonatomic) NSMutableArray *speedDialSortActions;

// Sort button context menu options
@property(nonatomic, strong) UIAction* manualSortAction;
@property(nonatomic, strong) UIAction* titleSortAction;
@property(nonatomic, strong) UIAction* addressSortAction;
@property(nonatomic, strong) UIAction* nicknameSortAction;
@property(nonatomic, strong) UIAction* descriptionSortAction;
@property(nonatomic, strong) UIAction* dateSortAction;

@end


@implementation VivaldiSpeedDialBaseController

@synthesize browserState = _browserState;
@synthesize speedDialViewController = _speedDialViewController;
@synthesize bookmarkFolderAddEditorController =
    _bookmarkFolderAddEditorController;
@synthesize bookmarkURLAddEditorController = _bookmarkURLAddEditorController;
@synthesize speedDialSharedState = _speedDialSharedState;
@synthesize collectionView = _collectionView;
@synthesize topScrollMenuContainer = _topScrollMenuContainer;
@synthesize topScrollMenu = _topScrollMenu;
@synthesize sortButton = _sortButton;
@synthesize manualSortAction = _manualSortAction;
@synthesize titleSortAction = _titleSortAction;
@synthesize addressSortAction = _addressSortAction;
@synthesize nicknameSortAction = _nicknameSortAction;
@synthesize descriptionSortAction = _descriptionSortAction;
@synthesize dateSortAction = _dateSortAction;
@synthesize emptySpeedDialView = _emptySpeedDialView;

#pragma mark - INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
    _browserState =
        _browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(_browserState);
    _bookmarks = ios::BookmarkModelFactory::GetForBrowserState(_browserState);
    _bridge.reset(new bookmarks::BookmarkModelBridge(self, _bookmarks));
  }
  return self;
}

- (void)dealloc {
  _speedDialViewController.delegate = nil;
  _bookmarkFolderAddEditorController.delegate = nil;
  _bookmarkURLAddEditorController.delegate = nil;
  _topScrollMenu.delegate = nil;
  self.mediator.consumer = nil;
  [self.mediator disconnect];
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [self setUpUI];
  if (self.bookmarks->loaded()) {
    [self loadSpeedDialViews];
  }
}

-(void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Refresh only the body if the sorting mode is changed by dragging
  BOOL refreshable = self.speedDialPositionModifiedFlag &&
                     self.bookmarks->loaded() &&
                     self.mediator;
  if (!refreshable)
    return;
  [self.mediator computeSpeedDialChildItems:nil];
}

-(void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  // If the top menu is not visible show it.
  if (self.topScrollMenuContainer.alpha != 0) {
    return;
  }
  [UIView animateWithDuration:0.3 animations:^{
    [self.topScrollMenuContainer setAlpha:1];
  }];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // The navigation bar height is not accurate when app is launched from the
  // landscape mode. Therefore, this method is triggered after layout is set up
  // to properly resize the top menu within the navigation bar.
  [self refreshTopMenuLayout];

  [self.collectionView reloadData];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if ((self.traitCollection.verticalSizeClass !=
          previousTraitCollection.verticalSizeClass) ||
      (self.traitCollection.horizontalSizeClass !=
          previousTraitCollection.horizontalSizeClass)) {
    [self handleDeviceOrientationChange];
  }
}

#pragma mark - PRIVATE
#pragma mark - SET UP UI COMPONENTS

/// Set up the UI Components
- (void)setUpUI {
  self.view.backgroundColor =
    [UIColor colorNamed:vNTPSpeedDialContainerbackgroundColor];
  [self setUpHeaderView];
  [self setupSpeedDialView];
  [self setupEmptySpeedDialView];
}

/// Set up the header view
- (void)setUpHeaderView {
  // Navigation bar customisation
  self.navigationController.navigationBar.backgroundColor =
    [UIColor colorNamed:vNTPBackgroundColor];
  self.navigationController.navigationBar.translucent = NO;

  UINavigationBarAppearance* appearance =
    [[UINavigationBarAppearance alloc] init];
  [appearance setBackgroundColor: [UIColor colorNamed:vNTPBackgroundColor]];

  NSDictionary* attributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
  };

  [appearance setTitleTextAttributes:attributes];
  [[UINavigationBar appearance] setScrollEdgeAppearance:appearance];
  [[UINavigationBar appearance] setStandardAppearance:appearance];
  [[UINavigationBar appearance] setCompactAppearance:appearance];

  // Scroll menu
  UINavigationBar* navBar = self.navigationController.navigationBar;

  UIView *topScrollMenuContainer = [UIView new];
  _topScrollMenuContainer = topScrollMenuContainer;
  topScrollMenuContainer.backgroundColor = UIColor.clearColor;
  VivaldiNTPTopMenuView *topScrollMenu =
      [[VivaldiNTPTopMenuView alloc] initWithHeight: navBar.frame.size.height];
  _topScrollMenu = topScrollMenu;
  topScrollMenu.delegate = self;
  [self.topScrollMenuContainer addSubview: topScrollMenu];
  [topScrollMenu anchorTop: self.topScrollMenuContainer.topAnchor
                   leading: self.topScrollMenuContainer.leadingAnchor
                    bottom: self.topScrollMenuContainer.bottomAnchor
                  trailing: nil
                   padding: topScrollMenuPadding];

  // Sort button
  UIButton *sortButton = [UIButton new];
  _sortButton = sortButton;
  [sortButton setImage:[UIImage imageNamed:vNTPToolbarSortIcon]
              forState:UIControlStateNormal];
  [sortButton setImage:[UIImage imageNamed:vNTPToolbarSortIcon]
              forState:UIControlStateHighlighted];
  sortButton.showsMenuAsPrimaryAction = YES;
  sortButton.menu = [self contextMenuForSpeedDialSortButton];
  [topScrollMenuContainer addSubview: _sortButton];
  [_sortButton anchorTop: nil
                 leading: self.topScrollMenu.trailingAnchor
                  bottom: nil
                trailing: self.topScrollMenuContainer.trailingAnchor
                 padding: sortButtonPadding
                    size: sortButtonSize];
  [sortButton centerYInSuperview];

  // Add the top scroll menu menu container as subview on navigation bar
  [navBar addSubview:topScrollMenuContainer];
  [topScrollMenuContainer anchorTop: navBar.topAnchor
                            leading: navBar.safeLeftAnchor
                             bottom: navBar.bottomAnchor
                           trailing: navBar.safeRightAnchor];
}

/// Set up the speed dial view
-(void)setupSpeedDialView {
  // The container view to hold the speed dial view
  UIView* bodyContainerView = [UIView new];
  bodyContainerView.backgroundColor = UIColor.clearColor;
  [self.view addSubview:bodyContainerView];

  [bodyContainerView anchorTop:self.view.safeTopAnchor
                       leading:self.view.safeLeftAnchor
                        bottom:self.view.safeBottomAnchor
                      trailing:self.view.safeRightAnchor];

  UICollectionViewFlowLayout *layout=
    [[UICollectionViewFlowLayout alloc] init];
  layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  layout.minimumInteritemSpacing = 0;
  layout.minimumLineSpacing = 0;
  layout.sectionInset = UIEdgeInsetsZero;
  layout.estimatedItemSize = UICollectionViewFlowLayoutAutomaticSize;
  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:layout];
  _collectionView = collectionView;
  [_collectionView setDataSource: self];
  [_collectionView setDelegate: self];
  _collectionView.showsHorizontalScrollIndicator = NO;
  _collectionView.showsVerticalScrollIndicator = NO;
  _collectionView.contentInsetAdjustmentBehavior =
    UIScrollViewContentInsetAdjustmentNever;
  _collectionView.backgroundColor = UIColor.clearColor;
  _collectionView.decelerationRate = UIScrollViewDecelerationRateFast;
  _collectionView.pagingEnabled = YES;

  // Register cell
  [_collectionView registerClass:[VivaldiSpeedDialContainerCell class]
      forCellWithReuseIdentifier:cellId];

  [bodyContainerView addSubview:_collectionView];
  [_collectionView fillSuperview];
}

-(void)setupEmptySpeedDialView {
  // Empty speed dial view. Set its alpha to 0 initially. This will be
  // toggled based on response from the mediator.
  VivaldiSpeedDialEmptyView* emptySpeedDialView =
    [VivaldiSpeedDialEmptyView new];
  _emptySpeedDialView = emptySpeedDialView;
  [self.view addSubview:emptySpeedDialView];
  [emptySpeedDialView fillSuperview];
  emptySpeedDialView.alpha = 0;
}

// Returns the context menu actions for speed dial sort button action
- (UIMenu*)contextMenuForSpeedDialSortButton {

  // Manual sorting action button
  NSString* manualSortTitle = l10n_util::GetNSString(IDS_IOS_SORT_MANUAL);
  UIAction* manualSortAction = [UIAction actionWithTitle:manualSortTitle
                                              image:nil
                                              identifier:nil
                                            handler:^(__kindof UIAction*_Nonnull
                                                      action) {
    [self sortSpeedDialsWithMode:SpeedDialSortingManual];
  }];
  self.manualSortAction = manualSortAction;

  // Sort by title action button
  NSString* titleSortTitle = l10n_util::GetNSString(IDS_IOS_SORT_BY_TITLE);
  UIAction* titleSortAction = [UIAction actionWithTitle:titleSortTitle
                                              image:nil
                                         identifier:nil
                                            handler:^(__kindof UIAction*_Nonnull
                                                      action) {
    [self sortSpeedDialsWithMode:SpeedDialSortingByTitle];
  }];
  self.titleSortAction = titleSortAction;

  // Sort by address action button
  NSString* addressSortTitle = l10n_util::GetNSString(IDS_IOS_SORT_BY_ADDRESS);
  UIAction* addressSortAction = [UIAction actionWithTitle:addressSortTitle
                                              image:nil
                                         identifier:nil
                                            handler:^(__kindof UIAction*_Nonnull
                                                      action) {
    [self sortSpeedDialsWithMode:SpeedDialSortingByAddress];
  }];
  self.addressSortAction = addressSortAction;

  // Sort by nickname action button
  NSString* nicknameSortTitle =
    l10n_util::GetNSString(IDS_IOS_SORT_BY_NICKNAME);
  UIAction* nicknameSortAction = [UIAction actionWithTitle:nicknameSortTitle
                                              image:nil
                                         identifier:nil
                                            handler:^(__kindof UIAction*_Nonnull
                                                      action) {
    [self sortSpeedDialsWithMode:SpeedDialSortingByNickname];
  }];
  self.nicknameSortAction = nicknameSortAction;

  // Sort by description action button
  NSString* descriptionSortTitle =
    l10n_util::GetNSString(IDS_IOS_SORT_BY_DESCRIPTION);
  UIAction* descriptionSortAction =
    [UIAction actionWithTitle:descriptionSortTitle
                        image:nil
                   identifier:nil
                      handler:^(__kindof UIAction*_Nonnull action) {
    [self sortSpeedDialsWithMode:SpeedDialSortingByDescription];
  }];
  self.descriptionSortAction = descriptionSortAction;

  // Sort by date created action button
  NSString* dateSortTitle = l10n_util::GetNSString(IDS_IOS_SORT_BY_DATE);
  UIAction* dateSortAction = [UIAction actionWithTitle:dateSortTitle
                                              image:nil
                                         identifier:nil
                                            handler:^(__kindof UIAction*_Nonnull
                                                      action) {
    [self sortSpeedDialsWithMode:SpeedDialSortingByDate];
  }];
  self.dateSortAction = dateSortAction;

  _speedDialSortActions = [[NSMutableArray alloc] initWithArray:@[
    manualSortAction, titleSortAction, addressSortAction,
    nicknameSortAction, descriptionSortAction, dateSortAction
  ]];

  [self setSortingStateOnContextMenuOption];

  UIMenu* menu = [UIMenu menuWithTitle:@"" children:_speedDialSortActions];
  return menu;
}

#pragma mark - PRIVATE METHODS

/// Create and start mediator to load the speed dial items in the menu and on the body.
- (void)loadSpeedDialViews {
  self.mediator = [[VivaldiSpeedDialHomeMediator alloc]
                      initWithBrowserState:self.browserState
                             bookmarkModel:self.bookmarks];
  self.mediator.consumer = self;
  [self.mediator startMediating];
}

/// Device orientation change handler
- (void)handleDeviceOrientationChange {
  // When device is rotated invalidate the height of the top scroll menu
  // since the navigation bar height is smalled in landscape mode than the
  // portrait.
  [self refreshTopMenuLayout];
  // Invalidate layout to refresh the item position and size.
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (void)refreshTopMenuLayout {
  [self.view layoutIfNeeded];
  UINavigationBar* navBar = self.navigationController.navigationBar;
  CGFloat height = navBar.frame.size.height;
  [self.topScrollMenu invalidateLayoutWithHeight:height];
}

/// Set current selected index of the menu on the shared state
- (void)setSelectedMenuItemIndex:(NSInteger)index {
  VivaldiSpeedDialSharedState *sharedState =
      [VivaldiSpeedDialSharedState manager];
  sharedState.selectedIndex = index;
}

/// Returns the currently selected index of the menu from the shared state
- (NSInteger)selectedMenuItemIndex {
  VivaldiSpeedDialSharedState *sharedState =
      [VivaldiSpeedDialSharedState manager];
  return sharedState.selectedIndex;
}

/// Set speed dial modified property on the shared state
- (void)setSpeedDialPositionModifiedFlag:(BOOL)flag {
  VivaldiSpeedDialSharedState *sharedState =
      [VivaldiSpeedDialSharedState manager];
  sharedState.isSpeedDialPositionModified = flag;
}

/// Returns the speed dial modified property from the shared state
- (BOOL)speedDialPositionModifiedFlag {
  VivaldiSpeedDialSharedState *sharedState =
      [VivaldiSpeedDialSharedState manager];
  return sharedState.isSpeedDialPositionModified;
}

/// Gets the sorting mode from prefs and update the selection state on the context menu.
- (void)setSortingStateOnContextMenuOption {

  switch (self.currentSortingMode) {
    case SpeedDialSortingManual:
      [self updateSortActionButtonState:self.manualSortAction];
      break;
    case SpeedDialSortingByTitle:
      [self updateSortActionButtonState:self.titleSortAction];
      break;
    case SpeedDialSortingByAddress:
      [self updateSortActionButtonState:self.addressSortAction];
      break;
    case SpeedDialSortingByNickname:
      [self updateSortActionButtonState:self.nicknameSortAction];
      break;
    case SpeedDialSortingByDescription:
      [self updateSortActionButtonState:self.descriptionSortAction];
      break;
    case SpeedDialSortingByDate:
      [self updateSortActionButtonState:self.dateSortAction];
      break;
  }
}

/// Updates the state on the context menu actions by uncehcking the all apart from the selected one.
- (void)updateSortActionButtonState:(UIAction*)settable {
  for (UIAction* action in self.speedDialSortActions) {
    if (action == settable) {
      [action setState:UIMenuElementStateOn];
    } else {
      [action setState:UIMenuElementStateOff];
    }
  }
}

/// Sets current sorting mode selected by the user on the pref and calls the mediator to handle the sorting
/// and refreshed the UI with sorted items.
- (void)sortSpeedDialsWithMode:(SpeedDialSortingMode)mode {

  if (self.currentSortingMode == mode)
    return;

  // Set new mode to the pref.
  [self setCurrentSortingMode:mode];

  // Refresh the context menu options after the selection.
  [self resetSortButtonContextMenuOptions];

  if (mode == SpeedDialSortingManual) {
    // For manual mode ping the mediator to compute and return the child items
    // as it is without any sorting algo applied by us.
    [self.mediator computeSpeedDialChildItems:nil];
  } else {
    // Ping the mediator to sort items and notify the consumers.
    [self.mediator computeSortedItems:self.speedDialChildItems byMode:mode];
  }
}

/// Returns current sorting mode
- (SpeedDialSortingMode)currentSortingMode {
  return [VivaldiStartPagePrefs
            getSDSortingModeWithPrefService:self.browserState->GetPrefs()];
}

/// Sets the user selected sorting mode on the prefs
- (void)setCurrentSortingMode:(SpeedDialSortingMode)mode {
  [VivaldiStartPagePrefs setSDSortingMode:mode
                           inPrefServices:self.browserState->GetPrefs()];
}

/// Returns current layout style for start page
- (VivaldiStartPageLayoutStyle)currentLayoutStyle {
  return [VivaldiStartPagePrefs
          getStartPageLayoutStyleWithPrefService:self.browserState->GetPrefs()];
}

/// Refresh sort button context menu options
- (void)resetSortButtonContextMenuOptions {
  self.sortButton.menu = [self contextMenuForSpeedDialSortButton];
}

/// Scroll to the selected page in the collection view.
- (void)scrollToItemWithIndex:(NSInteger)index {
  BOOL scrollable = self.speedDialMenuItems.count > 0 &&
                    self.speedDialChildItems.count > 0;
  if (!scrollable)
    return;

  NSIndexPath *indexPath = [NSIndexPath indexPathForRow:index
                                              inSection:0];
  [self.collectionView
      selectItemAtIndexPath:indexPath
                   animated:YES
             scrollPosition:UICollectionViewScrollPositionCenteredHorizontally];
  [self.collectionView
      scrollToItemAtIndexPath:indexPath
             atScrollPosition:UICollectionViewScrollPositionCenteredHorizontally
                     animated:YES];
}

/// Presents bookmark folder editor controller wrapped within a navigation controller.
/// Parameters: Item can be nil, parent is non-null and a boolean whether presenting on editing mode or not
/// is provided.
- (void)presentBookmarkFolderEditor:(VivaldiSpeedDialItem*)item
                             parent:(VivaldiSpeedDialItem*)parent
                          isEditing:(BOOL)isEditing {
  VivaldiBookmarkAddEditFolderViewController* controller =
    [VivaldiBookmarkAddEditFolderViewController
       initWithBrowser: self.browser
                  item: item
                parent: parent
             isEditing: isEditing
          allowsCancel: YES];

  UINavigationController *newVC =
      [[UINavigationController alloc]
        initWithRootViewController:controller];
  controller.delegate = self;

  // Present the nav bar controller on top of the parent
  [self.parentViewController presentViewController:newVC
                                          animated:YES
                                        completion:nil];
  self.bookmarkFolderAddEditorController = controller;
}

/// Presents bookmark url editor controller wrapped within a navigation controller.
/// Parameters: Item can be nil, parent is non-null and a boolean whether presenting on editing mode or not
/// is provided.
- (void)presentBookmarkURLEditor:(VivaldiSpeedDialItem*)item
                          parent:(VivaldiSpeedDialItem*)parent
                       isEditing:(BOOL)isEditing {
  VivaldiBookmarkAddEditURLViewController* controller =
    [VivaldiBookmarkAddEditURLViewController
     initWithBrowser: self.browser
                item: item
              parent: parent
           isEditing: isEditing
        allowsCancel: YES];

  UINavigationController *newVC =
      [[UINavigationController alloc]
        initWithRootViewController:controller];
  controller.delegate = self;

  // Present the nav bar controller on top of the parent
  [self.parentViewController presentViewController:newVC
                                          animated:YES
                                        completion:nil];
  self.bookmarkURLAddEditorController = controller;
}

- (void)showEmptySpeedDialView {
  if (self.emptySpeedDialView.alpha == 1)
    return;
  self.emptySpeedDialView.alpha = 1;
  [self.navigationController setNavigationBarHidden:YES
                                           animated:YES];
}

- (void)hideEmptySpeedDialView {
  if (self.emptySpeedDialView.alpha == 0)
    return;
  self.emptySpeedDialView.alpha = 0;
  [self.navigationController setNavigationBarHidden:NO
                                           animated:YES];
}

#pragma mark - COLLECTIONVIEW DATA SOURCE
- (NSInteger)collectionView:(UICollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section{
    return self.speedDialMenuItems.count;
}

- (UICollectionViewCell*)collectionView:(UICollectionView *)collectionView
                           cellForItemAtIndexPath:(NSIndexPath *)indexPath {
  VivaldiSpeedDialContainerCell *cell =
    [collectionView dequeueReusableCellWithReuseIdentifier:cellId
                                            forIndexPath:indexPath];
  NSInteger index = indexPath.row;
  cell.delegate = self;
  VivaldiSpeedDialItem* parent =
    [self.speedDialMenuItems objectAtIndex:index];
  NSArray* childItems = [self.speedDialChildItems objectAtIndex:index];
  [cell configureWith:childItems
               parent:parent
        faviconLoader:self.faviconLoader
          layoutStyle:[self currentLayoutStyle]
    deviceOrientation:!self.isDevicePortrait];
  [cell setCurrentPage: index];
  return cell;
}

- (CGPoint)collectionView:(UICollectionView *)collectionView
targetContentOffsetForProposedContentOffset:(CGPoint)proposedContentOffset {
  NSIndexPath *currentVisibleIndexPath =
    [NSIndexPath indexPathForRow:self.selectedMenuItemIndex
                       inSection:0];
  UICollectionViewLayoutAttributes* attributes =
    [collectionView layoutAttributesForItemAtIndexPath:currentVisibleIndexPath];
  CGPoint newOriginForOldIndex = attributes.frame.origin;
  return newOriginForOldIndex;
}

- (void)scrollViewWillBeginDecelerating:(UIScrollView *)scrollView {
  if (self.scrollFromMenuTap) {
    self.scrollFromMenuTap = NO;
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView *)scrollView {
  if (self.scrollFromMenuTap)
    return;

  CGFloat xPoint = scrollView.contentOffset.x + scrollView.frame.size.width / 2;
  CGFloat yPoint = scrollView.frame.size.height / 2;
  CGPoint center = CGPointMake(xPoint, yPoint);
  NSIndexPath* currentIndexPath =
    [self.collectionView indexPathForItemAtPoint:center];

  if (currentIndexPath) {
    [self.topScrollMenu selectItemWithIndex:currentIndexPath.row animated:YES];
    [self setSelectedMenuItemIndex:currentIndexPath.row];
  }
}

#pragma mark - FLOW LAYOUT
- (CGSize)collectionView:(UICollectionView *)collectionView
                  layout:(UICollectionViewLayout*)collectionViewLayout
  sizeForItemAtIndexPath:(NSIndexPath *)indexPath {
  CGFloat height = collectionView.bounds.size.height;
  CGFloat width = collectionView.bounds.size.width;
  return CGSizeMake(width, height);
}

#pragma mark - PUBLIC SETTERS
- (void)popPushedViewControllers {
  self.speedDialViewController = nil;
  [self.navigationController popToRootViewControllerAnimated:NO];
}

#pragma mark - SPEED DIAL HOME CONSUMER
- (void)refreshContents {
  [self.mediator computeSpeedDialFolders];
}

- (void)refreshMenuItems:(NSArray*)items {
  if (items.count <= 0) {
    [self.speedDialMenuItems removeAllObjects];
    [self.topScrollMenu removeAllItems];
    [self showEmptySpeedDialView];
    return;
  }

  [self hideEmptySpeedDialView];
  self.speedDialMenuItems = [[NSMutableArray alloc] initWithArray:items];
  [self.topScrollMenu setMenuItemsWithSDFolders:items
                                  selectedIndex:self.selectedMenuItemIndex];
}

- (void)refreshChildItems:(NSArray*)items {
  BOOL refreshable =
    self.speedDialMenuItems && self.speedDialMenuItems.count > 0;
  if (!refreshable)
    return;

  self.speedDialChildItems = [[NSMutableArray alloc] initWithArray:items];
  [CATransaction begin];
  [CATransaction setValue:(id)kCFBooleanTrue
                   forKey:kCATransactionDisableActions];
  [self.collectionView performBatchUpdates:^{
    [self.collectionView reloadSections:[NSIndexSet indexSetWithIndex:0]];
  } completion: ^(BOOL finished){
    [self scrollToItemWithIndex:self.selectedMenuItemIndex];
    [CATransaction commit];
  }];
}

- (void)reloadLayout {
  [self.collectionView reloadData];
}

#pragma mark - TOP MENU DELEGATE
- (void)didSelectItem:(VivaldiSpeedDialItem*)item
                index:(NSInteger)index {
  self.scrollFromMenuTap = YES;
  [self scrollToItemWithIndex:index];
  [self setSelectedMenuItemIndex:index];
  [self.collectionView reloadData];
}

#pragma mark - VIVALDI_SPEED_DIAL_CONTAINER_DELEGATE
- (void)didSelectItem:(VivaldiSpeedDialItem*)item
               parent:(VivaldiSpeedDialItem*)parent {
  // Hide the top menu before navigating
  if (self.topScrollMenuContainer.alpha == 1) {
    [self.topScrollMenuContainer setAlpha:0];
  }
  // Navigate to the folder if the selected speed dial is a folder.
  if (item.isFolder) {
    VivaldiSpeedDialViewController *controller =
      [VivaldiSpeedDialViewController initWithItem:item
                                            parent:parent
                                         bookmarks:self.bookmarks
                                           browser:self.browser
                                     faviconLoader:self.faviconLoader];
    controller.delegate = self;
    [self.navigationController pushViewController:controller
                                         animated:YES];
    self.speedDialViewController = controller;
  } else {
    // Pass it to delegate to open the URL.
    BOOL captureSnapshot = item.thumbnail.length == 0;
    if (self.delegate)
      [self.delegate didTapSpeedDialWithItem:item
                             captureSnapshot:captureSnapshot];
    [self.navigationController popToRootViewControllerAnimated:NO];
  }
}

- (void)didSelectEditItem:(VivaldiSpeedDialItem*)item
                   parent:(VivaldiSpeedDialItem*)parent {
  if (item.isFolder) {
    [self presentBookmarkFolderEditor:item
                               parent:parent
                            isEditing:YES];
  } else {
    [self presentBookmarkURLEditor:item
                            parent:parent
                         isEditing:YES];
  }
}

- (void)didSelectMoveItem:(VivaldiSpeedDialItem*)item
                   parent:(VivaldiSpeedDialItem*)parent {

  BOOL isMovable = parent.parent &&
    (item.parent->id() != parent.parent->id()) &&
    parent.parent->is_folder();

  if (!isMovable) {
    return;
  }

  if (item.isFolder) {
    std::set<const BookmarkNode*> editedNodes;
    editedNodes.insert(item.bookmarkNode);
    bookmark_utils_ios::MoveBookmarks(editedNodes,
                                      self.bookmarks,
                                      parent.parent);

  } else {
    self.bookmarks->Move(item.bookmarkNode,
                         parent.parent,
                         parent.parent->children().size());
  }
}

- (void)didMoveItemByDragging:(VivaldiSpeedDialItem*)item
                       parent:(VivaldiSpeedDialItem*)parent
                   toPosition:(NSInteger)position {
  // Change sorting mode to 'Manual' if user performs drag and drop action on
  // the start page.
  // Set new mode to the pref.
  [self setCurrentSortingMode:SpeedDialSortingManual];
  // Refresh the context menu options after the selection.
  [self resetSortButtonContextMenuOptions];

  if (!item.parent) {
    return;
  }

  self.bookmarks->Move(item.bookmarkNode,
                       item.parent,
                       position);
  [self setSpeedDialPositionModifiedFlag:YES];
}

- (void)didSelectDeleteItem:(VivaldiSpeedDialItem*)item
                     parent:(VivaldiSpeedDialItem*)parent {

  if (self.bookmarks->loaded() && item.bookmarkNode) {
    std::set<const BookmarkNode*> nodes;
    nodes.insert(item.bookmarkNode);
    bookmark_utils_ios::DeleteBookmarks(nodes, self.bookmarks);
    [[NSFileManager defaultManager] removeThumbnailForSDItem:item];
  }
}

- (void)didSelectAddNewSpeedDial:(BOOL)isFolder
                          parent:(VivaldiSpeedDialItem*)parent {
  if (isFolder) {
    [self presentBookmarkFolderEditor:nil
                               parent:parent
                            isEditing:NO];
  } else {
    [self presentBookmarkURLEditor:nil
                            parent:parent
                         isEditing:NO];
  }
}

#pragma mark - VIVALDI_BOOKMARK_ADD_EDIT_CONTROLLER_DELEGATE
- (void)didCreateNewFolder:(const bookmarks::BookmarkNode*)folder {
  // No op here. The collection will be reloaded if anythings changes in the
  // bookmark model.
}

#pragma mark - BOOKMARK MODEL OBSERVER

- (void)bookmarkModelLoaded {
  // If the view hasn't loaded yet, then return early. The eventual call to
  // viewDidLoad will properly initialize the views.  This early return must
  // come *after* the call to setRootNode above.
  if (![self isViewLoaded])
    return;
  [self loadSpeedDialViews];
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)node {
  // No-op. Mediator is responsible for refreshing.
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // No-op. Mediator is responsible for refreshing.
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  // No-op. Mediator is responsible for refreshing.
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)folder {
  // No-op. Mediator is responsible for refreshing.
}

- (void)bookmarkModelRemovedAllNodes {
  // No-op. Mediator is responsible for refreshing.
}

@end
