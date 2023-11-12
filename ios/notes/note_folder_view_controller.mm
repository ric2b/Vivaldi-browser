// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/notes/note_folder_view_controller.h"

#import <memory>
#import <vector>

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/notes/cells/note_folder_item.h"
#import "ios/notes/note_folder_editor_view_controller.h"
#import "ios/notes/note_model_bridge_observer.h"
#import "ios/notes/note_model_bridge_observer.h"
#import "ios/notes/note_ui_constants.h"
#import "ios/notes/note_utils_ios.h"
#import "notes/note_node.h"
#import "notes/notes_model.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The estimated height of every folder cell.
const CGFloat kEstimatedFolderCellHeight = 48.0;

// Height of section headers/footers.
const CGFloat kSectionHeaderHeight = 8.0;
const CGFloat kSectionFooterHeight = 8.0;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAddFolder = kSectionIdentifierEnumZero,
  SectionIdentifierNoteFolders,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCreateNewFolder = kItemTypeEnumZero,
  ItemTypeNoteFolder,
};

}  // namespace

using vivaldi::NoteNode;

@interface NoteFolderViewController ()<
    NoteFolderEditorViewControllerDelegate,
    NoteModelBridgeObserver,
    UITableViewDataSource,
    UITableViewDelegate> {
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
    NoteFolderEditorViewController* folderAddController;

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

@end

@implementation NoteFolderViewController

@synthesize allowsCancel = _allowsCancel;
@synthesize allowsNewFolders = _allowsNewFolders;
@synthesize noteModel = _noteModel;
@synthesize editedNodes = _editedNodes;
@synthesize folderAddController = _folderAddController;
@synthesize delegate = _delegate;
@synthesize folders = _folders;
@synthesize selectedFolder = _selectedFolder;

- (instancetype)initWithNotesModel:(vivaldi::NotesModel*)noteModel
                     allowsNewFolders:(BOOL)allowsNewFolders
                          editedNodes:
                              (const std::set<const NoteNode*>&)nodes
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
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Whevener this VC is displayed the bottom toolbar will be hidden.
  self.navigationController.toolbarHidden = YES;

  // Load the model.
  [self reloadModel];
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
  switch ([self.tableViewModel sectionIdentifierForSectionIndex:indexPath.section]) {
    case SectionIdentifierAddFolder:
      [self pushFolderAddViewController];
      break;

    case SectionIdentifierNoteFolders: {
      int folderIndex = indexPath.row;
      // If |shouldShowDefaultSection| is YES, the first cell on this section
      // should call |pushFolderAddViewController|.
      if ([self shouldShowDefaultSection]) {
        NSInteger itemType =
            [self.tableViewModel itemTypeForIndexPath:indexPath];
        if (itemType == ItemTypeCreateNewFolder) {
          [self pushFolderAddViewController];
          return;
        }
        // If |shouldShowDefaultSection| is YES we need to offset by 1 to get
        // the right NoteNode from |self.folders|.
        folderIndex--;
      }
      const NoteNode* folder = self.folders[folderIndex];
      [self changeSelectedFolder:folder];
      [self delayedNotifyDelegateOfSelection];
      break;
    }
  }
}

#pragma mark - NoteFolderEditorViewControllerDelegate

- (void)noteFolderEditor:(NoteFolderEditorViewController*)folderEditor
      didFinishEditingFolder:(const NoteNode*)folder {
  DCHECK(folder);
  [self reloadModel];
  [self changeSelectedFolder:folder];
  [self delayedNotifyDelegateOfSelection];
}

- (void)noteFolderEditorDidDeleteEditedFolder:
    (NoteFolderEditorViewController*)folderEditor {
  NOTREACHED();
}

- (void)noteFolderEditorDidCancel:
    (NoteFolderEditorViewController*)folderEditor {
  [self.navigationController popViewControllerAnimated:YES];
  self.folderAddController.delegate = nil;
  self.folderAddController = nil;
}

- (void)noteFolderEditorWillCommitTitleChange:
    (NoteFolderEditorViewController*)controller {
  // Do nothing.
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
  NoteFolderEditorViewController* folderCreator =
      [NoteFolderEditorViewController
          folderCreatorWithNotesModel:self.noteModel
                            parentFolder:self.selectedFolder
                                 browser:self.browser];
  folderCreator.delegate = self;
  folderCreator.snackbarCommandsHandler = self.snackbarCommandsHandler;
  [self.navigationController pushViewController:folderCreator animated:YES];
  self.folderAddController = folderCreator;
}

- (void)delayedNotifyDelegateOfSelection {
  self.view.userInteractionEnabled = NO;
  __weak NoteFolderViewController* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        NoteFolderViewController* strongSelf = weakSelf;
        // Early return if the controller has been deallocated.
        if (!strongSelf)
          return;
        strongSelf.view.userInteractionEnabled = YES;
        [strongSelf done:nil];
      });
}

@end
