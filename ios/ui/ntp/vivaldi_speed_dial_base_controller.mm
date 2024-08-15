// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_base_controller.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmark_add_edit_folder_view_controller.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmark_add_edit_url_view_controller.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_constants.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/cells/vivaldi_speed_dial_container_cell.h"
#import "ios/ui/ntp/top_menu/vivaldi_ntp_top_menu.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_home_mediator.h"
#import "ios/ui/ntp/vivaldi_speed_dial_shared_state.h"
#import "ios/ui/ntp/vivaldi_speed_dial_sorting_mode.h"
#import "ios/ui/ntp/vivaldi_speed_dial_view_controller.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs_helper.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"
#import "ios/ui/thumbnail/thumbnail_capturer_swift.h"
#import "ios/ui/thumbnail/vivaldi_thumbnail_service.h"
#import "prefs/vivaldi_pref_names.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using bookmarks::BookmarkModel;

// Namespace
namespace {
// Cell Identifier for the container cell
NSString* cellId = @"cellId";
// Notification Identifier For Background Wallpaper
NSString* vivaldiWallpaperUpdate = @"VivaldiBackgroundWallpaperUpdate";
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
  std::unique_ptr<BookmarkModelBridge> _bridge;
}
// Collection view that holds the children of speed dial folder.
@property (weak, nonatomic) UICollectionView *collectionView;
// Container view for holding the menu items and sort button.
@property(nonatomic, weak) UIView* topScrollMenuContainer;
// Top scroll menu view
@property(nonatomic, weak) VivaldiNTPTopMenuView* topScrollMenu;
// Sort button
@property(nonatomic, weak) UIButton* sortButton;
// The view controller to present when pushing to new view controller
@property(nonatomic, strong)
  VivaldiSpeedDialViewController* speedDialViewController;
// The view controller to present when pushing to add or edit folder
@property(nonatomic, strong)
  VivaldiBookmarkAddEditFolderViewController* bookmarkFolderAddEditorController;
// The view controller to present when pushing to add or edit item
@property(nonatomic, strong)
  VivaldiBookmarkAddEditURLViewController* bookmarkURLAddEditorController;
// The background Image for Speed Dial
@property(nonatomic, strong) UIImageView* backgroundImageView;
// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic, assign) FaviconLoader* faviconLoader;
// The Browser in which bookmarks are presented
@property(nonatomic, assign) Browser* browser;
// Bookmarks model that holds all the bookmarks data
@property (assign,nonatomic) bookmarks::BookmarkModel* bookmarks;
// Shared state for the speed dial item
@property(nonatomic, strong)
  VivaldiSpeedDialSharedState* speedDialSharedState;
// The user's browser state model used.
@property(nonatomic, assign) ChromeBrowserState* browserState;
// The service class that manages the speed dial thumbnail locally such as
// store/remove or fetch.
@property(nonatomic, strong) VivaldiThumbnailService* vivaldiThumbnailService;
// The capturer that captures the thumbnail and pass the image through a
// a callback which is consumed by VivaldiThumbnailService.
@property(nonatomic, strong) VivaldiThumbnailCapturer* thumbnailCapturer;
// The mediator that provides data for this view controller.
@property(nonatomic, strong) VivaldiSpeedDialHomeMediator* mediator;

// Array to hold the speed dial menu items
@property (strong, nonatomic) NSMutableArray *speedDialMenuItems;
// Array to hold the speed dial folders children
@property (strong, nonatomic) NSMutableArray *speedDialChildItems;
// Array to hold the sort button context menu options
@property (strong, nonatomic) NSMutableArray *speedDialSortActions;
// Parent to pass for new speed dial/folder item.
@property(nonatomic, strong) VivaldiSpeedDialItem* bookmarkBarItem;
// A boolean to keep track when the scrolling is taking place due to tap in the
// top menu item. There are two types of scroll event. One happens when user
// swipes the collection view left or right by pan/swipe gesture. The other one
// is when taps on the menu item.
@property (assign, nonatomic) BOOL scrollFromMenuTap;
// Height constraint of the top menu container
@property (assign, nonatomic) NSLayoutConstraint* menuContainerHeight;

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
@synthesize backgroundImageView = _backgroundImageView;
@synthesize bookmarkBarItem = _bookmarkBarItem;

