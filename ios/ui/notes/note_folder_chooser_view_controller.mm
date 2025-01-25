// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_folder_chooser_view_controller.h"

#import <memory>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/notes/note_node.h"
#import "components/notes/notes_model.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/notes/cells/note_folder_item.h"
#import "ios/ui/notes/note_add_edit_controller_delegate.h"
#import "ios/ui/notes/note_add_edit_folder_view_controller.h"
#import "ios/ui/notes/note_add_edit_view_controller.h"
#import "ios/ui/notes/note_folder_selection_header_view.h"
#import "ios/ui/notes/note_model_bridge_observer.h"
#import "ios/ui/notes/note_ui_constants.h"
#import "ios/ui/notes/note_utils_ios.h"
#import "ios/ui/notes/cells/note_folder_item.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

namespace {

// Maximum number of entries to fetch when searching.
const int kMaxNotesSearchResults = 50;

// The estimated height of every folder cell.
const CGFloat kEstimatedFolderCellHeight = 48.0;

// Height of section headers/footers.
const CGFloat kSectionHeaderHeight = 8.0;
const CGFloat kSectionFooterHeight = 8.0;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAddFolder = kSectionIdentifierEnumZero,
  SectionIdentifierNoteFolders,
  NoteHomeSectionIdentifierMessages,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCreateNewFolder = kItemTypeEnumZero,
  ItemTypeNoteFolder,
  NoteHomeItemTypeMessage,
};

}  // namespace

using vivaldi::NoteNode;

@interface NoteFolderChooserViewController ()<
    NoteModelBridgeObserver,
    UITableViewDataSource,
    UITableViewDelegate,
    NoteFolderSelectionHeaderViewDelegate,
    NoteAddEditControllerDelegate> {
  std::set<const vivaldi::NoteNode*> _editedNodes;
  std::vector<const vivaldi::NoteNode*> _folders;
  std::unique_ptr<notes::NoteModelBridge> _modelBridge;
}

// Should the controller setup Cancel and Done buttons instead of a back button.
@property(nonatomic, assign) BOOL allowsCancel;

// Should the controller setup a new-folder button.
@property(nonatomic, assign) BOOL allowsNewFolders;

// Reference to the main note model.
@property(nonatomic, assign) vivaldi::NotesModel* noteModel;

// The currently selected folder.
@property(nonatomic, readonly) const NoteNode* selectedFolder;

// The view controller to present when creating a new folder.
@property(nonatomic, strong)
    NoteAddEditFolderViewController* folderAddController;

// A linear list of folders.
@property(nonatomic, assign, readonly)
    const std::vector<const NoteNode*>& folders;

// The browser for this ViewController.
@property(nonatomic, readonly) Browser* browser;

// Reloads the model and the updates |self.tableView| to reflect any model
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

// The view controller to present when pushing to add or edit folder
@property(nonatomic, strong)
  NoteAddEditFolderViewController* noteFolderEditorController;
// The user's profile used.
@property(nonatomic, assign) ProfileIOS* profile;
@property(nonatomic, weak)
  NoteFolderSelectionHeaderView* tableHeaderView;
@property(nonatomic, strong) NSString* searchText;

@end

@implementation NoteFolderChooserViewController

@synthesize allowsCancel = _allowsCancel;
@synthesize allowsNewFolders = _allowsNewFolders;
@synthesize noteModel = _noteModel;
@synthesize editedNodes = _editedNodes;
@synthesize folderAddController = _folderAddController;
@synthesize delegate = _delegate;
@synthesize folders = _folders;
@synthesize selectedFolder = _selectedFolder;

@synthesize noteFolderEditorController = _noteFolderEditorController;
@synthesize profile = _profile;
@synthesize tableHeaderView = _tableHeaderView;
@synthesize searchText = _searchText;

- (instancetype)initWithNotesModel:(vivaldi::NotesModel*)noteModel
                  allowsNewFolders:(BOOL)allowsNewFolders
                       editedNodes:(const std::set<const NoteNode*>&)nodes
                      allowsCancel:(BOOL)allowsCancel
                    selectedFolder:(const NoteNode*)selectedFolder
                           browser:(Browser*)browser {
  DCHECK(noteModel);
  DCHECK(noteModel->loaded());
  DCHECK(browser);
  DCHECK(selectedFolder == NULL || selectedFolder->is_folder());
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _browser = browser;
    _allowsCancel = allowsCancel;
    _allowsNewFolders = allowsNewFolders;
    _noteModel = noteModel;
    _editedNodes = nodes;
    _selectedFolder = selectedFolder;

    // Set up the note model oberver.
    _modelBridge.reset(
        new notes::NoteModelBridge(self, _noteModel));

    _profile = _browser->GetProfile()->GetOriginalProfile();
  }

  return self;
}

- (void)changeSelectedFolder:(const NoteNode*)selectedFolder {
  DCHECK(selectedFolder);
  DCHECK(selectedFolder->is_folder());
  _selectedFolder = selectedFolder;
  [self reloadModel];
}

