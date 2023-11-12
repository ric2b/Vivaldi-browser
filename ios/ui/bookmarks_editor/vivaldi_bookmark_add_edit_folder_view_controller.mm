// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/bookmarks_editor/vivaldi_bookmark_add_edit_folder_view_controller.h"

#import <UIKit/UIKit.h>


#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/bookmarks/browser/titled_url_index.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/undo_manager_wrapper.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmark_parent_folder_view.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_constants.h"
#import "ios/ui/custom_views/vivaldi_text_field_view.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_speed_dial_item.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;
using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi_bookmark_kit::SetNodeSpeeddial;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding for the body container view
UIEdgeInsets bodyContainerViewPadding = UIEdgeInsetsMake(12, 12, 0, 12);
// Padding for the text stack view
UIEdgeInsets folderNameTextViewPadding = UIEdgeInsetsMake(12, 12, 0, 0);
// Padding for the parent folder view
UIEdgeInsets parentFolderViewPadding = UIEdgeInsetsMake(24, 24, 12, 18);

} // namespace

@interface VivaldiBookmarkAddEditFolderViewController ()
                        <VivaldiBookmarkParentFolderViewDelegate,
                        BookmarksFolderChooserCoordinatorDelegate> {
  // The folder chooser coordinator.
  BookmarksFolderChooserCoordinator* _folderChooserCoordinator;
}
// Textview for speed dial/bookmark folder name
@property(nonatomic,weak) VivaldiTextFieldView* folderNameTextView;
// A view for holding the parent folder components
@property(nonatomic,weak) VivaldiBookmarkParentFolderView* parentFolderView;
// The bookmark model used.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarks;
// The Browser in which bookmarks are presented
@property(nonatomic, assign) Browser* browser;
// The user's browser state model used.
@property(nonatomic, assign) ChromeBrowserState* browserState;
// The parent item of the children about to create/or going to be edited.
@property(nonatomic,assign) VivaldiSpeedDialItem* parentItem;
// The item that keeps track of the current parent selected. This only lives on
// this view and doesn't affect the source parent incase user changes the parent
// by folder selection option.
@property(nonatomic,strong) VivaldiSpeedDialItem* folderItem;
// The item about to be edited. This can be nil.
@property(nonatomic,strong) VivaldiSpeedDialItem* editingItem;
// A BOOL to keep track whether an existing folder is being edited or a new
// folder creation is taking place.
@property(nonatomic, assign) BOOL editingExistingFolder;
// Should the controller setup Cancel and Done buttons instead of a back button.
@property(nonatomic, assign) BOOL allowsCancel;

@end

@implementation VivaldiBookmarkAddEditFolderViewController

@synthesize folderNameTextView = _folderNameTextView;
@synthesize parentFolderView = _parentFolderView;
@synthesize bookmarks = _bookmarks;
@synthesize browser = _browser;
@synthesize browserState = _browserState;
@synthesize parentItem = _parentItem;
@synthesize folderItem = _folderItem;
@synthesize editingItem = _editingItem;
@synthesize editingExistingFolder = _editingExistingFolder;
@synthesize allowsCancel = _allowsCancel;

#pragma mark - INITIALIZERS
+ (instancetype)initWithBrowser:(Browser*)browser
                           item:(VivaldiSpeedDialItem*)item
                         parent:(VivaldiSpeedDialItem*)parent
                      isEditing:(BOOL)isEditing
                   allowsCancel:(BOOL)allowsCancel {
  DCHECK(browser);
  VivaldiBookmarkAddEditFolderViewController* controller =
    [[VivaldiBookmarkAddEditFolderViewController alloc] initWithBrowser:browser];
  controller.editingItem = item;
  controller.parentItem = parent;
  if (controller.bookmarks->loaded() &&
      !controller.bookmarks->is_permanent_node(item.bookmarkNode)) {
    controller.editingExistingFolder = isEditing;
  }
  controller.allowsCancel = allowsCancel;

  // Construct the folder item from the parent item
  VivaldiSpeedDialItem* folderItem =
    [[VivaldiSpeedDialItem alloc] initWithBookmark:parent.bookmarkNode];
  controller.folderItem = folderItem;

  return controller;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super init];
  if (self) {
    _browser = browser;
    _browserState =
        _browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _bookmarks =
      ios::LocalOrSyncableBookmarkModelFactory::
           GetForBrowserState(_browserState);
    _allowsNewFolders = YES;
  }
  return self;
}

- (void)dealloc {
  [self stopFolderChooserCordinator];
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [self setUpUI];
  [self updateUI];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if (self.editingExistingFolder)
    [self.navigationController setToolbarHidden:NO];
}

