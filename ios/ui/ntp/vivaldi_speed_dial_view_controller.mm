// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_view_controller.h"

#import <UIKit/UIKit.h>

#import "components/direct_match/direct_match_service.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/direct_match/direct_match_service_factory.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_container_view.h"
#import "ios/ui/ntp/vivaldi_speed_dial_home_mediator.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs_helper.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiSpeedDialViewController ()
          <VivaldiSpeedDialContainerDelegate,
          SpeedDialHomeConsumer> {
  direct_match::DirectMatchService* _directMatchService;
}
// The view that holds the speed dial folder children
@property(weak,nonatomic) VivaldiSpeedDialContainerView* speedDialContainerView;
// The background Image for Speed Dial
@property(nonatomic, strong) UIImageView* backgroundImageView;
// Bookmark Model that holds the bookmark data
@property(assign,nonatomic) BookmarkModel* bookmarks;
// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(assign,nonatomic) FaviconLoader* faviconLoader;
// The user's profile model used.
@property(assign,nonatomic) ProfileIOS* profile;
// Current browser
@property(assign,nonatomic) Browser* browser;
// The mediator that provides data for this view controller.
@property(strong,nonatomic) VivaldiSpeedDialHomeMediator* mediator;
// Array to hold the children
@property(strong,nonatomic) NSMutableArray *speedDialChildItems;
// Speed dial folder which is currently presented
@property(strong,nonatomic) VivaldiSpeedDialItem* currentItem;
// Parent of Speed dial folder which is currently presented.
@property(strong,nonatomic) VivaldiSpeedDialItem* parentItem;
@property(strong,nonatomic) UIImage* backgroundImage;

@end


@implementation VivaldiSpeedDialViewController

@synthesize speedDialContainerView = _speedDialContainerView;
@synthesize bookmarks = _bookmarks;
@synthesize browser = _browser;
@synthesize profile = _profile;
@synthesize mediator = _mediator;
@synthesize currentItem = _currentItem;
@synthesize parentItem = _parentItem;
@synthesize faviconLoader = _faviconLoader;
@synthesize speedDialChildItems = _speedDialChildItems;

#pragma mark - INITIALIZERS
+ (instancetype)initWithItem:(VivaldiSpeedDialItem*)item
                      parent:(VivaldiSpeedDialItem*)parent
                   bookmarks:(BookmarkModel*)bookmarks
                     browser:(Browser*)browser
               faviconLoader:(FaviconLoader*)faviconLoader
             backgroundImage:(UIImage*)backgroundImage {
  DCHECK(bookmarks);
  DCHECK(bookmarks->loaded());
  VivaldiSpeedDialViewController* controller =
    [[VivaldiSpeedDialViewController alloc] initWithBookmarks:bookmarks
                                                      browser:browser];
  controller.faviconLoader = faviconLoader;
  controller.currentItem = item;
  controller.parentItem = parent;
  controller.browser = browser;
  controller.title = item.title;
  controller.backgroundImage = backgroundImage;
  return controller;
}

- (instancetype)initWithBookmarks:(BookmarkModel*)bookmarks
                          browser:(Browser*)browser {
  self = [super init];
  if (self) {
    _bookmarks = bookmarks;
    _profile = browser->GetProfile()->GetOriginalProfile();
    _directMatchService =
        direct_match::DirectMatchServiceFactory::GetForProfile(_profile);
    [VivaldiStartPagePrefs setPrefService:_profile->GetPrefs()];
  }
  return self;
}

- (void)dealloc {
  [self removeObservers];
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [self setUpUI];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self startObservingDeviceOrientationChange];
  [self loadSpeedDialViews];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self removeObservers];
}

#pragma mark - PRIVATE
#pragma mark - SET UP UI COMPONENETS
/// Set up all views
- (void)setUpUI {
  self.view.backgroundColor =
    [UIColor colorNamed:vNTPSpeedDialContainerbackgroundColor];
  [self setupSpeedDialBackground];
  [self setupSpeedDialView];
}