- (void)dealloc {
  _folderAddController.delegate = nil;
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  [super loadModel];

  self.view.accessibilityIdentifier =
      kNoteFolderPickerViewContainerIdentifier;
  self.title = l10n_util::GetNSString(IDS_VIVALDI_NOTE_CHOOSE_GROUP_BUTTON);

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

  // Add Search
  [self setUpTableHeaderView];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Whevener this VC is displayed the bottom toolbar will be hidden.
  self.navigationController.toolbarHidden = YES;

  // Load the model.
  [self reloadModelForSearch];
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
              sectionForSectionIdentifier:SectionIdentifierNoteFolders] &&
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

    case SectionIdentifierNoteFolders: {
      // If |shouldShowDefaultSection| is YES, the first cell on this section
      // should call |pushFolderAddViewController|.
      if ([self shouldShowDefaultSection]) {
        NSInteger itemType =
            [self.tableViewModel itemTypeForIndexPath:indexPath];
        if (itemType == ItemTypeCreateNewFolder) {
          [self pushFolderAddViewController];
          return;
        }
      }
      const NoteNode* folder;
      NoteFolderItem* folderItem = base::apple::ObjCCast<NoteFolderItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
      folder = folderItem.noteNode;
      [self changeSelectedFolder:folder];
      [self delayedNotifyDelegateOfSelection];
      break;
    }
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate folderPickerDidDismiss:self];
}

#pragma mark - NotesModelBridgeObserver

- (void)noteModelLoaded {
  // The note model is assumed to be loaded when this controller is created.
  NOTREACHED();
}

- (void)noteNodeChanged:(const NoteNode*)noteNode {
  if (!noteNode->is_folder())
    return;
  [self reloadModel];
}

- (void)noteNodeChildrenChanged:(const NoteNode*)noteNode {
  [self reloadModel];
}

- (void)noteNode:(const NoteNode*)noteNode
     movedFromParent:(const NoteNode*)oldParent
            toParent:(const NoteNode*)newParent {
  if (noteNode->is_folder()) {
    [self reloadModel];
  }
}

- (void)noteNodeDeleted:(const NoteNode*)noteNode
                 fromFolder:(const NoteNode*)folder {
  // Remove node from editedNodes if it is already deleted (possibly remotely by
  // another sync device).
  if (self.editedNodes.find(noteNode) != self.editedNodes.end()) {
    self.editedNodes.erase(noteNode);
    // if editedNodes becomes empty, nothing to move.  Exit the folder picker.
    if (self.editedNodes.empty()) {
      [self.delegate folderPickerDidCancel:self];
    }
    // Exit here because nodes in editedNodes cannot be any visible folders in
    // folder picker.
    return;
  }

  if (!noteNode->is_folder())
    return;

  if (noteNode == self.selectedFolder) {
    // The selected folder has been deleted. Fallback on the Mobile Notes
    // node.
    [self changeSelectedFolder:self.noteModel->main_node()];
  }
  [self reloadModel];
}

- (void)noteModelRemovedAllNodes {
  // The selected folder is no longer valid. Fallback on the Mobile Notes
  // node.
  [self changeSelectedFolder:self.noteModel->main_node()];
  [self reloadModel];
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
  _folders = note_utils_ios::VisibleNonDescendantNodes(self.editedNodes,
                                                           self.noteModel);
  // Delete any existing section.
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierAddFolder])
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierAddFolder];
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierNoteFolders])
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierNoteFolders];

  // Creates Folders Section
  [self.tableViewModel
      addSectionWithIdentifier:SectionIdentifierNoteFolders];

  // Adds default "Add Folder" item if needed.
  if ([self shouldShowDefaultSection]) {
    NoteFolderItem* createFolderItem =
        [[NoteFolderItem alloc] initWithType:ItemTypeCreateNewFolder
                                           style:NoteFolderStyleNewFolder];
    // Add the "Add Folder" Item to the same section as the rest of the folder
    // entries.
    [self.tableViewModel addItem:createFolderItem
         toSectionWithIdentifier:SectionIdentifierNoteFolders];
  }
  // Add Folders entries.
  for (NSUInteger row = 0; row < _folders.size(); row++) {
    const NoteNode* folderNode = self.folders[row];
    NoteFolderItem* folderItem = [[NoteFolderItem alloc]
        initWithType:ItemTypeNoteFolder
               style:NoteFolderStyleFolderEntry];
    folderItem.title = note_utils_ios::TitleForNoteNode(folderNode);
    folderItem.currentFolder = (self.selectedFolder == folderNode);
    // Indentation level.
    NSInteger level = 0;
    const NoteNode* node = folderNode;
    while (node && !(self.noteModel->is_root_node(node))) {
      ++level;
      node = node->parent();
    }

    folderItem.noteNode = folderNode;
    // The root node is not shown as a folder, so top level folders have a
    // level strictly positive.
    DCHECK(level > 0);
    folderItem.indentationLevel = level - 1;
    [self.tableViewModel addItem:folderItem
         toSectionWithIdentifier:SectionIdentifierNoteFolders];
  }
  [self.tableView reloadData];
}

