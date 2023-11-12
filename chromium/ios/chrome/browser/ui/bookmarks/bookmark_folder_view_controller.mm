// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_view_controller.h"

#import <memory>
#import <vector>

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_folder_item.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "base/mac/foundation_util.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/bookmarks/vivaldi_bookmark_add_edit_folder_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/vivaldi_bookmark_folder_selection_header_view.h"
#import "ios/chrome/browser/ui/bookmarks/vivaldi_bookmark_prefs.h"
#import "ios/chrome/browser/ui/bookmarks/vivaldi_bookmarks_constants.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi::IsVivaldiRunning;
// End Vivaldi

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The estimated height of every folder cell.
const CGFloat kEstimatedFolderCellHeight = 48.0;

// Height of section headers/footers.
#if !defined(VIVALDI_BUILD)
const CGFloat kSectionHeaderHeight = 8.0;
#endif
// End Vivaldi

const CGFloat kSectionFooterHeight = 8.0;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAddFolder = kSectionIdentifierEnumZero,
  SectionIdentifierBookmarkFolders,
  BookmarkHomeSectionIdentifierMessages // Vivaldi
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCreateNewFolder = kItemTypeEnumZero,
  ItemTypeBookmarkFolder,
  BookmarkHomeItemTypeMessage // Vivaldi
};

}  // namespace

using bookmarks::BookmarkNode;

@interface BookmarkFolderViewController ()<
    BookmarkFolderEditorViewControllerDelegate,
    BookmarkModelBridgeObserver,
    UITableViewDataSource,
    UITableViewDelegate,
    VivaldiBookmarkFolderSelectionHeaderViewDelegate,
    VivaldiBookmarkAddEditControllerDelegate> {
  std::set<const BookmarkNode*> _editedNodes;
  std::vector<const BookmarkNode*> _folders;
  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;
}

// Should the controller setup Cancel and Done buttons instead of a back button.
@property(nonatomic, assign) BOOL allowsCancel;

// Should the controller setup a new-folder button.
@property(nonatomic, assign) BOOL allowsNewFolders;

// Reference to the main bookmark model.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;

// The currently selected folder.
@property(nonatomic, readonly) const BookmarkNode* selectedFolder;

// The view controller to present when creating a new folder.
@property(nonatomic, strong)
    BookmarkFolderEditorViewController* folderAddController;

// A linear list of folders.
@property(nonatomic, assign, readonly)
    const std::vector<const BookmarkNode*>& folders;

// The browser for this ViewController.
@property(nonatomic, readonly) Browser* browser;

// Reloads the model and the updates `self.tableView` to reflect any model
// changes.
- (void)reloadModel;

// Pushes on the navigation controller a view controller to create a new folder.
- (void)pushFolderAddViewController;

// Called when the user taps on a folder row. The cell is checked, the UI is
// locked so that the user can't interact with it, then the delegate is
// notified. Usual implementations of this delegate callback are to pop or
// dismiss this controller on selection. The delay is here to let the user get a
// visual feedback of the selection before this view disappears.
- (void)delayedNotifyDelegateOfSelection;

// Vivaldi
// The view controller to present when pushing to add or edit folder
@property(nonatomic, strong)
  VivaldiBookmarkAddEditFolderViewController* bookmarkFolderEditorController;
// The parent item of the children about to create/or going to be edited.
@property(nonatomic,assign) VivaldiSpeedDialItem* parentItem;
// The user's browser state model used.
@property(nonatomic, assign) ChromeBrowserState* browserState;
@property(nonatomic, weak)
  VivaldiBookmarkFolderSelectionHeaderView* tableHeaderView;
@property(nonatomic, strong) NSString* searchText;
// End Vivaldi

@end

@implementation BookmarkFolderViewController

@synthesize allowsCancel = _allowsCancel;
@synthesize allowsNewFolders = _allowsNewFolders;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize editedNodes = _editedNodes;
@synthesize folderAddController = _folderAddController;
@synthesize delegate = _delegate;
@synthesize folders = _folders;
@synthesize selectedFolder = _selectedFolder;

