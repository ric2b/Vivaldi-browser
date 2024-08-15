// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_base_controller.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_constants.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_coordinator.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_entry_point.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/cells/vivaldi_speed_dial_container_cell.h"
#import "ios/ui/ntp/top_toolbar/top_toolbar_swift.h"
#import "ios/ui/ntp/top_toolbar/vivaldi_ntp_top_toolbar_view_consumer.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_ntp_util.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_shared_state.h"
#import "ios/ui/ntp/vivaldi_speed_dial_sorting_mode.h"
#import "ios/ui/ntp/vivaldi_speed_dial_view_controller.h"
#import "ios/ui/settings/start_page/quick_settings/vivaldi_start_page_quick_settings_coordinator.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs_helper.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"
#import "ios/ui/thumbnail/thumbnail_capturer_swift.h"
#import "ios/ui/thumbnail/vivaldi_thumbnail_service.h"
#import "ios/ui/toolbar/vivaldi_toolbar_constants.h"
#import "prefs/vivaldi_pref_names.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using bookmarks::BookmarkModel;
using l10n_util::GetNSString;

// Namespace
namespace {
// Cell Identifier for the container cell
NSString* cellId = @"cellId";
// Notification Identifier For Background Wallpaper
NSString* vivaldiWallpaperUpdate = @"VivaldiBackgroundWallpaperUpdate";
// Padding for the top scroll menu
UIEdgeInsets topScrollMenuPadding = UIEdgeInsetsMake(0.f, 16.f, 0.f, 0.f);
// Padding for the more action button
UIEdgeInsets moreButtonPadding = UIEdgeInsetsMake(6.f, 0.f, 0.f, 12.f);
// Padding for the more action button when toolbar hidden on iPhone
UIEdgeInsets moreButtonPaddingNoToolbariPhone =
    UIEdgeInsetsMake(15.f, 0.f, 0.f, 12.f);
// Padding for the more action button when toolbar hidden on iPad
// iPhone and iPad nav bar height is different, so we need different padding to
// position the button on screen for toolbar visible and hidden state.
UIEdgeInsets moreButtonPaddingNoToolbariPad =
    UIEdgeInsetsMake(17.f, 0.f, 0.f, 12.f);
// Size for the more button container when new start page is enabled
CGSize moreButtonContainerSize = CGSizeMake(26.f, 14.f);
// Size for the more button container when new start page is disabled
CGSize moreButtonContainerSizeOldToolbar = CGSizeMake(30.f, 30.f);
// Corner radius for more button container
CGFloat moreButtonContainerRadius = 4.0f;
// Opacity for background color for more button container
CGFloat moreButtonContainerOpacity = 0.6f;

// Customize Start Page button
CGFloat customizeButtonShadowOpacity = 0.3f;
CGFloat customizeButtonVerticalPadding = 10.f;
CGFloat customizeButtonHorizontalPadding = 20.f;
CGFloat customizeButtonBottomPadding = 20.f;
CGFloat customizeButtonHeight = 40.f;
}

@interface VivaldiSpeedDialBaseController ()<UICollectionViewDataSource,
                                             UICollectionViewDelegate,
                                        UICollectionViewDelegateFlowLayout,
                                          VivaldiNTPTopToolbarViewConsumer,
                                        VivaldiSpeedDialContainerDelegate,
                                                    SpeedDialHomeConsumer> {
  // Start page settings coordinator.
  VivaldiStartPageQuickSettingsCoordinator* _startPageSettingsCoordinator;
}

// Collection view that holds the children of speed dial folder.
@property (weak, nonatomic) UICollectionView *collectionView;
// Container view for holding the menu items and sort button.
@property(nonatomic, weak) UIView* topScrollMenuContainer;
// Top toolbar menu view provider
@property(nonatomic, strong)
    VivaldiNTPTopToolbarViewProvider* topToolbarViewProvider;
// Top toolbar menu view controller
@property(nonatomic, strong) UIViewController* topToolbarViewController;
// More action button
@property(nonatomic, weak) UIButton* moreButton;
// More action button container
@property(nonatomic, weak) UIView* moreButtonContainer;
// Customize start page button visible at the bottom of the page based on pref
// value.
@property(nonatomic, weak) UIButton* customizeStartPageButton;
// The view controller to present when pushing to new view controller
@property(nonatomic, strong)
  VivaldiSpeedDialViewController* speedDialViewController;
// The background Image for Speed Dial
@property(nonatomic, strong) UIImageView* backgroundImageView;
// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
// Blur view for no results state
@property(weak, nonatomic) UIVisualEffectView *blurEffectView;