#pragma mark - INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
    _browserState =
        _browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(_browserState);
    _bookmarks = ios::LocalOrSyncableBookmarkModelFactory::
                      GetForBrowserState(_browserState);
    _vivaldiThumbnailService = [VivaldiThumbnailService new];
    _thumbnailCapturer = [[VivaldiThumbnailCapturer alloc] init];
    [VivaldiStartPagePrefs setPrefService:self.browserState->GetPrefs()];
    _bridge.reset(new BookmarkModelBridge(self, _bookmarks));
  }
  return self;
}

- (void)dealloc {
  _bookmarkBarItem = nil;
  _thumbnailCapturer = nil;
  _speedDialViewController.delegate = nil;
  _bookmarkFolderAddEditorController.delegate = nil;
  _bookmarkURLAddEditorController.delegate = nil;
  _topScrollMenu.delegate = nil;
  _vivaldiThumbnailService = nullptr;
  self.mediator.consumer = nil;
  [self.mediator disconnect];
  [[NSNotificationCenter defaultCenter] removeObserver: self
                                        name: vivaldiWallpaperUpdate
                                        object: nil];
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [self setUpUI];
  if (self.bookmarks->loaded()) {
    [self loadSpeedDialViews];
  }
  [self setupNotifications];
}

-(void)setupNotifications {
  [[NSNotificationCenter defaultCenter] addObserver: self
                                        selector: @selector(updateWallpaper)
                                        name: vivaldiWallpaperUpdate
                                        object: nil];
}

- (void)updateWallpaper {
    __weak __typeof(self) weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
      __strong __typeof(weakSelf) strongSelf = weakSelf;
      if (strongSelf) {
        strongSelf.backgroundImageView.image = [strongSelf getWallpaperImage];
      }
    });
}

- (UIImage *)getWallpaperImage {
  // Loading the image name from preferences
  NSString *wallpaper = [self selectedDefaultWallpaper];

  // setting it to nil if string is empty
  // so that we don't get warning : Invalid asset name supplied
  if ([wallpaper isEqualToString:@""]) {
    wallpaper = nil;
  }
  // Create and set the background image
  UIImage *wallpaperImage = wallpaper ? [UIImage imageNamed:wallpaper] : nil;
  // Check if the wallpaper name is nil and get custom wallpaper
  if (wallpaper == nil) {
    wallpaperImage = [self selectedCustomWallpaper];
  }
  return wallpaperImage;
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
  [self.collectionView.collectionViewLayout invalidateLayout];
  [self.collectionView reloadData];
  [self scrollToItemWithIndex:self.selectedMenuItemIndex];
}

- (void)viewWillTransitionToSize:(CGSize)size
    withTransitionCoordinator:
      (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  // Determine the target background image based on the new orientation
  UIImage *targetImage = [self getWallpaperImage];
  // Animate the transition
  [coordinator animateAlongsideTransition:
    ^(id<UIViewControllerTransitionCoordinatorContext> context) {
      [UIView
        transitionWithView:self.backgroundImageView
        duration:0.5
        options:UIViewAnimationOptionTransitionCrossDissolve
        animations:^{
          self.backgroundImageView.image = targetImage;
        } completion:nil];
  } completion:nil];
}

#pragma mark - PRIVATE
#pragma mark - SET UP UI COMPONENTS

/// Set up the UI Components
- (void)setUpUI {
  self.view.backgroundColor =
    [UIColor colorNamed:vNTPSpeedDialContainerbackgroundColor];
  [self setupSpeedDialBackground];
  [self setUpHeaderView];
  [self setupSpeedDialView];
}