// Vivaldi
@synthesize bookmarkFolderEditorController = _bookmarkFolderEditorController;
@synthesize browserState = _browserState;
@synthesize parentItem = _parentItem;
@synthesize tableHeaderView = _tableHeaderView;
@synthesize searchText = _searchText;
// End Vivaldi

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                     allowsNewFolders:(BOOL)allowsNewFolders
                          editedNodes:
                              (const std::set<const BookmarkNode*>&)nodes
                         allowsCancel:(BOOL)allowsCancel
                       selectedFolder:(const BookmarkNode*)selectedFolder
                              browser:(Browser*)browser {
  DCHECK(bookmarkModel);
  DCHECK(bookmarkModel->loaded());
  DCHECK(browser);
  DCHECK(selectedFolder == NULL || selectedFolder->is_folder());

  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _browser = browser;
    _allowsCancel = allowsCancel;
    _allowsNewFolders = allowsNewFolders;
    _bookmarkModel = bookmarkModel;
    _editedNodes = nodes;
    _selectedFolder = selectedFolder;

    // Set up the bookmark model oberver.
    _modelBridge.reset(
        new bookmarks::BookmarkModelBridge(self, _bookmarkModel));

    // Vivaldi
    _browserState =
        _browser->GetBrowserState()->GetOriginalChromeBrowserState();
    // End Vivaldi

  }
  return self;
}

- (void)changeSelectedFolder:(const BookmarkNode*)selectedFolder {
  DCHECK(selectedFolder);
  DCHECK(selectedFolder->is_folder());
  _selectedFolder = selectedFolder;

  if (IsVivaldiRunning()) {
    [self reloadModelVivaldi];
  } else {
  [self reloadModel];
  } // End Vivaldi

}

- (void)dealloc {
  _folderAddController.delegate = nil;

  // Vivaldi
  _bookmarkFolderEditorController.delegate = nil;
  // End Vivaldi

}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  [super loadModel];

  self.view.accessibilityIdentifier =
      kBookmarkFolderPickerViewContainerIdentifier;
  self.title = l10n_util::GetNSString(IDS_IOS_BOOKMARK_CHOOSE_GROUP_BUTTON);

  if (self.allowsCancel) {
    UIBarButtonItem* cancelItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                             target:self
                             action:@selector(cancel:)];
    cancelItem.accessibilityIdentifier = @"Cancel";
    self.navigationItem.leftBarButtonItem = cancelItem;
  }
  // Configure the table view.
  self.tableView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  self.tableView.estimatedRowHeight = kEstimatedFolderCellHeight;
  self.tableView.rowHeight = UITableViewAutomaticDimension;

  // Vivaldi
  [self setUpTableHeaderView];
  [self updateTableHeaderViewState];
  // End Vivaldi

}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Whevener this VC is displayed the bottom toolbar will be hidden.
  self.navigationController.toolbarHidden = YES;

  // Load the model.

  if (IsVivaldiRunning()) {
    [self reloadModelVivaldi];
  } else {
  [self reloadModel];
  } // End Vivaldi

}

#pragma mark - Presentation controller integration

- (BOOL)shouldBeDismissedOnTouchOutside {
  return NO;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.delegate folderPickerDidCancel:self];
  return YES;
}

#pragma mark - UITableViewDelegate

