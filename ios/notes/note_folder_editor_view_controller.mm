// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/notes/note_folder_editor_view_controller.h"

#include <memory>
#include <set>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/mac/foundation_util.h"
#include "base/notreached.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/notes/note_folder_view_controller.h"
#import "ios/notes/note_model_bridge_observer.h"
#import "ios/notes/note_ui_constants.h"
#import "ios/notes/note_utils_ios.h"
#import "ios/notes/cells/note_parent_folder_item.h"
#import "ios/notes/cells/note_text_field_item.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using vivaldi::NoteNode;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInfo = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFolderTitle = kItemTypeEnumZero,
  ItemTypeParentFolder,
};

}  // namespace

@interface NoteFolderEditorViewController ()<
    NoteFolderViewControllerDelegate,
    NoteModelBridgeObserver,
    NoteTextFieldItemDelegate> {
  std::unique_ptr<notes::NoteModelBridge> _modelBridge;

  // Flag to ignore note model Move notifications when the move is performed
  // by this class.
  BOOL _ignoresOwnMove;
}
@property(nonatomic, assign) BOOL editingExistingFolder;
@property(nonatomic, assign) vivaldi::NotesModel* noteModel;
@property(nonatomic, assign) Browser* browser;
@property(nonatomic, assign) ChromeBrowserState* browserState;
@property(nonatomic, assign) const NoteNode* folder;
@property(nonatomic, strong) NoteFolderViewController* folderViewController;
@property(nonatomic, assign) const NoteNode* parentFolder;
@property(nonatomic, weak) UIBarButtonItem* doneItem;
@property(nonatomic, strong) NoteTextFieldItem* titleItem;
@property(nonatomic, strong) NoteParentFolderItem* parentFolderItem;
// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// |noteModel| must not be NULL and must be loaded.
- (instancetype)initWithNotesModel:(vivaldi::NotesModel*)noteModel
    NS_DESIGNATED_INITIALIZER;

// Enables or disables the save button depending on the state of the form.
- (void)updateSaveButtonState;

// Configures collection view model.
- (void)setupCollectionViewModel;

// Bottom toolbar with DELETE button that only appears when the edited folder
// allows deletion.
- (void)addToolbar;

@end

@implementation NoteFolderEditorViewController

@synthesize noteModel = _noteModel;
@synthesize delegate = _delegate;
@synthesize editingExistingFolder = _editingExistingFolder;
@synthesize folder = _folder;
@synthesize folderViewController = _folderViewController;
@synthesize parentFolder = _parentFolder;
@synthesize browser = _browser;
@synthesize browserState = _browserState;
@synthesize doneItem = _doneItem;
@synthesize titleItem = _titleItem;
@synthesize parentFolderItem = _parentFolderItem;

#pragma mark - Class methods

+ (instancetype)folderCreatorWithNotesModel:
                    (vivaldi::NotesModel*)noteModel
                                  parentFolder:(const NoteNode*)parentFolder
                                       browser:(Browser*)browser {
  DCHECK(browser);
  NoteFolderEditorViewController* folderCreator =
      [[self alloc] initWithNotesModel:noteModel];
  folderCreator.parentFolder = parentFolder;
  folderCreator.folder = NULL;
  folderCreator.browser = browser;
  folderCreator.editingExistingFolder = NO;
  return folderCreator;
}

+ (instancetype)folderEditorWithNotesModel:
                    (vivaldi::NotesModel*)noteModel
                                       folder:(const NoteNode*)folder
                                      browser:(Browser*)browser {
  DCHECK(folder);
  DCHECK(!noteModel->is_permanent_node(folder));
  DCHECK(browser);
  NoteFolderEditorViewController* folderEditor =
      [[self alloc] initWithNotesModel:noteModel];
  folderEditor.parentFolder = folder->parent();
  folderEditor.folder = folder;
  folderEditor.browser = browser;
  folderEditor.browserState =
      browser->GetBrowserState()->GetOriginalChromeBrowserState();
  folderEditor.editingExistingFolder = YES;
  return folderEditor;
}

#pragma mark - Initialization

- (instancetype)initWithNotesModel:(vivaldi::NotesModel*)noteModel {
  DCHECK(noteModel);
  DCHECK(noteModel->loaded());
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _noteModel = noteModel;

    // Set up the note model oberver.
    _modelBridge.reset(
        new notes::NoteModelBridge(self, _noteModel));
  }
  return self;
}

- (void)dealloc {
  _titleItem.delegate = nil;
  _folderViewController.delegate = nil;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.backgroundColor = self.styler.tableViewBackgroundColor;
  self.tableView.estimatedRowHeight = 150.0;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kNoteCellHorizontalLeadingInset,
                                         0, 0)];

  // Add Done button.
  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(saveFolder)];
  doneItem.accessibilityIdentifier =
      kNoteFolderEditNavigationBarDoneButtonIdentifier;
  self.navigationItem.rightBarButtonItem = doneItem;
  self.doneItem = doneItem;

  if (self.editingExistingFolder) {
    // Add Cancel Button.
    UIBarButtonItem* cancelItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                             target:self
                             action:@selector(dismiss)];
    cancelItem.accessibilityIdentifier = @"Cancel";
    self.navigationItem.leftBarButtonItem = cancelItem;

    [self addToolbar];
  }
  [self updateEditingState];
  [self setupCollectionViewModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateSaveButtonState];
  if (self.editingExistingFolder) {
    self.navigationController.toolbarHidden = NO;
  } else {
    self.navigationController.toolbarHidden = YES;
  }
}