@property(nonatomic, assign) FaviconLoader* faviconLoader;
// The Browser in which bookmarks are presented
@property(nonatomic, assign) Browser* browser;
// Bookmarks model that holds all the bookmarks data
@property (assign,nonatomic) LegacyBookmarkModel* bookmarks;
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
// Coodrinator for bookmarks editor
@property(nonatomic, strong)
    VivaldiBookmarksEditorCoordinator* bookmarksEditorCoordinator;

// Array to hold the speed dial menu items to render the toolbar options
@property (strong, nonatomic) NSMutableArray *speedDialMenuItems;
// Array to hold the speed dial folder items to get child of each toolbar option
@property (strong, nonatomic) NSMutableArray *speedDialFolderItems;
// Array to hold the speed dial folders children
@property (strong, nonatomic) NSMutableArray *speedDialChildItems;
// Array to hold the sort button context menu options
@property (strong, nonatomic) NSMutableArray *speedDialSortActions;
// Array to hold ascendeing decending buttons for sort context menu options
@property (strong, nonatomic) NSMutableArray *sortOrderActions;
// Parent to pass for new speed dial/folder item.
@property(nonatomic, strong) VivaldiSpeedDialItem* bookmarkBarItem;
// A boolean to keep track when the scrolling is taking place due to tap in the
// top menu item. There are two types of scroll event. One happens when user
// swipes the collection view left or right by pan/swipe gesture. The other one
// is when taps on the menu item.
@property (assign, nonatomic) BOOL scrollFromMenuTap;
// Keep track whether bookmark model is loaded yet.
// TODO: @prio - See if we need it actually.
@property (assign, nonatomic) BOOL bookmarkModelIsLoaded;
// Height constraint of the top menu container
@property (assign, nonatomic) NSLayoutConstraint* menuContainerHeight;

// Sort button context menu options
@property(nonatomic, strong) UIAction* manualSortAction;
@property(nonatomic, strong) UIAction* titleSortAction;
@property(nonatomic, strong) UIAction* addressSortAction;
@property(nonatomic, strong) UIAction* nicknameSortAction;
@property(nonatomic, strong) UIAction* descriptionSortAction;
@property(nonatomic, strong) UIAction* dateSortAction;
@property(nonatomic, strong) UIAction* kindSortAction;

// The current sort order Action
@property(nonatomic, strong) UIAction* ascendingSortAction;
@property(nonatomic, strong) UIAction* descendingSortAction;

@end


@implementation VivaldiSpeedDialBaseController

@synthesize browserState = _browserState;
@synthesize speedDialViewController = _speedDialViewController;
@synthesize speedDialSharedState = _speedDialSharedState;
@synthesize collectionView = _collectionView;
@synthesize topScrollMenuContainer = _topScrollMenuContainer;
@synthesize topToolbarViewProvider = _topToolbarViewProvider;
@synthesize moreButton = _moreButton;
@synthesize moreButtonContainer = _moreButtonContainer;
@synthesize manualSortAction = _manualSortAction;
@synthesize titleSortAction = _titleSortAction;
@synthesize addressSortAction = _addressSortAction;
@synthesize nicknameSortAction = _nicknameSortAction;
@synthesize descriptionSortAction = _descriptionSortAction;
@synthesize dateSortAction = _dateSortAction;
@synthesize kindSortAction = _kindSortAction;
@synthesize ascendingSortAction = _ascendingSortAction;
@synthesize descendingSortAction = _descendingSortAction;
@synthesize backgroundImageView = _backgroundImageView;
@synthesize bookmarkBarItem = _bookmarkBarItem;

#pragma mark - INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser
                  bookmarkModel:(LegacyBookmarkModel*)bookmarkModel {
  self = [super init];
  if (self) {
    _browser = browser;
    _bookmarks = bookmarkModel;
    _browserState =
        _browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(_browserState);
    _vivaldiThumbnailService = [VivaldiThumbnailService new];
    _thumbnailCapturer = [[VivaldiThumbnailCapturer alloc] init];
    [VivaldiStartPagePrefs setPrefService:self.browserState->GetPrefs()];
    _bookmarkModelIsLoaded = NO;
  }
  return self;
}

- (void)invalidate {
  _bookmarkBarItem = nil;
  _thumbnailCapturer = nil;
  _speedDialViewController.delegate = nil;
  _bookmarksEditorCoordinator = nil;
  _topToolbarViewProvider.consumer = nil;
  _topToolbarViewProvider = nil;
  _topToolbarViewController = nil;
  _vivaldiThumbnailService = nullptr;
  _startPageSettingsCoordinator = nullptr;
  [[NSNotificationCenter defaultCenter] removeObserver: self
                                                  name: vivaldiWallpaperUpdate
                                                object: nil];
}