// Vivaldi
// We will skip this header section rendering part for Vivaldi since it only
// produces a line over the 'Add New' folder item which is not needed
// for Vivaldi.
#if !defined(VIVALDI_BUILD)

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return kSectionHeaderHeight;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  CGRect headerViewFrame =
      CGRectMake(0, 0, CGRectGetWidth(tableView.frame),
                 [self tableView:tableView heightForHeaderInSection:section]);
  UIView* headerView = [[UIView alloc] initWithFrame:headerViewFrame];
  if (section ==
          [self.tableViewModel
              sectionForSectionIdentifier:SectionIdentifierBookmarkFolders] &&
      [self shouldShowDefaultSection]) {
    CGRect separatorFrame =
        CGRectMake(0, 0, CGRectGetWidth(headerView.bounds),
                   1.0 / [[UIScreen mainScreen] scale]);  // 1-pixel divider.
    UIView* separator = [[UIView alloc] initWithFrame:separatorFrame];
    separator.autoresizingMask = UIViewAutoresizingFlexibleBottomMargin |
                                 UIViewAutoresizingFlexibleWidth;
    separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
    [headerView addSubview:separator];
  }
  return headerView;
}
#endif // End Vivaldi

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return kSectionFooterHeight;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  return [[UIView alloc] init];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  switch ([self.tableViewModel
      sectionIdentifierForSectionIndex:indexPath.section]) {
    case SectionIdentifierAddFolder:
      [self pushFolderAddViewController];
      break;

    case SectionIdentifierBookmarkFolders: {
      int folderIndex = indexPath.row;
      // If `shouldShowDefaultSection` is YES, the first cell on this section
      // should call `pushFolderAddViewController`.
      if ([self shouldShowDefaultSection]) {
        NSInteger itemType =
            [self.tableViewModel itemTypeForIndexPath:indexPath];
        if (itemType == ItemTypeCreateNewFolder) {
          [self pushFolderAddViewController];
          return;
        }
        // If `shouldShowDefaultSection` is YES we need to offset by 1 to get
        // the right BookmarkNode from `self.folders`.
        folderIndex--;
      }

      if (IsVivaldiRunning()) {
        BookmarkFolderItem* folderItem =
            base::mac::ObjCCast<BookmarkFolderItem>(
                [self.tableViewModel itemAtIndexPath:indexPath]);
        const BookmarkNode* folder = folderItem.bookmarkNode;
        [self changeSelectedFolder:folder];
      } else {
      const BookmarkNode* folder = self.folders[folderIndex];
      [self changeSelectedFolder:folder];
      } // End Vivaldi

      [self delayedNotifyDelegateOfSelection];
      break;
    }
  }
}

#pragma mark - BookmarkFolderEditorViewControllerDelegate

- (void)bookmarkFolderEditor:(BookmarkFolderEditorViewController*)folderEditor
      didFinishEditingFolder:(const BookmarkNode*)folder {
  DCHECK(folder);

  if (IsVivaldiRunning()) {
    [self reloadModelVivaldi];
  } else {
  [self reloadModel];
  } // End Vivaldi

  [self changeSelectedFolder:folder];
  [self delayedNotifyDelegateOfSelection];
}

- (void)bookmarkFolderEditorDidDeleteEditedFolder:
    (BookmarkFolderEditorViewController*)folderEditor {
  NOTREACHED();
}

- (void)bookmarkFolderEditorDidCancel:
    (BookmarkFolderEditorViewController*)folderEditor {
  [self.navigationController popViewControllerAnimated:YES];
  self.folderAddController.delegate = nil;
  self.folderAddController = nil;
}

- (void)bookmarkFolderEditorWillCommitTitleChange:
    (BookmarkFolderEditorViewController*)controller {
  // Do nothing.
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate folderPickerDidDismiss:self];
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  // The bookmark model is assumed to be loaded when this controller is created.
  NOTREACHED();
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  if (!bookmarkNode->is_folder())
    return;

  if (IsVivaldiRunning()) {
    [self reloadModelVivaldi];
  } else {
  [self reloadModel];
  } // End Vivaldi

}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {

  if (IsVivaldiRunning()) {
    [self reloadModelVivaldi];
  } else {
  [self reloadModel];
  } // End Vivaldi

}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (bookmarkNode->is_folder()) {

    if (IsVivaldiRunning()) {
      [self reloadModelVivaldi];
    } else {
    [self reloadModel];
    } // End Vivaldi

  }
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)bookmarkNode
                 fromFolder:(const BookmarkNode*)folder {
  // Remove node from editedNodes if it is already deleted (possibly remotely by
  // another sync device).
  if (self.editedNodes.find(bookmarkNode) != self.editedNodes.end()) {
    self.editedNodes.erase(bookmarkNode);
    // if editedNodes becomes empty, nothing to move.  Exit the folder picker.
    if (self.editedNodes.empty()) {
      [self.delegate folderPickerDidCancel:self];
    }
    // Exit here because nodes in editedNodes cannot be any visible folders in
    // folder picker.
    return;
  }

  if (!bookmarkNode->is_folder())
    return;

  if (bookmarkNode == self.selectedFolder) {
    // The selected folder has been deleted. Fallback on the Mobile Bookmarks
    // node.
    [self changeSelectedFolder:self.bookmarkModel->mobile_node()];
  }

  if (IsVivaldiRunning()) {
    [self reloadModelVivaldi];
  } else {
  [self reloadModel];
  } // End Vivaldi

}

