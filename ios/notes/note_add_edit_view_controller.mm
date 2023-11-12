// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/notes/note_add_edit_view_controller.h"

#include <memory>
#include <set>

#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/ios/block_types.h"
#import "base/mac/foundation_util.h"

#import "base/mac/scoped_cftyperef.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/notes/notes_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/notes/note_folder_view_controller.h"
#import "ios/notes/note_mediator.h"
#import "ios/notes/note_model_bridge_observer.h"
#import "ios/notes/note_ui_constants.h"
#import "ios/notes/note_utils_ios.h"
#import "ios/notes/cells/note_parent_folder_item.h"
#import "ios/notes/cells/note_text_field_item.h"
#import "ios/notes/cells/note_text_view_item.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using vivaldi::NotesModel;
using vivaldi::NoteNode;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInfo = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeName = kItemTypeEnumZero,
  ItemTypeFolder,
};

// Estimated Table Row height.
const CGFloat kEstimatedTableRowHeight = 50;
// Estimated TableSection Footer height.
const CGFloat kEstimatedTableSectionFooterHeight = 40;
}  // namespace

@interface NoteAddEditViewController () <NoteFolderViewControllerDelegate,
                                          NoteModelBridgeObserver,
                                          UITextViewDelegate,
                                          NoteTextFieldItemDelegate,
                                          NoteTextViewItemDelegate> {
  // Flag to ignore note model changes notifications.
  BOOL _ignoresNotesModelChanges;

  std::unique_ptr<notes::NoteModelBridge> _modelBridge;
}

// The note this controller displays or edits.
// Redefined to be readwrite.
@property(nonatomic, assign) const NoteNode* note;

// Reference to the note model.
@property(nonatomic, assign) NotesModel* noteModel;

// The parent of the note. This may be different from |note->parent()|
// if the changes have not been saved yet. |folder| then represents the
// candidate for the new parent of |note|.  This property is always a
// non-NULL, valid folder.
@property(nonatomic, assign) const NoteNode* folder;

// The folder picker view controller.
// Redefined to be readwrite. // Not currently changeble from within
@property(nonatomic, strong) NoteFolderViewController* folderViewController;

@property(nonatomic, assign) Browser* browser;

@property(nonatomic, assign) ChromeBrowserState* browserState;

// Cancel button item in navigation bar.
@property(nonatomic, strong) UIBarButtonItem* cancelItem;

// Done button item in navigation bar.
@property(nonatomic, strong) UIBarButtonItem* doneItem;

// CollectionViewItem-s from the collection.
@property(nonatomic, strong) NoteTextViewItem* contentItem;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

@property(nonatomic, assign) BOOL editingExistingItem;

// Reports the changes to the delegate, that has the responsibility to save the
// note.
- (void)commitNoteChanges;

// Changes |self.folder| and updates the UI accordingly.
// The change is not committed until the user taps the Save button.
- (void)changeFolder:(const NoteNode*)folder;

// The Save button is disabled if the form values are deemed non-valid. This
// method updates the state of the Save button accordingly.
- (void)updateSaveButtonState;

// Reloads the folder label text.
- (void)updateFolderLabel;

// Populates the UI with information from the models.
- (void)updateUIFromNote;

// Called when the Delete button is pressed.
- (void)deleteNote;

// Called when the Folder button is pressed.
- (void)moveNote;

// Called when the Cancel button is pressed.
- (void)cancel;

// Called when the Done button is pressed.
- (void)save;

@end

#pragma mark

@implementation NoteAddEditViewController

@synthesize note = _note;
@synthesize noteModel = _noteModel;
@synthesize delegate = _delegate;
@synthesize folder = _folder;
@synthesize folderViewController = _folderViewController;
@synthesize browser = _browser;
@synthesize browserState = _browserState;
@synthesize cancelItem = _cancelItem;
@synthesize doneItem = _doneItem;
@synthesize contentItem = _contentItem;
@synthesize editingExistingItem = _editingExistingItem;

#pragma mark - Lifecycle

- (instancetype)initWithNote:(const NoteNode*)note
                      parent:(const NoteNode*)parent
                     browser:(Browser*)browser {
  DCHECK(note || parent);
  DCHECK(browser);
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    // Browser may be OTR, which is why the original browser state is being
    // explicitly requested.
    _browser = browser;
    _browserState = browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _note = note;
    _folder = parent;
    _noteModel =
        vivaldi::NotesModelFactory::GetForBrowserState(_browserState);

    if (note) {
      _folder = _note->parent();
      _editingExistingItem = YES;
    }

    // Set up the note model oberver.
    _modelBridge.reset(
        new notes::NoteModelBridge(self, _noteModel));
  }
  return self;
}

- (void)textViewDidChange:(UITextView*)textView {
    _contentItem.text = textView.text;
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
  _folderViewController.delegate = nil;
}