-(void)setupSpeedDialBackground {
  UIImageView* backgroundImageView =
      [[UIImageView alloc] initWithImage:_backgroundImage];
  backgroundImageView.contentMode = UIViewContentModeScaleAspectFill;
  backgroundImageView.clipsToBounds = YES;
  self.backgroundImageView = backgroundImageView;
  [self.view addSubview:backgroundImageView];
  [self.backgroundImageView fillSuperview];
}

/// Set up the speed dial view
-(void)setupSpeedDialView {
  // The container view to hold the speed dial view
  UIView* bodyContainerView = [UIView new];
  bodyContainerView.backgroundColor = UIColor.clearColor;
  [self.view addSubview:bodyContainerView];

  [bodyContainerView anchorTop: self.view.safeTopAnchor
                       leading: self.view.safeLeftAnchor
                        bottom: self.view.safeBottomAnchor
                      trailing: self.view.safeRightAnchor];

  VivaldiSpeedDialContainerView* speedDialContainerView =
    [VivaldiSpeedDialContainerView new];
  _speedDialContainerView = speedDialContainerView;
  speedDialContainerView.delegate = self;
  [bodyContainerView addSubview:speedDialContainerView];
  [speedDialContainerView fillSuperview];
}

#pragma mark - PRIVATE

/// Create and start mediator to load the speed dial folder items
- (void)loadSpeedDialViews {
  BOOL loadable = self.bookmarks->loaded() &&
                  self.currentItem;
  if (!loadable)
    return;
  self.mediator = [[VivaldiSpeedDialHomeMediator alloc]
                      initWithProfile:self.profile
                        bookmarkModel:self.bookmarks];
  self.mediator.consumer = self;
  [self.mediator computeSpeedDialChildItems:self.currentItem];
}

/// Set up observer to notify whenever device orientation is changed.
- (void)startObservingDeviceOrientationChange {
  [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
  [[NSNotificationCenter defaultCenter]
     addObserver:self
        selector:@selector(handleDeviceOrientationChange:)
            name:UIDeviceOrientationDidChangeNotification
          object:[UIDevice currentDevice]];
}

/// Device orientation change handler
- (void)handleDeviceOrientationChange:(NSNotification*)note {
  [self.speedDialContainerView reloadLayoutWithStyle:[self currentLayoutStyle]
                                        layoutColumn:[self currentLayoutColumn]];
}

/// Refresh the UI when data source is updated.
- (void)refreshUI {
  [self loadSpeedDialViews];
}

/// Remove all observers set up.
- (void)removeObservers {
  [[NSNotificationCenter defaultCenter]
     removeObserver:self
               name:UIDeviceOrientationDidChangeNotification
             object:nil];
  self.mediator.consumer = nil;
  [self.mediator disconnect];
}

/// Returns current layout style for start page
- (VivaldiStartPageLayoutStyle)currentLayoutStyle {
  return [VivaldiStartPagePrefsHelper getStartPageLayoutStyle];
}

/// Returns current layout column for start page
- (VivaldiStartPageLayoutColumn)currentLayoutColumn {
  return [VivaldiStartPagePrefsHelper getStartPageSpeedDialMaximumColumns];
}

#pragma mark - SPEED DIAL HOME CONSUMER

- (void)bookmarkModelLoaded {
  // No op.
}

- (void)refreshContents {
  BOOL loadable = self.bookmarks->loaded() &&
                  self.currentItem;
  if (!loadable)
    return;
  [self.mediator computeSpeedDialChildItems:self.currentItem];
}

- (void)refreshNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  NSNumber *bookmarkNodeId = @(bookmarkNode->id());
  NSDictionary *userInfo = @{vSpeedDialIdentifierKey:bookmarkNodeId};
  [[NSNotificationCenter defaultCenter]
       postNotificationName:vSpeedDialPropertyDidChange
                     object:nil
                   userInfo:userInfo];
}

- (void)refreshMenuItems:(NSArray*)items
               SDFolders:(NSArray*)SDFolders {
  // No op here since there's no menu in this view.
}