- (void)bookmarkModelRemovedAllNodes {
  // The selected folder is no longer valid. Fallback on the Mobile Bookmarks
  // node.
  [self changeSelectedFolder:self.bookmarkModel->mobile_node()];

  if (IsVivaldiRunning()) {
    [self reloadModelVivaldi];
  } else {
  [self reloadModel];
  } // End Vivaldi

}

#pragma mark - Actions

- (void)done:(id)sender {
  [self.delegate folderPicker:self didFinishWithFolder:self.selectedFolder];
}

- (void)cancel:(id)sender {
  [self.delegate folderPickerDidCancel:self];
}

#pragma mark - Private

- (BOOL)shouldShowDefaultSection {
  return self.allowsNewFolders;
}

- (void)reloadModel {
  _folders = bookmark_utils_ios::VisibleNonDescendantNodes(self.editedNodes,
                                                           self.bookmarkModel);

  // Delete any existing section.
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierAddFolder])
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierAddFolder];
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierBookmarkFolders])
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierBookmarkFolders];

  // Creates Folders Section
  [self.tableViewModel
      addSectionWithIdentifier:SectionIdentifierBookmarkFolders];

  // Adds default "Add Folder" item if needed.
  if ([self shouldShowDefaultSection]) {
    BookmarkFolderItem* createFolderItem =
        [[BookmarkFolderItem alloc] initWithType:ItemTypeCreateNewFolder
                                           style:BookmarkFolderStyleNewFolder];
    // Add the "Add Folder" Item to the same section as the rest of the folder
    // entries.
    [self.tableViewModel addItem:createFolderItem
         toSectionWithIdentifier:SectionIdentifierBookmarkFolders];
  }

  // Add Folders entries.
  for (NSUInteger row = 0; row < _folders.size(); row++) {
    const BookmarkNode* folderNode = self.folders[row];
    BookmarkFolderItem* folderItem = [[BookmarkFolderItem alloc]
        initWithType:ItemTypeBookmarkFolder
               style:BookmarkFolderStyleFolderEntry];
    folderItem.title = bookmark_utils_ios::TitleForBookmarkNode(folderNode);
    folderItem.currentFolder = (self.selectedFolder == folderNode);

    // Vivaldi
    folderItem.bookmarkNode = folderNode;
    folderItem.isSpeedDial = GetSpeeddial(folderNode);
    // End Vivaldi

    if (IsVivaldiRunning()) {
      [self computeBookmarksForVivaldi:folderItem
                            folderNode:folderNode];
    } else {
    // Indentation level.
    NSInteger level = 0;
    const BookmarkNode* node = folderNode;
    while (node && !(self.bookmarkModel->is_root_node(node))) {
      ++level;
      node = node->parent();
    }
    // The root node is not shown as a folder, so top level folders have a
    // level strictly positive.
    DCHECK(level > 0);
    folderItem.indentationLevel = level - 1;

    [self.tableViewModel addItem:folderItem
         toSectionWithIdentifier:SectionIdentifierBookmarkFolders];
    } // End Vivaldi

  }

  [self.tableView reloadData];
}