- (void)dealloc {
  [self invalidate];
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [self setUpUI];
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
      [strongSelf setUpMoreButtonProperties];
      [strongSelf.collectionView reloadData];
      [strongSelf scrollViewDidEndDecelerating:strongSelf.collectionView];
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
  if (!self.speedDialPositionModifiedFlag)
    return;
  [self.delegate computeSpeedDialChildItems:nil];
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

- (void)viewWillTransitionToSize:(CGSize)size
    withTransitionCoordinator:
      (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  auto completion =
  ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    [self.collectionView.collectionViewLayout invalidateLayout];
    [self.collectionView reloadData];
    [self scrollToItemWithIndex:self.selectedMenuItemIndex
                       animated:YES];
  };
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
  } completion:completion];
}

#pragma mark - PRIVATE
#pragma mark - SET UP UI COMPONENTS

/// Set up the UI Components
- (void)setUpUI {
  self.view.backgroundColor =
    [UIColor colorNamed:vNTPSpeedDialContainerbackgroundColor];
  [self setupSpeedDialBackground];
  [self setUpBlurView];
  [self setUpHeaderView];
  [self setupSpeedDialView];
  [self setUpCustomizeStartPageButton];
}

- (void)setupSpeedDialBackground {
  UIImageView* backgroundImageView =
      [[UIImageView alloc] initWithImage:[self getWallpaperImage]];
  backgroundImageView.contentMode = UIViewContentModeScaleAspectFill;
  backgroundImageView.clipsToBounds = YES;
  self.backgroundImageView = backgroundImageView;
  [self.view addSubview:backgroundImageView];
  [self.backgroundImageView fillSuperview];
}

- (void)setUpBlurView {
  if (!UIAccessibilityIsReduceTransparencyEnabled()) {
    UIBlurEffect *blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    UIVisualEffectView *blurEffectView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    _blurEffectView = blurEffectView;
    blurEffectView.frame = self.view.bounds;
    blurEffectView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:blurEffectView];
    blurEffectView.alpha = 0;
  }
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

  VivaldiNTPTopToolbarViewProvider* toolbarViewProvider =
      [VivaldiNTPTopToolbarViewProvider new];
  _topToolbarViewProvider = toolbarViewProvider;

  UIViewController* toolbarViewController =
      [VivaldiNTPTopToolbarViewProvider makeViewController];
  toolbarViewController.view.backgroundColor = UIColor.clearColor;
  _topToolbarViewController = toolbarViewController;
  toolbarViewProvider.consumer = self;
  [self.topScrollMenuContainer addSubview:toolbarViewController.view];
  [toolbarViewController.view anchorTop:self.topScrollMenuContainer.topAnchor
                              leading:self.topScrollMenuContainer.leadingAnchor
                               bottom:self.topScrollMenuContainer.bottomAnchor
                             trailing:nil
                              padding:topScrollMenuPadding];

  // Add the top scroll menu menu container as subview on navigation bar
  [navBar addSubview:topScrollMenuContainer];
  [topScrollMenuContainer anchorTop: navBar.topAnchor
                            leading: navBar.safeLeftAnchor
                             bottom: navBar.bottomAnchor
                           trailing: navBar.safeRightAnchor];

  // More button
  UIButton *moreButton = [UIButton new];
  _moreButton = moreButton;

  UIImage* moreButtonIcon = IsNewStartPageIsEnabled() ?
      [UIImage imageNamed:vNTPToolbarMoreIcon] :
      [UIImage imageNamed:vNTPToolbarSortIcon];
  [moreButton setImage:moreButtonIcon
              forState:UIControlStateNormal];
  [moreButton setImage:moreButtonIcon
              forState:UIControlStateHighlighted];
  moreButton.showsMenuAsPrimaryAction = YES;
  moreButton.menu = [self contextMenuForToolbarMoreButton];
  moreButton.pointerInteractionEnabled = YES;
  // This button's background color is configured whenever the cell is
  // reused. The pointer style provider used here dynamically provides the
  // appropriate style based on the background color at runtime.
  moreButton.pointerStyleProvider =
      CreateOpaqueOrTransparentButtonPointerStyleProvider();

  UIView* moreButtonContainer = [UIView new];
  moreButtonContainer.layer.cornerRadius = moreButtonContainerRadius;
  moreButtonContainer.clipsToBounds = YES;
  _moreButtonContainer = moreButtonContainer;

  [self setUpMoreButtonProperties];
}

