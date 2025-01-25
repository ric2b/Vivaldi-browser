// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_consumer.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_mediator.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;
using l10n_util::GetNSStringF;

using bookmarks::BookmarkNode;

@interface VivaldiBookmarksEditorCoordinator()<
                                  BookmarksFolderChooserCoordinatorDelegate,
                                  VivaldiBookmarksEditorConsumer> {
  // The browser where the settings are being displayed.
  Browser* _browser;
  // The folder chooser coordinator.
  BookmarksFolderChooserCoordinator* _folderChooserCoordinator;
  // The navigation controller that is being presented. The bookmark editor view
  // controller is the child of this navigation controller.
  UINavigationController* _navigationController;
  // Receives commands to show a snackbar once a bookmark is edited or deleted.
  id<SnackbarCommands> _snackbarCommandsHandler;
}
// View provider for the bookmarks editor
@property(nonatomic, strong) VivaldiBookmarksEditorViewProvider* viewProvider;
// The item about to be edited. This can be nil.
@property(nonatomic,strong) VivaldiSpeedDialItem* editingItem;
// The parent of the item about to be edited. This can not be nil.
@property(nonatomic,strong) VivaldiSpeedDialItem* parentItem;
// The item that keeps track of the current parent selected. This only lives on
// this view and doesn't affect the source parent incase user changes the parent
// by folder selection option.s
@property(nonatomic,strong) VivaldiSpeedDialItem* parentFolderItem;
// Bookmarks editor mediator.
@property(nonatomic, strong) VivaldiBookmarksEditorMediator* mediator;
// Bookmarks editor entry point
@property(nonatomic, assign) VivaldiBookmarksEditorEntryPoint entryPoint;
@property(nonatomic, assign) BOOL isEditing;
@property(nonatomic, assign) BOOL allowsCancel;

@end

@implementation VivaldiBookmarksEditorCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
  (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                            item:(VivaldiSpeedDialItem*)item
                                          parent:(VivaldiSpeedDialItem*)parent
                                      entryPoint:
  (VivaldiBookmarksEditorEntryPoint)entryPoint
                                       isEditing:(BOOL)isEditing
                                    allowsCancel:(BOOL)allowsCancel {
  self = [self initWithBaseViewController:navigationController
                                  browser:browser
                                     item:item
                                   parent:parent
                               entryPoint:entryPoint
                                isEditing:isEditing
                             allowsCancel:allowsCancel];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      item:(VivaldiSpeedDialItem*)item
                                    parent:(VivaldiSpeedDialItem*)parent
                                entryPoint:
    (VivaldiBookmarksEditorEntryPoint)entryPoint
                                 isEditing:(BOOL)isEditing
                              allowsCancel:(BOOL)allowsCancel {

  self = [super initWithBaseViewController:viewController browser:browser];

  if (self) {
    _browser = browser;
    _editingItem = item;
    _parentItem = parent;
    _entryPoint = entryPoint;
    _isEditing = isEditing;
    _allowsCancel = allowsCancel;

    // Construct the folder item from the parent item
    VivaldiSpeedDialItem* parentFolderItem =
        [[VivaldiSpeedDialItem alloc] initWithBookmark:parent.bookmarkNode];
    _parentFolderItem = parentFolderItem;

    _snackbarCommandsHandler = HandlerForProtocol(
          self.browser->GetCommandDispatcher(), SnackbarCommands);
  }

  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self setupViewProvider];
  [self setupViewController];
  [self configureNavigationBarItems];
  [self presentViewController];
  [self setupBookmarkEditingMediator];
}

- (void)stop {
  [super stop];
  self.viewProvider = nil;
  self.parentFolderItem = nil;
  [self stopFolderChooserCordinator];
  [self.mediator disconnect];
  self.mediator = nil;
}

#pragma mark - Actions
- (void)handleCancelButtonTap {
  [self dismiss];
}

- (void)handleDoneButtonTap {
  switch (self.entryPoint) {
    case VivaldiBookmarksEditorEntryPointGroup:
      if (![self hasTitle])
        return;
      [_mediator
          setPreferenceShowSpeedDials:[self.viewProvider shouldShowSpeedDials]];
      [_mediator
          saveBookmarkFolderWithTitle:[self.viewProvider titleString]
                           useAsGroup:YES
                           parentNode:self.parentFolderItem.bookmarkNode];
      break;
    case VivaldiBookmarksEditorEntryPointFolder:
      if (![self hasTitle])
        return;
      [_mediator
          saveBookmarkFolderWithTitle:[self.viewProvider titleString]
                           useAsGroup:[self.viewProvider shouldUseAsGroup]
                           parentNode:self.parentFolderItem.bookmarkNode];
      break;
    case VivaldiBookmarksEditorEntryPointSpeedDial:
    case VivaldiBookmarksEditorEntryPointBookmark:
      [_mediator saveBookmarkWithTitle:[self.viewProvider titleString]
                                   url:[self.viewProvider urlString]
                              nickname:[self.viewProvider nicknameString]
                           description:[self.viewProvider descriptionString]
                            parentNode:self.parentFolderItem.bookmarkNode];
      break;
    default:
      break;
  }
}

#pragma mark - Private

- (void)setupViewProvider {
  self.viewProvider = [[VivaldiBookmarksEditorViewProvider alloc] init];
}

- (void)setupViewController {
  VivaldiBookmarksEditorEntryPurpose entryPurpose = self.isEditing ?
      VivaldiBookmarksEditorEntryPurposeEdit :
          VivaldiBookmarksEditorEntryPurposeAdd;

  UIViewController *controller =
      [self.viewProvider makeViewControllerWithEntryPoint:self.entryPoint
                                             entryPurpose:entryPurpose
                                onKeyboardReturnButtonTap:^{
        [self handleDoneButtonTap];
      }
                                     onFolderSelectionTap:^{
        [self didTapParentFolderForSelection];
      }
                                          onItemDeleteTap:^ {
        [self.mediator deleteBookmark];
      }];

  controller.title = [self titleForViewController];
  controller.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  controller.modalPresentationStyle = UIModalPresentationPageSheet;
  _navigationController =
      [[UINavigationController alloc] initWithRootViewController:controller];
  [self setupSheetPresentationController];
}

- (void)configureNavigationBarItems {

  UIBarButtonItem *doneItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                               target:self
                               action:@selector(handleDoneButtonTap)];
  _navigationController.topViewController
        .navigationItem.rightBarButtonItem = doneItem;

  if (self.allowsCancel) {
    UIBarButtonItem *cancelItem =
        [[UIBarButtonItem alloc]
            initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                                 target:self
                                 action:@selector(handleCancelButtonTap)];
    _navigationController.topViewController
        .navigationItem.leftBarButtonItem = cancelItem;
  }
}

- (void)setupSheetPresentationController {
  UISheetPresentationController *sheetPc =
      _navigationController.sheetPresentationController;
  sheetPc.detents = @[UISheetPresentationControllerDetent.mediumDetent,
                      UISheetPresentationControllerDetent.largeDetent];
  sheetPc.prefersScrollingExpandsWhenScrolledToEdge = NO;
  sheetPc.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
}

- (void)presentViewController {
  if (self.baseNavigationController && !self.allowsCancel) {
    [self.baseNavigationController
        pushViewController:_navigationController.topViewController
                  animated:YES];
  } else {
    [self.baseViewController presentViewController:_navigationController
                                          animated:YES completion:nil];
  }
}

- (void)setupBookmarkEditingMediator {
  ProfileIOS *profile = self.browser->GetProfile()->GetOriginalProfile();
  BookmarkModel *bookmarkModel = ios::BookmarkModelFactory::GetForProfile(profile);
  self.mediator =
    [[VivaldiBookmarksEditorMediator alloc] initWithBookmarkModel:bookmarkModel
        bookmarkNode:self.editingItem.bookmarkNode
            profile:profile];
  self.mediator.isEditing = self.isEditing;
  self.mediator.consumer = self;
  self.mediator.snackbarCommandsHandler = _snackbarCommandsHandler;
  [self updateFolderState];
  [self updateEditorViewComponents];
}

#pragma mark - Helpers