- (void)pushFolderAddViewController {
  DCHECK(self.allowsNewFolders);

  if (IsVivaldiRunning()) {
    VivaldiSpeedDialItem* parentItem =
      [[VivaldiSpeedDialItem alloc] initWithBookmark:self.selectedFolder];
    _parentItem = parentItem;
    [self navigateToBookmarkFolderEditor:nil
                                  parent:self.parentItem
                               isEditing:NO];
  } else {
  BookmarkFolderEditorViewController* folderCreator =
      [BookmarkFolderEditorViewController
          folderCreatorWithBookmarkModel:self.bookmarkModel
                            parentFolder:self.selectedFolder
                                 browser:self.browser];
  folderCreator.delegate = self;
  folderCreator.snackbarCommandsHandler = self.snackbarCommandsHandler;
  [self.navigationController pushViewController:folderCreator animated:YES];
  self.folderAddController = folderCreator;
  } // End Vivaldi

}

- (void)delayedNotifyDelegateOfSelection {
  self.view.userInteractionEnabled = NO;
  __weak BookmarkFolderViewController* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        BookmarkFolderViewController* strongSelf = weakSelf;
        // Early return if the controller has been deallocated.
        if (!strongSelf)
          return;
        strongSelf.view.userInteractionEnabled = YES;
        [strongSelf done:nil];
      });
}

#pragma mark - VIVALDI

/// Set up the table header view that contains the search bar and toggle to show
/// only speed dial folders.
- (void)setUpTableHeaderView {
  VivaldiBookmarkFolderSelectionHeaderView* tableHeaderView =
    [VivaldiBookmarkFolderSelectionHeaderView new];
  tableHeaderView.frame = CGRectMake(0, 0,
                                     self.view.bounds.size.width,
                                     vBookmarkFolderSelectionHeaderViewHeight);
  _tableHeaderView = tableHeaderView;
  self.tableView.tableHeaderView = tableHeaderView;
  tableHeaderView.delegate = self;
}

/// This is an extension for chromium 'reloadModel' method to compute bookmarks
/// for Vivaldi respecting Vivaldi pref. Note that the indentation is available only
/// when all folders are visible.
- (void)computeBookmarksForVivaldi:(BookmarkFolderItem*)folderItem
                        folderNode:(const BookmarkNode*)folderNode {

  // Remove empty message section if available
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:BookmarkHomeSectionIdentifierMessages])
    [self.tableViewModel
        removeSectionWithIdentifier:BookmarkHomeSectionIdentifierMessages];

  // Folder indentation will be available when all folders are visible. When
  // only speed dial folders are visible, list the folders normally.
  if (self.showOnlySpeedDialFolders) {
    if (folderItem.isSpeedDial)
      [self.tableViewModel addItem:folderItem
           toSectionWithIdentifier:SectionIdentifierBookmarkFolders];
  } else {
    // Indentation level.
    NSInteger level = 0;
    const BookmarkNode* node = folderNode;
    while (node && !(self.bookmarkModel->is_root_node(node))) {
      ++level;
      node = node->parent();
    }
    // The root node is not shown as a folder, so top level folders have a
    // level strictly positive.
    DCHECK(level > 0);
    folderItem.indentationLevel = level - 1;

    [self.tableViewModel addItem:folderItem
         toSectionWithIdentifier:SectionIdentifierBookmarkFolders];
  }
}