- (void)setUpMoreButtonProperties {
  BOOL shouldHideToolbar = IsNewStartPageIsEnabled() && ![self showSpeedDials];
  // Hide toolbar
  [self.navigationController setNavigationBarHidden:shouldHideToolbar
                                           animated:NO];
  // Redraw the more button
  [_moreButton removeFromSuperview];
  [_moreButtonContainer removeFromSuperview];

  if (shouldHideToolbar) {
    UIEdgeInsets paddingNoToolbar = [VivaldiGlobalHelpers isDeviceTablet] ?
        moreButtonPaddingNoToolbariPad : moreButtonPaddingNoToolbariPhone;

    [self.view addSubview: _moreButtonContainer];
    [_moreButtonContainer anchorTop: self.view.safeTopAnchor
                            leading: nil
                             bottom: nil
                           trailing: self.view.safeRightAnchor
                            padding: paddingNoToolbar
                               size: moreButtonContainerSize];

    [_moreButtonContainer addSubview: _moreButton];
    [_moreButton fillSuperview];
  } else {
    CGSize moreContainerSize = IsNewStartPageIsEnabled() ?
        moreButtonContainerSize : moreButtonContainerSizeOldToolbar;
    [self.topScrollMenuContainer addSubview: _moreButtonContainer];
    [_moreButtonContainer anchorTop:nil
                      leading:self.topToolbarViewController.view.trailingAnchor
                       bottom:nil
                     trailing:self.topScrollMenuContainer.trailingAnchor
                      padding:moreButtonPadding
                         size:moreContainerSize];
    [_moreButtonContainer centerYInSuperview];

    [_moreButtonContainer addSubview: _moreButton];
    [_moreButton fillSuperview];
  }

  // More button tint color
  UIColor* moreTintColor = [UIColor colorNamed:vToolbarButtonColor];
  if (IsNewStartPageIsEnabled()) {
    if ([self getWallpaperImage] && shouldHideToolbar) {
      BOOL shouldUseDarkTint =
        [VivaldiGlobalHelpers
            shouldUseDarkTextForImage:[self getWallpaperImage]];
      moreTintColor = shouldUseDarkTint ?
          [UIColor colorNamed:vNTPToolbarMoreLightTintColor] :
          [UIColor colorNamed:vNTPToolbarMoreDarkTintColor];
      _moreButtonContainer.backgroundColor =
          shouldUseDarkTint ?
              [[UIColor blackColor]
                  colorWithAlphaComponent:moreButtonContainerOpacity] :
              [[UIColor whiteColor]
                  colorWithAlphaComponent:moreButtonContainerOpacity];
      _moreButton.tintColor = moreTintColor;
    } else {
      _moreButtonContainer.backgroundColor = moreTintColor;
      _moreButton.tintColor = UIColor.systemBackgroundColor;
    }
  } else {
    _moreButtonContainer.backgroundColor = UIColor.clearColor;
    _moreButton.tintColor = moreTintColor;
  }
}

/// Set up the speed dial view
-(void)setupSpeedDialView {
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

  [self.view addSubview:_collectionView];
  [_collectionView anchorTop:self.view.safeTopAnchor
                     leading:self.view.safeLeftAnchor
                      bottom:self.view.bottomAnchor
                    trailing:self.view.safeRightAnchor];

  self.collectionView.hidden =
      IsNewStartPageIsEnabled() && ![self showSpeedDials];
}

- (void)setUpCustomizeStartPageButton {
  // Customize Start page button
  UIButton *customizeStartPageButton = [UIButton new];
  _customizeStartPageButton = customizeStartPageButton;
  [customizeStartPageButton
       setTitle:GetNSString(IDS_IOS_START_PAGE_CUSTOMIZE_TITLE)
       forState:UIControlStateNormal];
  customizeStartPageButton.pointerInteractionEnabled = YES;
  customizeStartPageButton.pointerStyleProvider =
      CreateOpaqueOrTransparentButtonPointerStyleProvider();

  customizeStartPageButton.layer.shadowColor =
      [[UIColor blackColor]
          colorWithAlphaComponent:customizeButtonShadowOpacity].CGColor;
  customizeStartPageButton.layer.shadowOpacity = vSpeedDialItemShadowOpacity;
  customizeStartPageButton.layer.shadowRadius = vSpeedDialItemShadowRadius;
  customizeStartPageButton.layer.shadowOffset = vSpeedDialItemShadowOffset;

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      customizeButtonVerticalPadding, customizeButtonHorizontalPadding,
      customizeButtonVerticalPadding, customizeButtonHorizontalPadding);
  buttonConfiguration.baseForegroundColor = [UIColor labelColor];
  buttonConfiguration.background.backgroundColor =
      [UIColor colorNamed:vNTPSpeedDialCellBackgroundColor];
  buttonConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  customizeStartPageButton.configuration = buttonConfiguration;
  [customizeStartPageButton
       addTarget:self
          action:@selector(handleCustomizeStartPageButtonTap)
       forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:customizeStartPageButton];

  [customizeStartPageButton centerXInSuperview];
  [NSLayoutConstraint activateConstraints:@[
    [customizeStartPageButton.bottomAnchor
         constraintGreaterThanOrEqualToAnchor:self.view.bottomAnchor
         constant:-customizeButtonBottomPadding],
    [customizeStartPageButton.heightAnchor
        constraintEqualToConstant:customizeButtonHeight]
  ]];

  BOOL hideButton = !IsNewStartPageIsEnabled() ||
      (!ShouldShowCustomizeStartPageButton() &&
          ![self showStartPageCustomizeButton]);
  customizeStartPageButton.hidden = hideButton;
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

