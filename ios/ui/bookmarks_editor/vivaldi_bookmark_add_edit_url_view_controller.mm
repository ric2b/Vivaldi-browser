// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/bookmarks_editor/vivaldi_bookmark_add_edit_url_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/bookmarks/browser/titled_url_index.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "components/url_formatter/url_fixer.h"
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
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_speed_dial_item.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;
using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi_bookmark_kit::SetNodeDescription;
using vivaldi_bookmark_kit::SetNodeNickname;
using vivaldi_bookmark_kit::SetNodeSpeeddial;

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

// Padding for the body container view
UIEdgeInsets bodyContainerViewPadding = UIEdgeInsetsMake(12, 12, 0, 12);
// Padding for the text stack view
UIEdgeInsets textViewStackPadding = UIEdgeInsetsMake(12, 12, 0, 0);
// Padding for the parent folder view
UIEdgeInsets parentFolderViewPadding = UIEdgeInsetsMake(24, 24, 12, 18);

}  // namespace

@interface VivaldiBookmarkAddEditURLViewController ()
                      <VivaldiBookmarkParentFolderViewDelegate,
                      BookmarksFolderChooserCoordinatorDelegate> {
// The folder chooser coordinator.
BookmarksFolderChooserCoordinator* _folderChooserCoordinator;
}

// Textview for speed dial/bookmark name
@property(nonatomic,weak) VivaldiTextFieldView* nameTextView;
// Textview for speed dial/bookmark URL
@property(nonatomic,weak) VivaldiTextFieldView* urlTextView;
// Textview for speed dial/bookmark nickname
@property(nonatomic,weak) VivaldiTextFieldView* nickNameTextView;
// Textview for speed dial/bookmark description
@property(nonatomic,weak) VivaldiTextFieldView* descriptionTextView;
// A view that holds the parent folder details
@property(nonatomic,weak) VivaldiBookmarkParentFolderView* parentFolderView;
// The bookmark model used.
@property(nonatomic,assign) bookmarks::BookmarkModel* bookmarks;
// The Browser in which bookmarks are presented
@property(nonatomic,assign) Browser* browser;
// The user's browser state model used.
@property(nonatomic,assign) ChromeBrowserState* browserState;
// The parent item of the children about to create/or going to be edited.
@property(nonatomic,assign) VivaldiSpeedDialItem* parentItem;
// The item that keeps track of the current parent selected. This only lives on
// this view and doesn't affect the source parent incase user changes the parent
// by folder selection option.
@property(nonatomic,strong) VivaldiSpeedDialItem* folderItem;
// The item about to be edited. This can be nil.
@property(nonatomic,strong) VivaldiSpeedDialItem* editingItem;
// A BOOL to keep track whether an exiting item is being edited or a new item
// creation is taking place.
@property(nonatomic,assign) BOOL editingExistingItem;
// Should the controller setup Cancel and Done buttons instead of a back button.
@property(nonatomic, assign) BOOL allowsCancel;

@end

@implementation VivaldiBookmarkAddEditURLViewController
@synthesize nameTextView = _nameTextView;
@synthesize urlTextView = _urlTextView;
@synthesize nickNameTextView = _nickNameTextView;
@synthesize descriptionTextView = _descriptionTextView;
@synthesize parentFolderView = _parentFolderView;
@synthesize bookmarks = _bookmarks;
@synthesize browser = _browser;
@synthesize browserState = _browserState;
@synthesize parentItem = _parentItem;
@synthesize folderItem = _folderItem;
@synthesize editingItem = _editingItem;
@synthesize editingExistingItem = _editingExistingItem;
@synthesize allowsCancel = _allowsCancel;


#pragma mark - INITIALIZER
+ (instancetype)initWithBrowser:(Browser*)browser
                           item:(VivaldiSpeedDialItem*)item
                         parent:(VivaldiSpeedDialItem*)parent
                      isEditing:(BOOL)isEditing
                   allowsCancel:(BOOL)allowsCancel {
  DCHECK(browser);
  VivaldiBookmarkAddEditURLViewController* controller =
    [[VivaldiBookmarkAddEditURLViewController alloc] initWithBrowser:browser];
  controller.editingItem = item;
  controller.parentItem = parent;
  controller.editingExistingItem = isEditing;
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
  if (self.editingExistingItem)
    [self.navigationController setToolbarHidden:NO];
}