- (void)refreshChildItems:(NSArray*)items
        topSitesAvailable:(BOOL)topSitesAvailable {
  if (!self.currentItem || !self.faviconLoader)
    return;

  BrowserActionFactory* actionFactory =
      [[BrowserActionFactory alloc] initWithBrowser:_browser
              scenario:kMenuScenarioHistogramBookmarkEntry];
  [self.speedDialContainerView configureActionFactory:actionFactory];
  [self.speedDialContainerView configureWith:items
                                      parent:self.currentItem
                               faviconLoader:self.faviconLoader
                          directMatchService:_directMatchService
                                 layoutStyle:[self currentLayoutStyle]
                                layoutColumn:[self currentLayoutColumn]
                                showAddGroup:NO
                           frequentlyVisited:NO
                           topSitesAvailable:NO
                            topToolbarHidden:NO
                           verticalSizeClass:
                                self.view.traitCollection.verticalSizeClass];
}

- (void)setFrequentlyVisitedPagesEnabled:(BOOL)enabled {
  // No op. Handled in base view controller.
}

- (void)setSpeedDialsEnabled:(BOOL)enabled {
  // No op. Handled in base view controller.
}

- (void)setShowCustomizeStartPageButtonEnabled:(BOOL)enabled {
  // No op. Handled in base view controller.
}

- (void)reloadLayout {
  [self.speedDialContainerView reloadLayoutWithStyle:[self currentLayoutStyle]
                                        layoutColumn:[self currentLayoutColumn]];
}

#pragma mark - VIVALDI_SPEED_DIAL_CONTAINER_VIEW_DELEGATE
- (void)didSelectItem:(VivaldiSpeedDialItem*)item
               parent:(VivaldiSpeedDialItem*)parent {
  // Navigate to the folder if the selected speed dial is a folder.
  if (item.isFolder) {
    VivaldiSpeedDialViewController *controller =
      [VivaldiSpeedDialViewController initWithItem:item
                                            parent:parent
                                         bookmarks:self.bookmarks
                                           browser:self.browser
                                     faviconLoader:self.faviconLoader
                                   backgroundImage:self.backgroundImage];
    controller.delegate = self;
    [self.navigationController pushViewController:controller
                                         animated:YES];
  } else {
    // Pass it to delegate to open the URL.
    if (self.delegate)
      [self.delegate didSelectItem:item parent:parent];
  }
}

- (void)didSelectEditItem:(VivaldiSpeedDialItem*)item
                   parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectEditItem:item parent:parent];
}

- (void)didSelectMoveItem:(VivaldiSpeedDialItem*)item
                   parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectMoveItem:item parent:parent];
}

- (void)didMoveItemByDragging:(VivaldiSpeedDialItem*)item
                       parent:(VivaldiSpeedDialItem*)parent
                   toPosition:(NSInteger)position {
  if (self.delegate)
    [self.delegate didMoveItemByDragging:item
                                  parent:parent
                              toPosition:position];
}

- (void)didSelectDeleteItem:(VivaldiSpeedDialItem*)item
                     parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectDeleteItem:item parent:parent];
}

- (void)didRefreshThumbnailForItem:(VivaldiSpeedDialItem*)item
                            parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didRefreshThumbnailForItem:item parent:parent];
}

- (void)didSelectAddNewSpeedDial:(BOOL)isFolder
                          parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectAddNewSpeedDial:isFolder parent:parent];
}

- (void)didSelectAddNewGroupForParent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectAddNewGroupForParent:parent];
}

- (void)didSelectItemToOpenInNewTab:(VivaldiSpeedDialItem*)item
                             parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectItemToOpenInNewTab:item parent:parent];
}

- (void)didSelectItemToOpenInBackgroundTab:(VivaldiSpeedDialItem*)item
                                    parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectItemToOpenInBackgroundTab:item parent:parent];
}

- (void)didSelectItemToOpenInPrivateTab:(VivaldiSpeedDialItem*)item
                                 parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectItemToOpenInPrivateTab:item parent:parent];
}

- (void)didSelectItemToShare:(VivaldiSpeedDialItem*)item
                      parent:(VivaldiSpeedDialItem*)parent
                    fromView:(UIView*)view {
  if (self.delegate)
    [self.delegate didSelectItemToShare:item parent:parent fromView:view];
}

- (void)didTapOnCollectionViewEmptyArea {
  if (self.delegate)
    [self.delegate didTapOnCollectionViewEmptyArea];
}

@end