// Returns the context menu actions for toolbar more button action
- (UIMenu*)contextMenuForToolbarMoreButton {
  // Add group action button
  NSString* addGroupActionTitle =
      GetNSString(IDS_IOS_START_PAGE_ADD_GROUP_TITLE);
  UIAction* addGroupAction = [UIAction actionWithTitle:addGroupActionTitle
                                                 image:nil
                                            identifier:nil
                                               handler:^(__kindof UIAction*_Nonnull
                                                         action) {
    [self presentBookmarkEditorWithItem:nil
                                 parent:self.bookmarkBarItem
                             entryPoint:VivaldiBookmarksEditorEntryPointGroup
                              isEditing:NO];
  }];

  // Customize start page action button
  NSString* customizePageActionTitle =
      GetNSString(IDS_IOS_START_PAGE_CUSTOMIZE_TITLE);
  UIAction* customizePageAction =
      [UIAction actionWithTitle:customizePageActionTitle
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull action) {
    [self showStartPageSettings];
  }];

  // Manual sorting action button
  NSString* manualSortTitle = GetNSString(IDS_IOS_SORT_MANUAL);
  UIAction* manualSortAction = [UIAction actionWithTitle:manualSortTitle
                                              image:nil
                                              identifier:nil
                                            handler:^(__kindof UIAction*_Nonnull
                                                      action) {
    [self refreshSortingMode:SpeedDialSortingManual];
  }];
  self.manualSortAction = manualSortAction;

  // Sort by title action button
  NSString* titleSortTitle = GetNSString(IDS_IOS_SORT_BY_TITLE);
  UIAction* titleSortAction = [UIAction actionWithTitle:titleSortTitle
                                              image:nil
                                         identifier:nil
                                            handler:^(__kindof UIAction*_Nonnull
                                                      action) {
    [self refreshSortingMode:SpeedDialSortingByTitle];
  }];
  self.titleSortAction = titleSortAction;

  // Sort by address action button
  NSString* addressSortTitle = GetNSString(IDS_IOS_SORT_BY_ADDRESS);
  UIAction* addressSortAction = [UIAction actionWithTitle:addressSortTitle
                                              image:nil
                                         identifier:nil
                                            handler:^(__kindof UIAction*_Nonnull
                                                      action) {
    [self refreshSortingMode:SpeedDialSortingByAddress];
  }];
  self.addressSortAction = addressSortAction;

  // Sort by nickname action button
  NSString* nicknameSortTitle = GetNSString(IDS_IOS_SORT_BY_NICKNAME);
  UIAction* nicknameSortAction = [UIAction actionWithTitle:nicknameSortTitle
                                              image:nil
                                         identifier:nil
                                            handler:^(__kindof UIAction*_Nonnull
                                                      action) {
    [self refreshSortingMode:SpeedDialSortingByNickname];
  }];
  self.nicknameSortAction = nicknameSortAction;

  // Sort by description action button
  NSString* descriptionSortTitle = GetNSString(IDS_IOS_SORT_BY_DESCRIPTION);
  UIAction* descriptionSortAction =
    [UIAction actionWithTitle:descriptionSortTitle
                        image:nil
                   identifier:nil
                      handler:^(__kindof UIAction*_Nonnull action) {
    [self refreshSortingMode:SpeedDialSortingByDescription];
  }];
  self.descriptionSortAction = descriptionSortAction;

  // Sort by date created action button
  NSString* dateSortTitle = GetNSString(IDS_IOS_SORT_BY_DATE);
  UIAction* dateSortAction = [UIAction actionWithTitle:dateSortTitle
                                              image:nil
                                         identifier:nil
                                            handler:^(__kindof UIAction*_Nonnull
                                                      action) {
    [self refreshSortingMode:SpeedDialSortingByDate];
  }];
  self.dateSortAction = dateSortAction;

  // Sort by kind action button
  NSString* kindSortTitle = GetNSString(IDS_IOS_SORT_BY_KIND);
  UIAction* kindSortAction = [UIAction actionWithTitle:kindSortTitle
                                                 image:nil
                                            identifier:nil
                                               handler:^(__kindof UIAction*_Nonnull
                                                         action) {
    [self refreshSortingMode:SpeedDialSortingByKind];
  }];
  self.kindSortAction = kindSortAction;

  _speedDialSortActions = [[NSMutableArray alloc] initWithArray:@[
      manualSortAction, titleSortAction, addressSortAction,
      nicknameSortAction, descriptionSortAction, dateSortAction, kindSortAction
  ]];

  [self setSortingStateOnContextMenuOption];

  // Ascending sort action button
  UIAction* ascendingSortAction =
      [UIAction actionWithTitle:GetNSString(IDS_IOS_NOTE_ASCENDING_SORT_ORDER)
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                  action) {
        [self refreshSortOrder:SpeedDialSortingOrderAscending];
      }];
  self.ascendingSortAction = ascendingSortAction;

  // Descending sort action button
  UIAction* descendingSortAction =
      [UIAction actionWithTitle:GetNSString(IDS_IOS_NOTE_DESCENDING_SORT_ORDER)
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                  action) {
        [self refreshSortOrder:SpeedDialSortingOrderDescending];
      }];
  self.descendingSortAction = descendingSortAction;

  if (!self.isSortedByManual) {
    _sortOrderActions = [[NSMutableArray alloc] initWithArray:@[
      ascendingSortAction, descendingSortAction]
    ];
  } else {
    _sortOrderActions = nil;
  }
  [self updateSortOrderStateOnContextMenuOption];

  UIMenu* sortingActionsMenu =
      [UIMenu menuWithTitle:@""
                      image:nil
                 identifier:nil
                    options:UIMenuOptionsDisplayInline
                   children:_speedDialSortActions];

  UIMenu* sortingOrderMenu =
      [UIMenu menuWithTitle:@""
                      image:nil
                 identifier:nil
                    options:UIMenuOptionsDisplayInline
                   children:_sortOrderActions];

  UIMenuOptions options = UIMenuOptionsDisplayInline;
  if (@available(iOS 17.0, *)) {
    options = UIMenuOptionsDisplayAsPalette;
  }

  if (IsNewStartPageIsEnabled()) {
    UIMenu* sortingMenu =
        [UIMenu menuWithTitle:GetNSString(IDS_IOS_START_PAGE_SORTING_TITLE)
                        image:nil
                   identifier:nil
                      options:options
                     children:@[sortingActionsMenu, sortingOrderMenu]];

    UIMenu* menu = [UIMenu menuWithTitle:@""
                                children:@[addGroupAction,
                                           customizePageAction,
                                           sortingMenu]];
    return menu;
  } else {
    UIMenu* sortingMenu =
        [UIMenu menuWithTitle:@""
                        image:nil
                   identifier:nil
                      options:UIMenuOptionsDisplayInline
                     children:@[sortingActionsMenu, sortingOrderMenu]];
    return sortingMenu;
  }

}