#pragma mark - PRIVATE METHODS
/// Updates the textfields if editing an item, and the parent folder components.
- (void)updateUI {

  // Update the textfield with folder title if in editing mode
  if (self.editingExistingItem && self.editingItem) {
    [self.nameTextView setText:self.editingItem.title];
    [self.urlTextView setText:self.editingItem.urlString];
    [self.nickNameTextView setText:self.editingItem.nickname];
    [self.descriptionTextView setText:self.editingItem.description];
    [self.parentFolderView setChildrenAttributesWithItem:self.editingItem];
  }

  // Set focus on first text field
  [self.nameTextView setFocus];

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
  if (self.editingExistingItem)
    [self addToolbar];
}

/// Set up the header view
- (void)setUpNavBarView {

  self.title = !self.editingExistingItem ?
    l10n_util::GetNSString(IDS_IOS_ADD_BOOKMARK) :
    l10n_util::GetNSString(IDS_IOS_EDIT_BOOKMARK);

  // Add Done button.
  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(saveBookmark)];
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

  NSString* namePlaceholderString =
    l10n_util::GetNSString(IDS_IOS_BOOKMARK_NAME);
  VivaldiTextFieldView* nameTextView =
    [[VivaldiTextFieldView alloc]
      initWithPlaceholder:namePlaceholderString];
  _nameTextView = nameTextView;

  NSString* urlPlaceholderString = l10n_util::GetNSString(IDS_IOS_BOOKMARK_URL);
  VivaldiTextFieldView* urlTextView =
    [[VivaldiTextFieldView alloc]
      initWithPlaceholder:urlPlaceholderString];
  [urlTextView setURLMode];
  _urlTextView = urlTextView;

  NSString* nicknamePlaceholderString =
    l10n_util::GetNSString(IDS_IOS_BOOKMARK_NICKNAME);
  VivaldiTextFieldView* nickNameTextView =
    [[VivaldiTextFieldView alloc]
      initWithPlaceholder:nicknamePlaceholderString];
  _nickNameTextView = nickNameTextView;

  NSString* descriptionPlaceholderString =
    l10n_util::GetNSString(IDS_IOS_BOOKMARK_DESCRIPTION);
  VivaldiTextFieldView* descriptionTextView =
    [[VivaldiTextFieldView alloc]
     initWithPlaceholder:descriptionPlaceholderString];
  _descriptionTextView = descriptionTextView;

  // Setting height to only one view should be enough since text views are part
  // of UIStackView and distribution is FillEqually
  [nameTextView setHeightWithConstant:vBookmarkTextViewHeight];

  UIStackView *textViewStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    nameTextView, urlTextView, nickNameTextView, descriptionTextView
  ]];
  textViewStack.axis = UILayoutConstraintAxisVertical;
  textViewStack.distribution = UIStackViewDistributionFillEqually;

  [bodyContainerView addSubview:textViewStack];
  [textViewStack anchorTop: bodyContainerView.topAnchor
                   leading: bodyContainerView.leadingAnchor
                    bottom: nil
                  trailing: bodyContainerView.trailingAnchor
                   padding: textViewStackPadding];

  NSString* locationTitleString =
    l10n_util::GetNSString(IDS_IOS_BOOKMARK_LOCATION);
  VivaldiBookmarkParentFolderView* parentFolderView =
    [[VivaldiBookmarkParentFolderView alloc]
      initWithTitle: [locationTitleString uppercaseString]];
  _parentFolderView = parentFolderView;
  [bodyContainerView addSubview:parentFolderView];
  [parentFolderView anchorTop: textViewStack.bottomAnchor
                      leading: bodyContainerView.leadingAnchor
                       bottom: bodyContainerView.bottomAnchor
                     trailing: bodyContainerView.trailingAnchor
                      padding: parentFolderViewPadding];
  [parentFolderView setSpeedDialSelectionHidden:YES];
  parentFolderView.delegate = self;
}