-(void)setupSpeedDialBackground {
  UIImageView* backgroundImageView =
      [[UIImageView alloc] initWithImage:[self getWallpaperImage]];
  backgroundImageView.contentMode = UIViewContentModeScaleAspectFill;
  backgroundImageView.clipsToBounds = YES;
  self.backgroundImageView = backgroundImageView;
  [self.view addSubview:backgroundImageView];
  [self.backgroundImageView fillSuperview];
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
                        bottom:self.view.bottomAnchor
                      trailing:self.view.safeRightAnchor];

  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:[self createLayout]];
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

- (UICollectionViewLayout*)createLayout {
  UICollectionViewCompositionalLayoutSectionProvider sectionProvider =
    ^NSCollectionLayoutSection* (
                                 NSInteger sectionIndex,
                                 id<NSCollectionLayoutEnvironment> environment
                                 ) {
         // Item
         NSCollectionLayoutSize *itemSize =
            [NSCollectionLayoutSize sizeWithWidthDimension:
             [NSCollectionLayoutDimension fractionalWidthDimension:1.0]
             heightDimension:[NSCollectionLayoutDimension
                              fractionalHeightDimension:1.0]];
         NSCollectionLayoutItem *item = [NSCollectionLayoutItem
                                         itemWithLayoutSize:itemSize];

         // Group
         NSCollectionLayoutSize *groupSize =
            [NSCollectionLayoutSize sizeWithWidthDimension:
             [NSCollectionLayoutDimension fractionalWidthDimension:1.0]
             heightDimension:[NSCollectionLayoutDimension
                              fractionalHeightDimension:1.0]];
         NSCollectionLayoutGroup *group =
            [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                                           subitem:item
                                                             count:1];

         // Section
         NSCollectionLayoutSection *section =
            [NSCollectionLayoutSection sectionWithGroup:group];
         return section;
     };

    UICollectionViewCompositionalLayoutConfiguration *config =
        [[UICollectionViewCompositionalLayoutConfiguration alloc] init];
    config.scrollDirection = UICollectionViewScrollDirectionHorizontal;
    UICollectionViewCompositionalLayout *layout =
        [[UICollectionViewCompositionalLayout alloc]
          initWithSectionProvider:sectionProvider configuration:config];
    return layout;
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

  // Parent bookmark bar node item
  VivaldiSpeedDialItem* bookmarkBarItem =
      [[VivaldiSpeedDialItem alloc]
          initWithBookmark:self.bookmarks->bookmark_bar_node()];
  _bookmarkBarItem = bookmarkBarItem;
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
  return [VivaldiStartPagePrefsHelper getSDSortingMode];
}

/// Sets the user selected sorting mode on the prefs
- (void)setCurrentSortingMode:(SpeedDialSortingMode)mode {
  [VivaldiStartPagePrefsHelper setSDSortingMode:mode];
}

/// Returns current layout style for start page
- (VivaldiStartPageLayoutStyle)currentLayoutStyle {
  return [VivaldiStartPagePrefsHelper getStartPageLayoutStyle];
}

/// Returns preloaded wallpaper name
- (NSString*)selectedDefaultWallpaper {
  return [VivaldiStartPagePrefsHelper getWallpaperName];
}