#pragma mark - PRIVATE METHODS
/// Updates the textfields if editing an item, and the parent folder components.
- (void)updateUI {

  // Update the textfield with folder title if in editing mode
  if (self.editingExistingFolder && self.editingItem) {
    [self.folderNameTextView setText:self.editingItem.title];
    [self.parentFolderView setChildrenAttributesWithItem:self.editingItem];
  }
  [self.folderNameTextView setFocus];

  [self updateFolderState];
}

/// Updates the parent folder view componets, i.e. title and icon.
- (void)updateFolderState {
  [self.parentFolderView setParentFolderAttributesWithItem:self.folderItem];
}

#pragma mark - SET UP UI COMPONENETS

- (void)setUpUI {
  self.view.backgroundColor =
    [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  [self setUpNavBarView];
  [self setupContentView];
  if (self.editingExistingFolder)
    [self addToolbar];
}

/// Set up the header view
- (void)setUpNavBarView {

  self.title = !self.editingExistingFolder ?
    l10n_util::GetNSString(IDS_IOS_ADD_FOLDER) :
    l10n_util::GetNSString(IDS_IOS_EDIT_FOLDER);

  // Add Done button.
  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(saveFolder)];
  doneItem.accessibilityIdentifier =
      kBookmarkFolderEditNavigationBarDoneButtonIdentifier;
  self.navigationItem.rightBarButtonItem = doneItem;

  if (self.allowsCancel) {
    // Add cancel button
    UIBarButtonItem* cancelItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                             target:self
                             action:@selector(cancel)];
    cancelItem.accessibilityIdentifier = @"Cancel";
    self.navigationItem.leftBarButtonItem = cancelItem;
  }
}

/// Set up the speed dial view
-(void)setupContentView {
  // The container view to hold the speed dial view
  UIView* bodyContainerView = [UIView new];
  bodyContainerView.backgroundColor =
    [UIColor colorNamed: kGroupedSecondaryBackgroundColor];
  bodyContainerView.layer.cornerRadius = vBookmarkBodyCornerRadius;
  bodyContainerView.clipsToBounds = YES;
  [self.view addSubview:bodyContainerView];

  [bodyContainerView anchorTop:self.view.safeTopAnchor
                       leading:self.view.safeLeftAnchor
                        bottom:nil
                      trailing:self.view.safeRightAnchor
                       padding:bodyContainerViewPadding];

  NSString* folderTitleString =
    l10n_util::GetNSString(IDS_IOS_BOOKMARK_FOLDER_TITLE);
  VivaldiTextFieldView* folderNameTextView =
    [[VivaldiTextFieldView alloc]
      initWithPlaceholder:folderTitleString];
  _folderNameTextView = folderNameTextView;
  [bodyContainerView addSubview:folderNameTextView];
  [folderNameTextView anchorTop:bodyContainerView.topAnchor
                        leading:bodyContainerView.leadingAnchor
                         bottom:nil
                       trailing:bodyContainerView.trailingAnchor
                        padding:folderNameTextViewPadding
                           size:CGSizeMake(0, vBookmarkTextViewHeight)];

  NSString* parentFolderString =
    l10n_util::GetNSString(IDS_IOS_BOOKMARK_PARENT_FOLDER);
  VivaldiBookmarkParentFolderView* parentFolderView =
    [[VivaldiBookmarkParentFolderView alloc]
      initWithTitle:[parentFolderString uppercaseString]];
  _parentFolderView = parentFolderView;
  [bodyContainerView addSubview:parentFolderView];
  [parentFolderView anchorTop:folderNameTextView.bottomAnchor
                        leading:bodyContainerView.leadingAnchor
                         bottom:bodyContainerView.bottomAnchor
                       trailing:bodyContainerView.trailingAnchor
                        padding:parentFolderViewPadding];
  [parentFolderView setSpeedDialSelectionHidden:NO];
  parentFolderView.delegate = self;
}

/// Adds toolbar at the bottom when an exiting item is editing
- (void)addToolbar {
  self.navigationController.toolbarHidden = NO;
  NSString* titleString = l10n_util::GetNSString(IDS_IOS_DELETE_FOLDER);
  UIBarButtonItem* deleteButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(deleteFolder)];
  deleteButton.accessibilityIdentifier =
      kBookmarkFolderEditorDeleteButtonIdentifier;
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  deleteButton.tintColor = [UIColor colorNamed:kRedColor];

  [self setToolbarItems:@[ spaceButton, deleteButton, spaceButton ]
               animated:NO];
}

