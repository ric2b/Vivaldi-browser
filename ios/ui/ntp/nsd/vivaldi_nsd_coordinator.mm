// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/nsd/vivaldi_nsd_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/ui/ntp/nsd/nsd_swift.h"
#import "ios/ui/ntp/nsd/vivaldi_nsd_mediator.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;
using l10n_util::GetNSStringF;

using bookmarks::BookmarkNode;

@interface VivaldiNSDCoordinator()<BookmarksFolderChooserCoordinatorDelegate> {
  // The browser where the editor is being displayed.
  Browser* _browser;
  // The folder chooser coordinator.
  BookmarksFolderChooserCoordinator* _folderChooserCoordinator;
  // The navigation controller that is being presented. The bookmark editor view
  // controller is the child of this navigation controller.
  UINavigationController* _navigationController;
}
// View provider for the editor
@property(nonatomic, strong) VivaldiNSDViewProvider* viewProvider;
@property(nonatomic, strong) VivaldiNSDMediator* mediator;
// The location where new item will be added. This can not be nil.
@property(nonatomic,strong) VivaldiSpeedDialItem* folderItem;

@end

@implementation VivaldiNSDCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
  (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                          parent:(VivaldiSpeedDialItem*)parent {
  self = [self initWithBaseViewController:navigationController
                                  browser:browser
                                   parent:parent];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    parent:(VivaldiSpeedDialItem*)parent {

  self = [super initWithBaseViewController:viewController browser:browser];

  if (self) {
    _browser = browser;
    _folderItem = parent;
  }

  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self setupViewProvider];
  [self observeTapEvents];
  [self setupViewController];
  [self setupMediator];
  [self configureNavigationBarItems];
  [self presentViewController];
}

- (void)stop {
  [super stop];
  self.viewProvider = nil;
  self.folderItem = nil;
  [self stopFolderChooserCordinator];
  [self.mediator disconnect];
  self.mediator = nil;
}

#pragma mark - Actions
- (void)handleCancelButtonTap {
  [self dismiss];
}

- (void)handleDoneButtonTap {
  if (self.folderItem != nil &&
      self.folderItem.bookmarkNode != nil &&
      [[self.viewProvider urlString] length] > 0) {
    [_mediator saveItemWithTitle:[self.viewProvider titleString]
                             url:[self.viewProvider urlString]];
  } else {
    [self dismiss];
  }
}

#pragma mark - Private

- (void)setupViewProvider {
  self.viewProvider = [[VivaldiNSDViewProvider alloc] init];
}

- (void)observeTapEvents {
  __weak __typeof(self) weakSelf = self;

  [self.viewProvider
      observeKeyboardReturnTapEvent:^ {
    [weakSelf handleDoneButtonTap];
  }];

  [self.viewProvider observeChangeLocationTapEvent:^ {
    [weakSelf handleFolderSelectionEvent];
  }];

  [self.viewProvider
      observeCategoryDetailsNavigateEvent:^
        (VivaldiNSDDirectMatchCategory *category) {
    [weakSelf handleCategoryTap:category];
  }];

  [self.viewProvider observeViewDismissalEvent:^ {
    [weakSelf dismiss];
  }];
}

- (void)setupViewController {
  UIViewController *controller = [self.viewProvider makeViewController];

  NSString* type = GetNSString(IDS_IOS_ITEM_TYPE_SPEED_DIAL);
  NSString* title =
      GetNSStringF(IDS_IOS_NEW_ITEM_WITH_TYPE, base::SysNSStringToUTF16(type));
  controller.title = title;
  controller.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  controller.modalPresentationStyle = UIModalPresentationPageSheet;
  _navigationController =
      [[UINavigationController alloc] initWithRootViewController:controller];
}

- (void)configureNavigationBarItems {

  UIBarButtonItem *doneItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                               target:self
                               action:@selector(handleDoneButtonTap)];
  _navigationController.topViewController
        .navigationItem.rightBarButtonItem = doneItem;

  UIBarButtonItem *cancelItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                               target:self
                               action:@selector(handleCancelButtonTap)];
  _navigationController.topViewController
      .navigationItem.leftBarButtonItem = cancelItem;
}

- (void)presentViewController {
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)setupMediator {
  ProfileIOS *profile =
      self.browser->GetProfile()->GetOriginalProfile();
  BookmarkModel *bookmarkModel =
      ios::BookmarkModelFactory::GetForProfile(profile);
  self.mediator =
    [[VivaldiNSDMediator alloc] initWithBookmarkModel:bookmarkModel
                                              profile:profile
                                   locationFolderItem:self.folderItem];
  self.mediator.consumer = self.viewProvider;
  self.viewProvider.delegate = self.mediator;
}

#pragma mark - Actions

- (void)handleFolderSelectionEvent {
  if (!self.folderItem.bookmarkNode)
    return;

  std::set<const BookmarkNode*> hiddenNodes;
  _folderChooserCoordinator =
      [[BookmarksFolderChooserCoordinator alloc]
       initWithBaseNavigationController:_baseNavigationController
                                             ? _baseNavigationController
                                             : _navigationController
                                       browser:self.browser
                                   hiddenNodes:hiddenNodes];
  _folderChooserCoordinator.allowsNewFolders = _allowsNewFolders;
  [_folderChooserCoordinator
      setSelectedFolder:self.folderItem.bookmarkNode];
  _folderChooserCoordinator.delegate = self;
  [_folderChooserCoordinator start];
}

- (void)handleCategoryTap:(VivaldiNSDDirectMatchCategory*)category {
  UIViewController *controller =
      [self.viewProvider makeCategoryDetailsViewController];
  controller.title = category.titleString;

  UIBarButtonItem *doneItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                               target:self
                               action:@selector(handleCancelButtonTap)];
  controller.navigationItem.rightBarButtonItem = doneItem;

  [_navigationController pushViewController:controller
                                   animated:YES];
}

- (void)stopFolderChooserCordinator {
  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;
}

- (void)dismiss {
  __weak __typeof(self) weakSelf = self;

  void (^completionBlock)(void) = ^{
    SEL didDismissSelector = @selector(newSpeedDialCoordinatorDidDismiss);
    if ([weakSelf.delegate respondsToSelector:didDismissSelector]) {
      [weakSelf.delegate newSpeedDialCoordinatorDidDismiss];
    }
  };

  [self stop];
  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:completionBlock];
}

#pragma mark - BookmarksFolderChooserCoordinatorDelegate

- (void)bookmarksFolderChooserCoordinatorDidConfirm:
(BookmarksFolderChooserCoordinator*)coordinator
                                 withSelectedFolder:
(const bookmarks::BookmarkNode*)folder {
  DCHECK(_folderChooserCoordinator);
  DCHECK(folder);
  VivaldiSpeedDialItem* selectedFolderItem =
      [[VivaldiSpeedDialItem alloc] initWithBookmark:folder];
  self.folderItem = selectedFolderItem;
  [self stopFolderChooserCordinator];
  [self.mediator manuallyChangeFolder:selectedFolderItem];
}

- (void)bookmarksFolderChooserCoordinatorDidCancel:
(BookmarksFolderChooserCoordinator*)coordinator {
  DCHECK(_folderChooserCoordinator);
  [self stopFolderChooserCordinator];
}

@end