/// This method is reponsible for searching through the bookmark model
/// with search keyword and return the result. If there are no result
/// and empty result cell will return. Note that this only returns the folder
/// and respects the setting 'Show only Speed dial folders'
- (void)computeBookmarkTableViewDataMatching:(NSString*)searchText
                  orShowMessageWhenNoResults:(NSString*)noResults {

  // Delete any existing section.
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierAddFolder])
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierAddFolder];
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierBookmarkFolders])
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierBookmarkFolders];
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:BookmarkHomeSectionIdentifierMessages])
    [self.tableViewModel
        removeSectionWithIdentifier:BookmarkHomeSectionIdentifierMessages];

  // Creates Folders and empty result Section
  [self.tableViewModel
      addSectionWithIdentifier:SectionIdentifierBookmarkFolders];
  [self.tableViewModel
      addSectionWithIdentifier:BookmarkHomeSectionIdentifierMessages];

  std::vector<const BookmarkNode*> nodes;
  bookmarks::QueryFields query;
  query.word_phrase_query.reset(new std::u16string);
  *query.word_phrase_query = base::SysNSStringToUTF16(searchText);
  GetBookmarksMatchingProperties(self.bookmarkModel,
                                 query,
                                 vMaxBookmarkFolderSearchResults,
                                 &nodes);

  int count = 0;
  for (const BookmarkNode* node : nodes) {
    if (!node->is_folder()) {
      continue;
    }

    if (self.showOnlySpeedDialFolders) {
      if (!GetSpeeddial(node)) {
        continue;
      }
    }

    // When search result is visible ignore the folder indentation and
    // show a normal list.
    BookmarkFolderItem* folderItem = [[BookmarkFolderItem alloc]
        initWithType:ItemTypeBookmarkFolder
               style:BookmarkFolderStyleFolderEntry];
    folderItem.title = bookmark_utils_ios::TitleForBookmarkNode(node);
    folderItem.currentFolder = (self.selectedFolder == node);
    folderItem.bookmarkNode = node;
    folderItem.isSpeedDial = GetSpeeddial(node);

    [self.tableViewModel addItem:folderItem
         toSectionWithIdentifier:SectionIdentifierBookmarkFolders];

    count++;
  }

  if (count == 0) {
    TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:BookmarkHomeItemTypeMessage];
    item.textAlignment = NSTextAlignmentLeft;
    item.textColor = [UIColor colorNamed:kTextPrimaryColor];
    item.text = noResults;
    [self.tableViewModel addItem:item
         toSectionWithIdentifier:BookmarkHomeSectionIdentifierMessages];
  }
}

/// Returns the setting from prefs to show only speed dial folders or all folders
- (BOOL)showOnlySpeedDialFolders {
  if (!self.browserState)
    return NO;

  return [VivaldiBookmarkPrefs
          getFolderViewModeFromPrefService:self.browserState->GetPrefs()];
}

/// Updates the UISwitch state from pref.
- (void)updateTableHeaderViewState {
  [self.tableHeaderView
    setShowOnlySpeedDialFolder:self.showOnlySpeedDialFolders];
}

- (void)reloadModelVivaldi {
  if ([self.searchText length] == 0) {
    [self reloadModel];
  } else {
    NSString* noResults = l10n_util::GetNSString(IDS_HISTORY_NO_SEARCH_RESULTS);
    [self computeBookmarkTableViewDataMatching:self.searchText
                    orShowMessageWhenNoResults:noResults];
    [self.tableView reloadData];
  }
}

/// Parameters: Item can be nil, parent is non-null and a boolean whether
/// presenting on editing mode or not is provided.
- (void)navigateToBookmarkFolderEditor:(VivaldiSpeedDialItem*)item
                                parent:(VivaldiSpeedDialItem*)parent
                             isEditing:(BOOL)isEditing {
  VivaldiBookmarkAddEditFolderViewController* controller =
    [VivaldiBookmarkAddEditFolderViewController
       initWithBrowser:self.browser
                  item:item
                parent:parent
             isEditing:isEditing
          allowsCancel:NO];

  controller.delegate = self;
  [self.navigationController pushViewController:controller animated:YES];
  self.bookmarkFolderEditorController = controller;
}

#pragma mark - VivaldiBookmarkFolderSelectionHeaderViewDelegate
- (void)didChangeShowOnlySpeedDialFoldersState:(BOOL)show {
  [VivaldiBookmarkPrefs setFolderViewMode:show
                           inPrefServices:self.browserState->GetPrefs()];
  [self reloadModelVivaldi];
}

- (void)searchBarTextDidChange:(NSString*)searchText {
  self.searchText = searchText;
  [self reloadModelVivaldi];
}

#pragma mark - VIVALDI_BOOKMARK_ADD_EDIT_CONTROLLER_DELEGATE
- (void)didCreateNewFolder:(const bookmarks::BookmarkNode*)folder {
  DCHECK(folder);
  [self reloadModelVivaldi];
  [self changeSelectedFolder:folder];
  [self delayedNotifyDelegateOfSelection];
}

@end