- (NSString*)titleForEntryPoint {

  switch (self.entryPoint) {
    case VivaldiBookmarksEditorEntryPointGroup:
      return GetNSString(IDS_IOS_ITEM_TYPE_GROUP);
    case VivaldiBookmarksEditorEntryPointFolder:
      return GetNSString(IDS_IOS_ITEM_TYPE_FOLDER);
    case VivaldiBookmarksEditorEntryPointSpeedDial:
      return GetNSString(IDS_IOS_ITEM_TYPE_SPEED_DIAL);
    case VivaldiBookmarksEditorEntryPointBookmark:
      return GetNSString(IDS_IOS_ITEM_TYPE_BOOKMARK);
    default:
      return @"";
  }
}

- (NSString*)titleForViewController {
  std::u16string title = base::SysNSStringToUTF16([self titleForEntryPoint]);
  return _isEditing ?
      GetNSStringF(IDS_IOS_EDIT_ITEM_WITH_TYPE, title) :
      GetNSStringF(IDS_IOS_NEW_ITEM_WITH_TYPE, title);
}

/// Updates the parent folder view componets
- (void)updateFolderState {
  if (self.entryPoint == VivaldiBookmarksEditorEntryPointGroup && !_isEditing)
    return;
  if (self.parentFolderItem.bookmarkNode == nullptr)
    return;
  [self.viewProvider updateParentWithTitle:self.parentFolderItem.title
                             parentIsGroup:self.parentFolderItem.isSpeedDial];
}

/// Updates the editor view components such as Title, Url etc.
- (void)updateEditorViewComponents {
  if (_isEditing && self.viewProvider && self.editingItem) {
    [self.viewProvider updateEditorWithTitle:self.editingItem.title
                                         url:self.editingItem.urlString
                                    nickname:self.editingItem.nickname
                                 description:self.editingItem.description
                                     isGroup:self.editingItem.isSpeedDial];
  }
}

- (void)didTapParentFolderForSelection {
  if (!self.parentFolderItem.bookmarkNode)
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
      setSelectedFolder:self.parentFolderItem.bookmarkNode];
  _folderChooserCoordinator.delegate = self;
  [_folderChooserCoordinator start];
}

- (void)stopFolderChooserCordinator {
  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;
}

- (void)dismiss {
  if (self.allowsCancel) {
    [self stop];
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  } else {
    [_baseNavigationController popToRootViewControllerAnimated:YES];
  }
}

- (BOOL)hasTitle {
  return [[self.viewProvider titleString] length] > 0;
}

#pragma mark - BookmarksFolderChooserCoordinatorDelegate

- (void)bookmarksFolderChooserCoordinatorDidConfirm:
(BookmarksFolderChooserCoordinator*)coordinator
                                 withSelectedFolder:
(const bookmarks::BookmarkNode*)folder {
  DCHECK(_folderChooserCoordinator);
  DCHECK(folder);
  VivaldiSpeedDialItem* parentFolderItem =
      [[VivaldiSpeedDialItem alloc] initWithBookmark:folder];
  self.parentFolderItem = parentFolderItem;
  [self updateFolderState];
  [self stopFolderChooserCordinator];
  [self.mediator manuallyChangeFolder:folder];
}

- (void)bookmarksFolderChooserCoordinatorDidCancel:
(BookmarksFolderChooserCoordinator*)coordinator {
  DCHECK(_folderChooserCoordinator);
  [self stopFolderChooserCordinator];
}

#pragma mark - VivaldiBookmarksEditorConsumer
- (void)didCreateNewFolder:(const bookmarks::BookmarkNode*)folder {
  [self.consumer didCreateNewFolder:folder];
  [self.viewProvider reset];
}

- (void)bookmarksEditorShouldClose {
  [self.viewProvider reset];
  [self dismiss];
}

- (void)bookmarksEditorTopSitesDidUpdate:
    (NSMutableArray<VivaldiBookmarksEditorTopSitesItem*>*)topSites {
  [self.viewProvider updateEditorWithTopSites:topSites];
}

- (void)bookmarksEditorTopSiteDidUpdate:
    (VivaldiBookmarksEditorTopSitesItem*)topSite {
  [self.viewProvider updateEditorWithTopSite:topSite];
}

#pragma mark - VivaldiBookmarksEditorConsumer

- (void)setPreferenceShowSpeedDials:(BOOL)showSpeedDials {
  [self.viewProvider updateEditorWithShowSpeedDials:showSpeedDials];
}

@end