/// Returns custom wallpaper name
- (UIImage*)selectedCustomWallpaper {
  // It doesn't require size traits, image contentMode is aspect fill
  UIImage *wallpaper =
    UIDeviceOrientationIsLandscape([UIDevice currentDevice].orientation)
      ? [VivaldiStartPagePrefsHelper getLandscapeWallpaper] :
          [VivaldiStartPagePrefsHelper getPortraitWallpaper];
  return wallpaper;
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

- (void)captureThumbnailForItem:(VivaldiSpeedDialItem*)item
                    isMigrating:(BOOL)isMigrating
                  shouldReplace:(BOOL)shouldReplace {
  NSNumber *bookmarkNodeId = @(item.bookmarkNode->id());
  [self notifyUpdateForItemWithID:bookmarkNodeId
                 captureThumbnail:YES
                  finishedCapture:NO];

  NSURL *url = [NSURL URLWithString:item.urlString];
  [self.thumbnailCapturer captureSnapshotWithURL:url
                              completion:^(UIImage *image, NSError *error) {
    [self notifyUpdateForItemWithID:bookmarkNodeId
                   captureThumbnail:YES
                    finishedCapture:YES];
    if (image) {
      [self.vivaldiThumbnailService
           storeThumbnailForSDItem:item
                          snapshot:image
                           replace:shouldReplace
                       isMigrating:isMigrating
                         bookmarks:self.bookmarks];
    }
  }];
}

/// Notifies the listeners that the speed dial item property
/// has been updated. This can be triggered when a thumbnail capture
/// event happens whether refresh starts or ends, also when other
/// properties like title, url etc changes from other actions.
/// Thumbnail capture object will be added only if this method is
/// triggered by any thumbnail capture event.
- (void)notifyUpdateForItemWithID:(NSNumber*)itemID
                 captureThumbnail:(BOOL)captureThumbnail
                  finishedCapture:(BOOL)finishedCapture {
  NSMutableDictionary *userInfo =
      [NSMutableDictionary dictionaryWithObject:itemID
                                         forKey:vSpeedDialIdentifierKey];

  if (captureThumbnail) {
    NSNumber *finishedCaptureThumbnail =
        [NSNumber numberWithBool:finishedCapture];
    [userInfo setObject:finishedCaptureThumbnail
                 forKey:vSpeedDialThumbnailRefreshStateKey];
  }

  [[NSNotificationCenter defaultCenter]
       postNotificationName:vSpeedDialPropertyDidChange
                     object:nil
                   userInfo:userInfo];
}

#pragma mark - COLLECTIONVIEW DATA SOURCE
- (NSInteger)collectionView:(UICollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section{
  // Return 1 item when speed dial menu items count is empty to show + button
  // that allows adding folder or item.
  return self.speedDialMenuItems.count ?: 1;
}

- (UICollectionViewCell*)collectionView:(UICollectionView *)collectionView
                           cellForItemAtIndexPath:(NSIndexPath *)indexPath {
  VivaldiSpeedDialContainerCell *cell =
    [collectionView dequeueReusableCellWithReuseIdentifier:cellId
                                            forIndexPath:indexPath];
  NSInteger index = indexPath.row;
  [cell setCurrentPage: index];
  cell.delegate = self;

  if (self.speedDialMenuItems.count <= 0) {
    [cell configureWith:@[]
                 parent:_bookmarkBarItem
          faviconLoader:self.faviconLoader
            layoutStyle:[self currentLayoutStyle]];
  } else {
    VivaldiSpeedDialItem* parent =
      [self.speedDialMenuItems objectAtIndex:index];
    NSArray* childItems = [self.speedDialChildItems objectAtIndex:index];
    [cell configureWith:childItems
                 parent:parent
          faviconLoader:self.faviconLoader
            layoutStyle:[self currentLayoutStyle]];
  }

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

#pragma mark - SPEED DIAL HOME CONSUMER
- (void)refreshContents {
  [self.mediator computeSpeedDialFolders];
}

- (void)refreshNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  NSNumber *bookmarkNodeId = @(bookmarkNode->id());
  [self notifyUpdateForItemWithID:bookmarkNodeId
                 captureThumbnail:NO
                  finishedCapture:NO];
}

- (void)refreshMenuItems:(NSArray*)items {
  [self.navigationController setNavigationBarHidden:items.count == 0
                                           animated:NO];
  if (items.count <= 0) {
    [self.speedDialMenuItems removeAllObjects];
    [self.topScrollMenu removeAllItems];
    return;
  }

  self.speedDialMenuItems = [[NSMutableArray alloc] initWithArray:items];
  [self.topScrollMenu setMenuItemsWithSDFolders:items
                                  selectedIndex:self.selectedMenuItemIndex];
}

- (void)refreshChildItems:(NSArray*)items {
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
  // Navigate to the folder if the selected speed dial is a folder.
  if (item.isFolder) {
    // Hide the top menu before navigating
    if (self.topScrollMenuContainer.alpha == 1) {
      [self.topScrollMenuContainer setAlpha:0];
    }

    VivaldiSpeedDialViewController *controller =
      [VivaldiSpeedDialViewController initWithItem:item
                                            parent:parent
                                         bookmarks:self.bookmarks
                                           browser:self.browser
                                     faviconLoader:self.faviconLoader
                                   backgroundImage:[self getWallpaperImage]];
    controller.delegate = self;
    [self.navigationController pushViewController:controller
                                         animated:YES];
    self.speedDialViewController = controller;
  } else {
    BOOL isOffTheRecord = _browserState->IsOffTheRecord();
    BOOL hasThumbnail = item.thumbnail.length != 0;
    BOOL shouldMigate =
        [self.vivaldiThumbnailService shouldMigrateForSDItem:item];

    BOOL captureThumbnail = (shouldMigate || !hasThumbnail) && !isOffTheRecord;
    if (captureThumbnail) {
      [self captureThumbnailForItem:item
                        isMigrating:shouldMigate
                      shouldReplace:NO];
    }

    // Resign the edit mode of omnibox
    id<OmniboxCommands> omniboxCommandHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), OmniboxCommands);
    [omniboxCommandHandler cancelOmniboxEdit];

    // Go to the website
    UrlLoadParams params = UrlLoadParams::InCurrentTab(item.url);
    params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
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
    std::vector<const bookmarks::BookmarkNode*> editedNodes;
    editedNodes.push_back(item.bookmarkNode);
    bookmark_utils_ios::MoveBookmarks(editedNodes,
                                      self.bookmarks,
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
    std::vector<const bookmarks::BookmarkNode*> nodes;
    nodes.push_back(item.bookmarkNode);
    const BookmarkNode* trashFolder = _bookmarks->trash_node();
    bookmark_utils_ios::MoveBookmarks(nodes,
                                      self.bookmarks,
                                      self.bookmarks,
                                      trashFolder);
    [_vivaldiThumbnailService removeThumbnailForSDItem:item];
  }
}

- (void)didRefreshThumbnailForItem:(VivaldiSpeedDialItem*)item
                            parent:(VivaldiSpeedDialItem*)parent {
  BOOL shouldMigate =
      [self.vivaldiThumbnailService shouldMigrateForSDItem:item];
  [self captureThumbnailForItem:item
                    isMigrating:shouldMigate
                  shouldReplace:YES];
}

- (void)didSelectAddNewSpeedDial:(BOOL)isFolder
                          parent:(VivaldiSpeedDialItem*)parent {
  // When there's no speed dial items, bookmark bar node is passed as the parent
  // Otherwise, pass the parent node of the selected item.
  VivaldiSpeedDialItem* parentItem =
      self.speedDialMenuItems.count == 0 ? _bookmarkBarItem : parent;

  if (isFolder) {
    [self presentBookmarkFolderEditor:nil
                               parent:parentItem
                            isEditing:NO];
  } else {
    [self presentBookmarkURLEditor:nil
                            parent:parentItem
                         isEditing:NO];
  }
}

#pragma mark - VIVALDI_BOOKMARK_ADD_EDIT_CONTROLLER_DELEGATE
- (void)didCreateNewFolder:(const bookmarks::BookmarkNode*)folder {
  // No op here. The collection will be reloaded if anythings changes in the
  // bookmark model.
}

#pragma mark - BOOKMARK MODEL OBSERVER

- (void)bookmarkModelLoaded:(bookmarks::BookmarkModel*)model {
  // If the view hasn't loaded yet, then return early. The eventual call to
  // viewDidLoad will properly initialize the views.  This early return must
  // come *after* the call to setRootNode above.
  if (![self isViewLoaded])
    return;
  [self loadSpeedDialViews];
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  // No-op. Mediator is responsible for refreshing.
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
    didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  // No-op. Mediator is responsible for refreshing.
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
          didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
           fromParent:(const bookmarks::BookmarkNode*)oldParent
             toParent:(const bookmarks::BookmarkNode*)newParent {
  // No-op. Mediator is responsible for refreshing.
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  // No-op. Mediator is responsible for refreshing.
}

- (void)bookmarkModelRemovedAllNodes:(bookmarks::BookmarkModel*)model {
  // No-op. Mediator is responsible for refreshing.
}

@end