/// Executes when the done button is tapped.
- (void)saveFolder {

  if (!self.bookmarks->loaded())
    return;

  DCHECK(self.folderItem.bookmarkNode);
  NSString *folderString = self.folderNameTextView.getText;
  if (!self.folderNameTextView.hasText)
    return;

  std::u16string folderTitle = base::SysNSStringToUTF16(folderString);

  if (self.editingExistingFolder && self.editingItem.bookmarkNode) {
    DCHECK(self.editingItem.bookmarkNode);

    // Update title if changed
    if (self.editingItem.bookmarkNode->GetTitle() != folderTitle) {
      self.bookmarks->SetTitle(self.editingItem.bookmarkNode,
                               folderTitle,
                               bookmarks::metrics::BookmarkEditSource::kUser);
    }

    // Set speed dial status if changed
    if (self.editingItem.isSpeedDial !=
      self.parentFolderView.isUseAsSpeedDialFolder) {
        SetNodeSpeeddial(self.bookmarks,
                         self.editingItem.bookmarkNode,
                         self.parentFolderView.isUseAsSpeedDialFolder);
    }

    // Move folder if changed
    if (self.editingItem.parent->id() != self.folderItem.id) {
      std::vector<const bookmarks::BookmarkNode*> editedNodes;
      editedNodes.push_back(self.editingItem.bookmarkNode);
      bookmark_utils_ios::MoveBookmarks(editedNodes,
                                        self.bookmarks,
                                        self.bookmarks,
                                        self.folderItem.bookmarkNode);
    }
    [self notifyDelegateWithFolder:self.folderItem.bookmarkNode];
  } else {

    // Store the folder
    const BookmarkNode* folder =
      self.bookmarks->AddFolder(self.folderItem.bookmarkNode,
                                self.folderItem.bookmarkNode->children().size(),
                                folderTitle);

    // Set speed dial status
    BOOL isSpeedDialFolder = self.parentFolderView.isUseAsSpeedDialFolder;
    if (isSpeedDialFolder)
      SetNodeSpeeddial(self.bookmarks, folder, YES);
    [self notifyDelegateWithFolder:folder];
  }
}

/// Dismiss the presented controller on cancel tap or pop view
/// controller when pushed within navigation controller.
- (void)cancel {
  if (self.allowsCancel) {
    [self dismissViewControllerAnimated:YES completion:nil];
  } else {
    [self.navigationController popViewControllerAnimated:YES];
  }
}

/// Executes when delete button is tapped.
- (void)deleteFolder {
  if (!self.bookmarks->loaded())
    return;
  DCHECK(self.editingExistingFolder);
  DCHECK(self.editingItem);
  std::set<const BookmarkNode*> editedNodes;
  editedNodes.insert(self.editingItem.bookmarkNode);
  bookmark_utils_ios::DeleteBookmarks(editedNodes, self.bookmarks);
  [self notifyDelegateWithFolder:nil];
}

/// Notify the listeners that the bookmark model is updated and associated UI
/// should be refreshed.
- (void)notifyDelegateWithFolder:(const BookmarkNode*)folder {
  if (self.delegate) {
    [self.delegate didCreateNewFolder:folder];
  }

  [self.view endEditing:YES];
  [self cancel];
}


#pragma mark - PARENT FOLDER SELECTION DELEGATE
- (void) didTapParentFolder {
  if (!self.bookmarks->loaded() || !self.folderItem.bookmarkNode)
    return;

  std::set<const BookmarkNode*> editedNodes;

  if (self.editingItem.bookmarkNode)
    editedNodes.insert(self.editingItem.bookmarkNode);

  _folderChooserCoordinator = [[BookmarksFolderChooserCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:self.browser
                           hiddenNodes:editedNodes];
  [_folderChooserCoordinator setSelectedFolder:self.folderItem.bookmarkNode];
  _folderChooserCoordinator.allowsNewFolders = _allowsNewFolders;
  _folderChooserCoordinator.delegate = self;
  [_folderChooserCoordinator start];
}

- (void)stopFolderChooserCordinator {
  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;
}

#pragma mark - BookmarksFolderChooserCoordinatorDelegate

- (void)bookmarksFolderChooserCoordinatorDidConfirm:
            (BookmarksFolderChooserCoordinator*)coordinator
                                 withSelectedFolder:
                                     (const bookmarks::BookmarkNode*)folder {
  DCHECK(_folderChooserCoordinator);
  DCHECK(folder);

  self.folderItem.bookmarkNode = folder;
  [self updateFolderState];
  [self stopFolderChooserCordinator];
}

- (void)bookmarksFolderChooserCoordinatorDidCancel:
    (BookmarksFolderChooserCoordinator*)coordinator {
  DCHECK(_folderChooserCoordinator);
  [self stopFolderChooserCordinator];
}

@end