#pragma mark - PRIVATE METHODS

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
    case SpeedDialSortingByKind:
      [self updateSortActionButtonState:self.kindSortAction];
      break;
  }
}

/// Returns YES if the current sorting mode is manual
- (BOOL)isSortedByManual {
  return self.currentSortingMode == SpeedDialSortingManual;
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

// Updates the state on the context menu actions for sorting order
// by unchecking the all apart from the selected one.
- (void)updateSortOrderStateOnContextMenuOption {
  if (self.currentSortingOrder == SpeedDialSortingOrderAscending) {
    [self updateSortOrderActionButtonState:self.ascendingSortAction];
  } else {
    [self updateSortOrderActionButtonState:self.descendingSortAction];
  }
}

/// Set sorting order on the prefs
- (void)setSortingOrder:(SpeedDialSortingOrder)order {
  [VivaldiStartPagePrefs setSDSortingOrder:order];
}

/// Returns current sorting order
- (SpeedDialSortingOrder)currentSortingOrder {
  return [VivaldiStartPagePrefs getSDSortingOrder];
}

/// Returns whether show speed dials is enabled
- (BOOL)showSpeedDials {
  return [VivaldiStartPagePrefs showSpeedDials];
}

/// Returns whether show start page customize button is enabled
- (BOOL)showStartPageCustomizeButton {
  return [VivaldiStartPagePrefs showStartPageCustomizeButton];
}

- (void)updateSortOrderActionButtonState:(UIAction*)settable {
  for (UIAction* action in self.sortOrderActions) {
    if (action == settable) {
      [action setState:UIMenuElementStateOn];
    } else {
      [action setState:UIMenuElementStateOff];
    }
  }
}

/// Refresh the context menu and ping the mediator to prepare and send modified
/// items based on sorting mode and order.
- (void)computeSpeedDialItems {
  // Refresh the context menu options after the selection.
  [self resetMoreButtonContextMenuOptions];

  if (self.currentSortingMode == SpeedDialSortingManual) {
    // For manual mode ping the mediator to compute and return the child items
    // as it is without any sorting algo applied by us.
    [self.delegate computeSpeedDialChildItems:nil];
  } else {
    // Ping the mediator to sort items and notify the consumers.
    [self.delegate computeSortedItems:self.speedDialChildItems
                               byMode:self.currentSortingMode];
  }
}

- (void)refreshSortingMode:(SpeedDialSortingMode)mode {
  if (self.currentSortingMode == mode)
    return;
  [self setCurrentSortingMode:mode];
  [self computeSpeedDialItems];
}

- (void)refreshSortOrder:(SpeedDialSortingOrder)order {
  if (self.currentSortingOrder == order)
    return;
  [self setSortingOrder:order];
  [self computeSpeedDialItems];
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

/// Returns current layout column for start page
- (VivaldiStartPageLayoutColumn)currentLayoutColumn {
  return [VivaldiStartPagePrefsHelper getStartPageSpeedDialMaximumColumns];
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

/// Refresh more button context menu options
- (void)resetMoreButtonContextMenuOptions {
  self.moreButton.menu = [self contextMenuForToolbarMoreButton];
}

/// Scroll to the selected page in the collection view.
- (void)scrollToItemWithIndex:(NSInteger)index
                     animated:(BOOL)animated {
  BOOL scrollable = self.speedDialMenuItems.count > 0 &&
                    index < long(self.speedDialMenuItems.count) &&
                    self.speedDialChildItems.count > 0;
  if (!scrollable)
    return;

  NSIndexPath *indexPath = [NSIndexPath indexPathForRow:index
                                              inSection:0];
  [self.collectionView
      scrollToItemAtIndexPath:indexPath
             atScrollPosition:UICollectionViewScrollPositionCenteredHorizontally
                     animated:animated];
}

- (void)showStartPageSettings {
  _startPageSettingsCoordinator =
      [[VivaldiStartPageQuickSettingsCoordinator alloc]
            initWithBaseNavigationController:self.navigationController
                                     browser:_browser];
  [_startPageSettingsCoordinator start];
}

- (void)presentBookmarkEditorWithItem:(VivaldiSpeedDialItem*)item
                            parent:(VivaldiSpeedDialItem*)parent
                        entryPoint:(VivaldiBookmarksEditorEntryPoint)entryPoint
                         isEditing:(BOOL)isEditing {

  VivaldiBookmarksEditorCoordinator* bookmarksEditorCoordinator =
      [[VivaldiBookmarksEditorCoordinator alloc]
               initWithBaseViewController:self
                                  browser:_browser
                                     item:item
                                   parent:parent
                               entryPoint:entryPoint
                                isEditing:isEditing
                             allowsCancel:YES];
  _bookmarksEditorCoordinator = bookmarksEditorCoordinator;
  _bookmarksEditorCoordinator.allowsNewFolders = YES;
  [bookmarksEditorCoordinator start];
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

- (void)setTopToolbarAndPagesHidden:(BOOL)hidden {
  self.collectionView.hidden = IsNewStartPageIsEnabled() && hidden;
  [self setUpMoreButtonProperties];
}

- (void)handleCustomizeStartPageButtonTap {
  if (ShouldShowCustomizeStartPageButton()) {
    SetCustomizeStartPageButtonShown();
  }
  [self showStartPageSettings];
  _customizeStartPageButton.hidden = !IsNewStartPageIsEnabled() ||
      (![self showStartPageCustomizeButton] &&
       !ShouldShowCustomizeStartPageButton());
}

#pragma mark - COLLECTIONVIEW DATA SOURCE
- (NSInteger)collectionView:(UICollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section{
  if (IsNewStartPageIsEnabled()) {
    // Return 1 extra item alongside SD menu items to show the
    // Add new group page.
    return self.speedDialMenuItems.count + 1;
  } else {
    // Return 1 item when speed dial menu items count is empty to show + button
    // that allows adding folder or item.
    return self.speedDialMenuItems.count ?: 1;
  }
}

- (UICollectionViewCell*)collectionView:(UICollectionView *)collectionView
                           cellForItemAtIndexPath:(NSIndexPath *)indexPath {
  VivaldiSpeedDialContainerCell *cell =
    [collectionView dequeueReusableCellWithReuseIdentifier:cellId
                                            forIndexPath:indexPath];
  NSInteger index = indexPath.row;
  cell.delegate = self;

  if (self.speedDialFolderItems.count <= 0 ||
      index == long(self.speedDialFolderItems.count)) {
    [cell configureWith:@[]
                 parent:_bookmarkBarItem
          faviconLoader:self.faviconLoader
            layoutStyle:[self currentLayoutStyle]
           layoutColumn:[self currentLayoutColumn]
           showAddGroup:IsNewStartPageIsEnabled()
     verticalSizeClass:self.view.traitCollection.verticalSizeClass];
  } else {
    [cell setCurrentPage: index];
    VivaldiSpeedDialItem* parent =
      [self.speedDialFolderItems objectAtIndex:index];
    NSArray* childItems = [self.speedDialChildItems objectAtIndex:index];
    [cell configureWith:childItems
                 parent:parent
          faviconLoader:self.faviconLoader
            layoutStyle:[self currentLayoutStyle]
           layoutColumn:[self currentLayoutColumn]
           showAddGroup:NO
      verticalSizeClass:self.view.traitCollection.verticalSizeClass];
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

  if (currentIndexPath &&
      currentIndexPath.row < long(self.speedDialMenuItems.count)) {
    [self.topToolbarViewProvider selectItemWithIndex:currentIndexPath.row];
    [self setSelectedMenuItemIndex:currentIndexPath.row];
  }

  if (IsNewStartPageIsEnabled()) {
    // TODO: @prio- Cleanup this
    if (currentIndexPath &&
        currentIndexPath.row == long(self.speedDialMenuItems.count)) {
      [self.topToolbarViewProvider removeSelection];
      [UIView animateWithDuration:0.3 animations:^{
        self.customizeStartPageButton.alpha = 0;
        if ([self getWallpaperImage]) {
          self.blurEffectView.alpha = 1;
        } else {
          self.blurEffectView.alpha = 0;
        }
      }];
    } else {
      [UIView animateWithDuration:0.3 animations:^{
        self.customizeStartPageButton.alpha = 1;
        self.blurEffectView.alpha = 0;
      }];
    }
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

#pragma mark - SpeedDialHomeConsumer
- (void)bookmarkModelLoaded {
  // Parent bookmark bar node item
  VivaldiSpeedDialItem* bookmarkBarItem =
      [[VivaldiSpeedDialItem alloc]
          initWithBookmark:self.bookmarks->bookmark_bar_node()];
  _bookmarkBarItem = bookmarkBarItem;

  _bookmarkModelIsLoaded = YES;
}

- (void)refreshContents {
  [self.delegate computeSpeedDialFolders];
}

- (void)refreshNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  NSNumber *bookmarkNodeId = @(bookmarkNode->id());
  [self notifyUpdateForItemWithID:bookmarkNodeId
                 captureThumbnail:NO
                  finishedCapture:NO];
}

- (void)refreshMenuItems:(NSArray*)items
               SDFolders:(NSArray*)SDFolders {
  [self.navigationController setNavigationBarHidden:items.count == 0
                                           animated:NO];
  if (items.count <= 0) {
    [self.speedDialMenuItems removeAllObjects];
    [self.speedDialFolderItems removeAllObjects];
    [self.topToolbarViewProvider removeAllItems];
    return;
  }

  self.speedDialMenuItems = [[NSMutableArray alloc] initWithArray:items];
  self.speedDialFolderItems = [[NSMutableArray alloc] initWithArray:SDFolders];
  [self.topToolbarViewProvider
      updateToolbarWithItems:items
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
    [self scrollToItemWithIndex:self.selectedMenuItemIndex animated:NO];
    [CATransaction commit];
  }];
}

- (void)setSpeedDialsEnabled:(BOOL)enabled {
  [self setTopToolbarAndPagesHidden:!enabled];
}

- (void)setShowCustomizeStartPageButtonEnabled:(BOOL)enabled {
  _customizeStartPageButton.hidden = !IsNewStartPageIsEnabled() ||
      (!enabled && !ShouldShowCustomizeStartPageButton());
}

- (void)reloadLayout {
  [self.collectionView reloadData];
}

#pragma mark - VivaldiNTPTopToolbarViewConsumer
- (void)didSelectItemWithIndex:(NSInteger)index {
  self.scrollFromMenuTap = YES;
  [self scrollToItemWithIndex:index animated:YES];
  [self setSelectedMenuItemIndex:index];
  [self.collectionView reloadData];

  if (IsNewStartPageIsEnabled()) {
    // TODO: @prio- Cleanup this
    [UIView animateWithDuration:0.3 animations:^{
      self.customizeStartPageButton.alpha = 1;
      self.blurEffectView.alpha = 0;
    }];
  }
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
  VivaldiBookmarksEditorEntryPoint entryPoint = item.isFolder ?
      VivaldiBookmarksEditorEntryPointFolder :
      VivaldiBookmarksEditorEntryPointSpeedDial;

  [self presentBookmarkEditorWithItem:item
                               parent:parent
                           entryPoint:entryPoint
                            isEditing:YES];
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
  [self resetMoreButtonContextMenuOptions];

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
  VivaldiBookmarksEditorEntryPoint entryPoint = isFolder ?
      VivaldiBookmarksEditorEntryPointFolder :
      VivaldiBookmarksEditorEntryPointSpeedDial;

  [self presentBookmarkEditorWithItem:nil
                               parent:parentItem
                           entryPoint:entryPoint
                            isEditing:NO];
}

- (void)didSelectAddNewGroupForParent:(VivaldiSpeedDialItem*)parent {
  [self presentBookmarkEditorWithItem:nil
                               parent:parent
                           entryPoint:VivaldiBookmarksEditorEntryPointGroup
                            isEditing:NO];
}

@end