#pragma mark View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.backgroundColor = self.styler.tableViewBackgroundColor;
  self.tableView.estimatedRowHeight = kEstimatedTableRowHeight;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedSectionFooterHeight =
      kEstimatedTableSectionFooterHeight;
  self.view.accessibilityIdentifier = kNoteAddEditViewContainerIdentifier;

  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kNoteCellHorizontalLeadingInset,
                                         0, 0)];
  if (self.note)
    self.title = l10n_util::GetNSString(IDS_VIVALDI_NOTE_EDIT_SCREEN_TITLE);
  else
    self.title = l10n_util::GetNSString(IDS_VIVALDI_NOTE_NEW_NOTE);

  self.navigationItem.hidesBackButton = YES;

  UIBarButtonItem* cancelItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancel)];
  cancelItem.accessibilityIdentifier = @"Cancel";
  self.navigationItem.leftBarButtonItem = cancelItem;
  self.cancelItem = cancelItem;

  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(save)];
  doneItem.accessibilityIdentifier =
      kNoteEditNavigationBarDoneButtonIdentifier;
  self.navigationItem.rightBarButtonItem = doneItem;
  self.doneItem = doneItem;

  // Setup the bottom toolbar.
  NSString* titleString = l10n_util::GetNSString(IDS_VIVALDI_NOTE_DELETE);
  UIBarButtonItem* deleteButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(deleteNote)];
  deleteButton.accessibilityIdentifier = kNoteEditDeleteButtonIdentifier;
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  deleteButton.tintColor = [UIColor colorNamed:kRedColor];

  [self setToolbarItems:@[ spaceButton, deleteButton, spaceButton ]
               animated:NO];

  [self updateUIFromNote];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Whevener this VC is displayed the bottom toolbar will be shown.
  self.navigationController.toolbarHidden = NO;
}

#pragma mark - Presentation controller integration

- (BOOL)shouldBeDismissedOnTouchOutside {
  return NO;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self cancel];
  return YES;
}

#pragma mark - Private

// Retrieves input note name string from UI.
- (NSString*)inputNoteName {
    return self.contentItem.text;
}

- (void)commitNoteChanges {
  // To stop getting recursive events from committed note editing changes
  // ignore note model updates notifications.
  base::AutoReset<BOOL> autoReset(&_ignoresNotesModelChanges, YES);

  // Tell delegate if note name or title has been changed.
  if (self.note &&
      self.note->GetTitle() !=
           base::SysNSStringToUTF16([self inputNoteName]) ) {
    [self.delegate noteEditorWillCommitContentChange:self];
  }
  [self.snackbarCommandsHandler
      showSnackbarMessage:
          note_utils_ios::CreateOrUpdateNoteWithUndoToast(
                self.note, [self inputNoteName], GURL(),
                self.folder, self.noteModel, self.browserState)];
}

- (void)changeFolder:(const NoteNode*)folder {
  DCHECK(folder->is_folder());
  self.folder = folder;
  [NoteMediator setFolderForNewNotes:self.folder
                              inBrowserState:self.browserState];
  [self updateFolderLabel];
}

- (void)dismiss {
  [self.view endEditing:YES];

  // Dismiss this controller.
  [self.delegate noteEditorWantsDismissal:self];
}

#pragma mark - Layout

- (void)updateSaveButtonState {
    self.doneItem.enabled = YES;
}

- (void)updateFolderLabel {
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeFolder
                              sectionIdentifier:SectionIdentifierInfo];
  if (!indexPath) {
    return;
  }

  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];
}

- (void)updateUIFromNote {
  [self loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierInfo];
  self.contentItem = [[NoteTextViewItem alloc] initWithType:ItemTypeName];
  self.contentItem.accessibilityIdentifier = @"Title Field";
  self.contentItem.placeholder =
      l10n_util::GetNSString(IDS_VIVALDI_NOTE_NAME_FIELD_HEADER);
  if (self.note)
      self.contentItem.text = note_utils_ios::TitleForNoteNode(self.note);
  self.contentItem.delegate = self;
  [model addItem:self.contentItem toSectionWithIdentifier:SectionIdentifierInfo];
  // Save button state.
  [self updateSaveButtonState];
}

#pragma mark - Actions

- (void)deleteNote {
  if (self.note && self.noteModel->loaded()) {
    // To stop getting recursive events from committed note editing changes
    // ignore note model updates notifications.
    base::AutoReset<BOOL> autoReset(&_ignoresNotesModelChanges, YES);

    std::set<const NoteNode*> nodes;
    nodes.insert(self.note);
    [self.snackbarCommandsHandler
        showSnackbarMessage:note_utils_ios::DeleteNotesWithUndoToast(
                                nodes, self.noteModel, self.browserState)];
    self.note = nil;
  }
  [self.delegate noteEditorWantsDismissal:self];
}