#pragma mark - Presentation controller integration

- (BOOL)shouldBeDismissedOnTouchOutside {
  return NO;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self dismiss];
  return YES;
}

#pragma mark - Actions

- (void)dismiss {
  [self.view endEditing:YES];
  [self.delegate noteFolderEditorDidCancel:self];
}

- (void)deleteFolder {
  DCHECK(self.editingExistingFolder);
  DCHECK(self.folder);
  std::set<const NoteNode*> editedNodes;
  editedNodes.insert(self.folder);
  [self.snackbarCommandsHandler
      showSnackbarMessage:note_utils_ios::DeleteNotesWithUndoToast(
                              editedNodes, self.noteModel,
                              self.browserState)];
  [self.delegate noteFolderEditorDidDeleteEditedFolder:self];
}

- (void)saveFolder {
  DCHECK(self.parentFolder);

  NSString* folderString = self.titleItem.text;
  DCHECK(folderString.length > 0);
  std::u16string folderTitle = base::SysNSStringToUTF16(folderString);

  if (self.editingExistingFolder) {
    DCHECK(self.folder);
    // Tell delegate if folder title has been changed.
    if (self.folder->GetTitle() != folderTitle) {
      [self.delegate noteFolderEditorWillCommitTitleChange:self];
    }

    self.noteModel->SetTitle(self.folder, folderTitle);
    if (self.folder->parent() != self.parentFolder) {
      base::AutoReset<BOOL> autoReset(&_ignoresOwnMove, YES);
      std::set<const NoteNode*> editedNodes;
      editedNodes.insert(self.folder);
      [self.snackbarCommandsHandler
          showSnackbarMessage:note_utils_ios::MoveNotesWithUndoToast(
                                  editedNodes, self.noteModel,
                                  self.parentFolder, self.browserState)];
    }
  } else {
    DCHECK(!self.folder);
    self.folder = self.noteModel->AddFolder(
        self.parentFolder, self.parentFolder->children().size(), folderTitle);
  }
  [self.view endEditing:YES];
  [self.delegate noteFolderEditor:self didFinishEditingFolder:self.folder];
}

- (void)changeParentFolder {
  std::set<const NoteNode*> editedNodes;
  if (self.folder)
    editedNodes.insert(self.folder);
  NoteFolderViewController* folderViewController =
      [[NoteFolderViewController alloc]
          initWithNotesModel:self.noteModel
               allowsNewFolders:NO
                    editedNodes:editedNodes
                   allowsCancel:NO
                 selectedFolder:self.parentFolder
                        browser:_browser];
  folderViewController.delegate = self;
  folderViewController.snackbarCommandsHandler = self.snackbarCommandsHandler;

  self.folderViewController = folderViewController;

  [self.navigationController pushViewController:folderViewController
                                       animated:YES];
}

#pragma mark - NoteFolderViewControllerDelegate

- (void)folderPicker:(NoteFolderViewController*)folderPicker
    didFinishWithFolder:(const NoteNode*)folder {
  self.parentFolder = folder;
  [self updateParentFolderState];
  [self.navigationController popViewControllerAnimated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

- (void)folderPickerDidCancel:(NoteFolderViewController*)folderPicker {
  [self.navigationController popViewControllerAnimated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

- (void)folderPickerDidDismiss:(NoteFolderViewController*)folderPicker {
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
  [self dismiss];
}

#pragma mark - NoteModelBridgeObserver

- (void)noteModelLoaded {
  // The note model is assumed to be loaded when this controller is created.
  NOTREACHED();
}

- (void)noteNodeChanged:(const NoteNode*)noteNode {
  if (noteNode == self.parentFolder) {
    [self updateParentFolderState];
  }
}

- (void)noteNodeChildrenChanged:(const NoteNode*)noteNode {
  // No-op.
}

- (void)noteNode:(const NoteNode*)noteNode
     movedFromParent:(const NoteNode*)oldParent
            toParent:(const NoteNode*)newParent {
  if (_ignoresOwnMove)
    return;
  if (noteNode == self.folder) {
    DCHECK(oldParent == self.parentFolder);
    self.parentFolder = newParent;
    [self updateParentFolderState];
  }
}

- (void)noteNodeDeleted:(const NoteNode*)noteNode
                 fromFolder:(const NoteNode*)folder {
  if (noteNode == self.parentFolder) {
    self.parentFolder = NULL;
    [self updateParentFolderState];
    return;
  }
  if (noteNode == self.folder) {
    self.folder = NULL;
    self.editingExistingFolder = NO;
    [self updateEditingState];
  }
}

- (void)noteModelRemovedAllNodes {
  if (self.noteModel->is_permanent_node(self.parentFolder))
    return;  // The current parent folder is still valid.

  self.parentFolder = NULL;
  [self updateParentFolderState];
}

#pragma mark - NoteTextFieldItemDelegate

- (void)textDidChangeForItem:(NoteTextFieldItem*)item {
  self.modalInPresentation = YES;
  [self updateSaveButtonState];
}

- (void)textFieldDidBeginEditing:(UITextField*)textField {
 // textField.textColor = [NoteTextFieldCell textColorForEditing:YES];
}

- (void)textFieldDidEndEditing:(UITextField*)textField {
//  textField.textColor = [NoteTextFieldCell textColorForEditing:NO];
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeParentFolder) {
    [self changeParentFolder];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                           title:nil
                         message:nil
                   barButtonItem:self.navigationItem.leftBarButtonItem];

  __weak __typeof(self) weakSelf = self;
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_SAVE_CHANGES)
                action:^{
                  [weakSelf saveFolder];
                }
                 style:UIAlertActionStyleDefault];
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_DISCARD_CHANGES)
                action:^{
                  [weakSelf dismiss];
                }
                 style:UIAlertActionStyleDestructive];
  // IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES)
                action:^{
                  weakSelf.navigationItem.leftBarButtonItem.enabled = YES;
                  weakSelf.navigationItem.rightBarButtonItem.enabled = YES;
                }
                 style:UIAlertActionStyleCancel];

  self.navigationItem.leftBarButtonItem.enabled = NO;
  self.navigationItem.rightBarButtonItem.enabled = NO;
  [self.actionSheetCoordinator start];
}

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  // Resign first responder if trying to dismiss the VC so the keyboard doesn't
  // linger until the VC dismissal has completed.
  [self.view endEditing:YES];
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismiss];
}