- (void)pushFolderAddViewController {
  DCHECK(self.allowsNewFolders);
  NoteAddEditFolderViewController* folderCreator =
      [NoteAddEditFolderViewController
        initWithBrowser:self.browser
                   item:nil
                 parent:self.selectedFolder
            isEditing:NO
         allowsCancel:NO];
  folderCreator.delegate = self;

  [self.navigationController pushViewController:folderCreator animated:YES];
  self.folderAddController = folderCreator;
}

- (void)delayedNotifyDelegateOfSelection {
  self.view.userInteractionEnabled = NO;
  __weak NoteFolderChooserViewController* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        NoteFolderChooserViewController* strongSelf = weakSelf;
        // Early return if the controller has been deallocated.
        if (!strongSelf)
          return;
        strongSelf.view.userInteractionEnabled = YES;
        [strongSelf done:nil];
      });
}

- (void)computeNoteTableViewDataMatching:(NSString*)searchText
                  orShowMessageWhenNoResults:(NSString*)noResults {
  // Delete any existing section.
  if ([self.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierNoteFolders])
    [self.tableViewModel
        removeSectionWithIdentifier:SectionIdentifierNoteFolders];
 if ([self.tableViewModel
          hasSectionForSectionIdentifier:NoteHomeSectionIdentifierMessages])
    [self.tableViewModel
        removeSectionWithIdentifier:NoteHomeSectionIdentifierMessages];

  // Creates Folders and empty result Section
  [self.tableViewModel
      addSectionWithIdentifier:SectionIdentifierNoteFolders];
   [self.tableViewModel
      addSectionWithIdentifier:NoteHomeSectionIdentifierMessages];

  std::vector<const NoteNode*> nodes;
  self.noteModel->GetNotesFoldersMatching(
        base::SysNSStringToUTF16(searchText),
        kMaxNotesSearchResults,
        nodes);
  int count = 0;
  for (const NoteNode* node : nodes) {
  // When search result is visible ignore the folder indentation and
  // show a normal list.
    NoteFolderItem* folderItem =
        [[NoteFolderItem alloc]
            initWithType:ItemTypeNoteFolder
                   style:NoteFolderStyleFolderEntry];
    folderItem.title = note_utils_ios::TitleForNoteNode(node);
    folderItem.currentFolder = (self.selectedFolder == node);
    folderItem.noteNode = node;

    [self.tableViewModel addItem:folderItem
         toSectionWithIdentifier:SectionIdentifierNoteFolders];
    count++;
  }

  if (count == 0) {
    TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:NoteHomeItemTypeMessage];
    item.textAlignment = NSTextAlignmentLeft;
    item.textColor = [UIColor colorNamed:kTextPrimaryColor];
    item.text = noResults;
    [self.tableViewModel addItem:item
         toSectionWithIdentifier:NoteHomeSectionIdentifierMessages];
  }
}

/// Set up the table header view that contains the search bar
- (void)setUpTableHeaderView {
  NoteFolderSelectionHeaderView* tableHeaderView =
    [NoteFolderSelectionHeaderView new];
  tableHeaderView.frame = CGRectMake(0, 0,
                                     self.view.bounds.size.width,
                                     vNoteFolderSelectionHeaderViewHeight);
  _tableHeaderView = tableHeaderView;
  self.tableView.tableHeaderView = tableHeaderView;
  tableHeaderView.delegate = self;
}

/// Parameters: Item can be nil, parent is non-null and a boolean whether
/// presenting on editing mode or not is provided.
- (void)navigateToNoteFolderEditor:(NoteNode*)item
                                parent:(NoteNode*)parent
                             isEditing:(BOOL)isEditing {
  NoteAddEditFolderViewController* controller =
    [NoteAddEditFolderViewController
       initWithBrowser:self.browser
                  item:item
                parent:parent
             isEditing:isEditing
          allowsCancel:NO];

  controller.delegate = self;
  [self.navigationController pushViewController:controller animated:YES];
  self.noteFolderEditorController = controller;
}

- (void)reloadModelForSearch {
  if ([self.searchText length] == 0) {
    [self reloadModel];
  } else {
    NSString* noResults = l10n_util::GetNSString(IDS_HISTORY_NO_SEARCH_RESULTS);
    [self computeNoteTableViewDataMatching:self.searchText
                orShowMessageWhenNoResults:noResults];
    [self.tableView reloadData];
  }
}

#pragma mark - NoteFolderSelectionHeaderViewDelegate
- (void)searchBarTextDidChange:(NSString*)searchText {
  self.searchText = searchText;
  [self reloadModelForSearch];
}

#pragma mark - NOTE_ADD_EDIT_VIEW_CONTROLLER_DELEGATE
- (void)didCreateNewFolder:(const NoteNode*)folder {
  DCHECK(folder);
  [self reloadModel];
  [self changeSelectedFolder:folder];
  [self delayedNotifyDelegateOfSelection];
}

@end