/// Adds toolbar at the bottom when an exiting item is editing
- (void)addToolbar {
  self.navigationController.toolbarHidden = NO;
  NSString* titleString = l10n_util::GetNSString(IDS_IOS_DELETE_BOOKMARK);
  UIBarButtonItem* deleteButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(deleteBookmark)];
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
- (void)saveBookmark {
  [self executeBookmarkAddUpdateOperation];
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


/// Bookmark saving operation takes place here.
- (void) executeBookmarkAddUpdateOperation {
  DCHECK(!self.folderItem.bookmarkNode ||
         self.folderItem.bookmarkNode->is_url());

  NSString* urlString = [self inputURLString];
  GURL url = ConvertUserDataToGURL(urlString);

  if (!self.inputURLIsValid)
    return;

  NSString* bookmarkName = [self inputBookmarkName];
  if (bookmarkName.length == 0) {
    bookmarkName = urlString;
  }
  std::u16string titleString = base::SysNSStringToUTF16(bookmarkName);

  // Secondly, create an Undo group for all undoable actions.
  UndoManagerWrapper* wrapper =
      [[UndoManagerWrapper alloc] initWithBrowserState:self.browserState];

  // Create or update the bookmark.
  [wrapper startGroupingActions];

  // If editing an existing item.
  if (self.editingExistingItem && self.editingItem.bookmarkNode) {
    DCHECK(self.editingItem.bookmarkNode);
    const BookmarkNode* node = self.editingItem.bookmarkNode;

    // Update title
    self.bookmarks->SetTitle(node,
                             titleString,
                             bookmarks::metrics::BookmarkEditSource::kUser);

    // Update URL
    self.bookmarks->SetURL(node,
                           url,
                           bookmarks::metrics::BookmarkEditSource::kUser);

    // Update description
    NSString* description = [self inputBookmarkDescription];
    const std::string descriptionString = base::SysNSStringToUTF8(description);
    SetNodeDescription(self.bookmarks, node, descriptionString);

    // Update nickname
    NSString* nickname = [self inputBookmarkNickName];
    const std::string nicknameString = base::SysNSStringToUTF8(nickname);
    SetNodeNickname(self.bookmarks, node, nicknameString);

    // Move
    BOOL isMovable = self.folderItem.parent &&
      !self.folderItem.parent->HasAncestor(node) &&
      (self.editingItem.parent->id() != self.folderItem.id);

    if (isMovable) {
      self.bookmarks->Move(node,
                           self.folderItem.bookmarkNode,
                           self.folderItem.bookmarkNode->children().size());
    }

  } else {
    // Save a new bookmark
    const BookmarkNode* node =
      self.bookmarks->AddURL(self.folderItem.bookmarkNode,
                             self.folderItem.bookmarkNode->children().size(),
                             titleString,
                             url);

    // Update the description
    NSString* description = [self inputBookmarkDescription];
    if (description.length > 0) {
      const std::string descriptionString = base::SysNSStringToUTF8(description);
      SetNodeDescription(self.bookmarks, node, descriptionString);
    }

    // Update the nickname
    NSString* nickname = [self inputBookmarkNickName];
    if (nickname.length > 0) {
      const std::string nicknameString = base::SysNSStringToUTF8(nickname);
      SetNodeNickname(self.bookmarks, node, nicknameString);
    }
  }

  [wrapper stopGroupingActions];
  [wrapper resetUndoManagerChanged];

  [self dismissEditor];
}

/// Executes when delete button is tapped.
- (void)deleteBookmark {
  if (self.bookmarks->loaded() && self.editingItem.bookmarkNode) {
    std::set<const BookmarkNode*> nodes;
    nodes.insert(self.editingItem.bookmarkNode);
    bookmark_utils_ios::DeleteBookmarks(nodes, self.bookmarks);
  }
  [self dismissEditor];
}

/// Returns whether the URL inserted is a valid URL.
- (BOOL)inputURLIsValid {
  return ConvertUserDataToGURL([self inputURLString]).is_valid();
}

/// Retrieves input URL string from UI.
- (NSString*)inputURLString {
  return self.urlTextView.getText;
}

/// Retrieves input bookmark name string from UI.
- (NSString*)inputBookmarkName {
  return self.nameTextView.getText;
}

/// Retrieves input bookmark nick name string from UI.
- (NSString*)inputBookmarkNickName {
  return self.nickNameTextView.getText;
}

/// Retrieves input bookmark description string from UI.
- (NSString*)inputBookmarkDescription{
  return self.descriptionTextView.getText;
}

/// Dismiss the keyboard and navigate to previous page.
- (void)dismissEditor {
  [self.view endEditing:YES];
  [self cancel];
}

- (void)stopFolderChooserCordinator {
  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;
}

#pragma mark - PARENT FOLDER SELECTION DELEGATE
- (void)didTapParentFolder {
  if (!self.folderItem.bookmarkNode)
    return;

  std::set<const BookmarkNode*> editedNodes;

  _folderChooserCoordinator = [[BookmarksFolderChooserCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:self.browser
                           hiddenNodes:editedNodes];
  [_folderChooserCoordinator setSelectedFolder:self.folderItem.bookmarkNode];
  _folderChooserCoordinator.delegate = self;
  [_folderChooserCoordinator start];
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
  [self.navigationController popViewControllerAnimated:YES];
  [self stopFolderChooserCordinator];
}

- (void)bookmarksFolderChooserCoordinatorDidCancel:
    (BookmarksFolderChooserCoordinator*)coordinator {
  DCHECK(_folderChooserCoordinator);
  [self stopFolderChooserCordinator];
}

@end