#pragma mark - Private

- (void)setParentFolder:(const NoteNode*)parentFolder {
  if (!parentFolder) {
    parentFolder = self.noteModel->main_node();
  }
  _parentFolder = parentFolder;
}

- (void)updateEditingState {
  if (![self isViewLoaded])
    return;

  self.view.accessibilityIdentifier =
      (self.folder) ? kNoteFolderEditViewContainerIdentifier
                    : kNoteFolderCreateViewContainerIdentifier;

  [self setTitle:(self.folder)
                     ? l10n_util::GetNSString(
                           IDS_VIVALDI_NOTE_NEW_GROUP_EDITOR_EDIT_TITLE)
                     : l10n_util::GetNSString(
                           IDS_VIVALDI_NOTE_NEW_GROUP_EDITOR_CREATE_TITLE)];
}

- (void)updateParentFolderState {
  NSIndexPath* folderSelectionIndexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeParentFolder
                              sectionIdentifier:SectionIdentifierInfo];
  self.parentFolderItem.title =
      note_utils_ios::TitleForNoteNode(self.parentFolder);
  [self.tableView reloadRowsAtIndexPaths:@[ folderSelectionIndexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];

  if (self.editingExistingFolder && self.navigationController.isToolbarHidden)
    [self addToolbar];

  if (!self.editingExistingFolder && !self.navigationController.isToolbarHidden)
    self.navigationController.toolbarHidden = YES;
}

- (void)setupCollectionViewModel {
  [self loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierInfo];

  NoteTextFieldItem* titleItem =
      [[NoteTextFieldItem alloc] initWithType:ItemTypeFolderTitle];
  titleItem.text =
      (self.folder)
          ? note_utils_ios::TitleForNoteNode(self.folder)
          : l10n_util::GetNSString(IDS_VIVALDI_NOTE_NEW_GROUP_DEFAULT_NAME);
  titleItem.placeholder =
      l10n_util::GetNSString(IDS_VIVALDI_NOTE_NEW_EDITOR_NAME_LABEL);
  titleItem.accessibilityIdentifier = @"Title";
  [self.tableViewModel addItem:titleItem
       toSectionWithIdentifier:SectionIdentifierInfo];
  titleItem.delegate = self;
  self.titleItem = titleItem;

  NoteParentFolderItem* parentFolderItem =
      [[NoteParentFolderItem alloc] initWithType:ItemTypeParentFolder];
  parentFolderItem.title =
      note_utils_ios::TitleForNoteNode(self.parentFolder);
  [self.tableViewModel addItem:parentFolderItem
       toSectionWithIdentifier:SectionIdentifierInfo];
  self.parentFolderItem = parentFolderItem;
}

- (void)addToolbar {
  self.navigationController.toolbarHidden = NO;
  NSString* titleString = l10n_util::GetNSString(IDS_VIVALDI_NOTE_GROUP_DELETE);
  UIBarButtonItem* deleteButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(deleteFolder)];
  deleteButton.accessibilityIdentifier =
      kNoteFolderEditorDeleteButtonIdentifier;
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  deleteButton.tintColor = [UIColor colorNamed:kRedColor];

  [self setToolbarItems:@[ spaceButton, deleteButton, spaceButton ]
               animated:NO];
}

- (void)updateSaveButtonState {
  self.doneItem.enabled = (self.titleItem.text.length > 0);
}

@end