- (void)moveNote {
  DCHECK(self.noteModel);
  DCHECK(!self.folderViewController);

  std::set<const NoteNode*> editedNodes;
  editedNodes.insert(self.note);
  NoteFolderViewController* folderViewController =
      [[NoteFolderViewController alloc]
          initWithNotesModel:self.noteModel
               allowsNewFolders:YES
                    editedNodes:editedNodes
                   allowsCancel:NO
                 selectedFolder:self.folder
                        browser:_browser];
  folderViewController.delegate = self;
  folderViewController.snackbarCommandsHandler = self.snackbarCommandsHandler;
  self.folderViewController = folderViewController;
  self.folderViewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  [self.navigationController pushViewController:self.folderViewController
                                       animated:YES];
}

- (void)cancel {
  [self dismiss];
}

- (void)save {
  [self commitNoteChanges];
  [self dismiss];
}

#pragma mark - NoteTextFieldItemDelegate

- (void)textDidChangeForItem:(NoteTextFieldItem*)item {
  self.modalInPresentation = YES;
  [self updateSaveButtonState];
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - NoteTextViewItemDelegate

- (void)textDidChangeForTextViewItem:(NoteTextViewItem*)item {
  self.modalInPresentation = YES;
  [self updateSaveButtonState];
}

- (BOOL)textFieldLongShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  UITableViewCell* cell =
      [super tableView:tableView cellForRowAtIndexPath:indexPath];
  NSInteger type = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (type) {
    case ItemTypeName:
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    case ItemTypeFolder:
      break;
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] == ItemTypeFolder)
    [self moveNote];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView =
      [super tableView:tableView viewForFooterInSection:section];
  if (section ==
      [self.tableViewModel sectionForSectionIdentifier:SectionIdentifierInfo]) {
    UITableViewHeaderFooterView* headerFooterView =
        base::mac::ObjCCastStrict<UITableViewHeaderFooterView>(footerView);
    headerFooterView.textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    headerFooterView.textLabel.textColor = [UIColor colorNamed:kRedColor];
  }
  return footerView;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
    return tableView.bounds.size.height;
}

#pragma mark - NoteFolderViewControllerDelegate

- (void)folderPicker:(NoteFolderViewController*)folderPicker
    didFinishWithFolder:(const NoteNode*)folder {
  [self changeFolder:folder];
  // This delegate method can be called on two occasions:
  // - the user selected a folder in the folder picker. In that case, the folder
  // picker should be popped;
  // - the user created a new folder, in which case the navigation stack
  // contains this note editor (|self|), a folder picker and a folder
  // creator. In such a case, both the folder picker and creator shoud be popped
  // to reveal this note editor. Thus the call to
  // |popToViewController:animated:|.
  [self.navigationController popToViewController:self animated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

- (void)folderPickerDidCancel:(NoteFolderViewController*)folderPicker {
  // This delegate method can only be called from the folder picker, which is
  // the only view controller on top of this note editor (|self|). Thus the
  // call to |popViewControllerAnimated:|.
  [self.navigationController popViewControllerAnimated:YES];
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}

- (void)folderPickerDidDismiss:(NoteFolderViewController*)folderPicker {
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
  [self dismiss];
}

#pragma mark - NotesModelBridgeObserver

- (void)noteModelLoaded {
  // No-op.
}

- (void)noteNodeChanged:(const NoteNode*)noteNode {
  if (_ignoresNotesModelChanges)
    return;

  if (self.note == noteNode)
    [self updateUIFromNote];
}

- (void)noteNodeChildrenChanged:(const NoteNode*)noteNode {
  if (_ignoresNotesModelChanges)
    return;

  [self updateFolderLabel];
}

- (void)noteNode:(const NoteNode*)noteNode
     movedFromParent:(const NoteNode*)oldParent
            toParent:(const NoteNode*)newParent {
  if (_ignoresNotesModelChanges)
    return;

  if (self.note == noteNode)
    [self.folderViewController changeSelectedFolder:newParent];
}

- (void)noteNodeDeleted:(const NoteNode*)noteNode
                 fromFolder:(const NoteNode*)folder {
  if (_ignoresNotesModelChanges)
    return;

  if (self.note == noteNode) {
    self.note = nil;
    [self.delegate noteEditorWantsDismissal:self];
  } else if (self.folder == noteNode) {
    [self changeFolder:self.noteModel->root_node()];
  }
}

- (void)noteModelRemovedAllNodes {
  if (_ignoresNotesModelChanges)
    return;

  self.note = nil;
  if (!self.noteModel->is_permanent_node(self.folder)) {
    [self changeFolder:self.noteModel->root_node()];
  }

  [self.delegate noteEditorWantsDismissal:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                           title:nil
                         message:nil
                   barButtonItem:self.cancelItem];

  __weak __typeof(self) weakSelf = self;
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_SAVE_CHANGES)
                action:^{
                  [weakSelf save];
                }
                 style:UIAlertActionStyleDefault];
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_DISCARD_CHANGES)
                action:^{
                  [weakSelf cancel];
                }
                 style:UIAlertActionStyleDestructive];
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

#pragma mark - UIResponder

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  [self dismiss];
}

@end
