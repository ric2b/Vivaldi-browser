// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_add_edit_view_controller.h"

#import <memory>
#import <set>

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_cftyperef.h"
#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/notes/notes_factory.h"
#import "ios/ui/custom_views/vivaldi_text_view.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/notes/cells/note_parent_folder_item.h"
#import "ios/ui/notes/note_folder_chooser_view_controller.h"
#import "ios/ui/notes/note_mediator.h"
#import "ios/ui/notes/note_model_bridge_observer.h"
#import "ios/ui/notes/note_ui_constants.h"
#import "ios/ui/notes/note_utils_ios.h"
#import "ios/ui/notes/note_mediator.h"
#import "ios/ui/notes/note_model_bridge_observer.h"
#import "ios/ui/notes/note_parent_folder_view.h"
#import "ios/ui/notes/note_ui_constants.h"
#import "ios/ui/notes/note_utils_ios.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using vivaldi::NotesModel;
using vivaldi::NoteNode;

namespace {

UIEdgeInsets noteTextViewPadding = UIEdgeInsetsMake(8, 8, 0, 8);
CGFloat noteTextViewBottomPadding = 12;

// Padding for the body container view
UIEdgeInsets bodyContainerViewPadding = UIEdgeInsetsMake(12, 12, 0, 12);
CGFloat bodyContainerCornerRadius = 6;
}  // namespace

@interface NoteAddEditViewController () <NoteFolderChooserViewControllerDelegate,
                                        NoteModelBridgeObserver,
                                        UITextViewDelegate> {
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
@property(nonatomic, strong) NoteFolderChooserViewController*
    folderViewController;

@property(nonatomic, assign) Browser* browser;

@property(nonatomic, assign) ChromeBrowserState* browserState;

// Cancel button item in navigation bar.
@property(nonatomic, strong) UIBarButtonItem* cancelItem;

// Done button item in navigation bar.
@property(nonatomic, strong) UIBarButtonItem* doneItem;

// The note text content view
@property(nonatomic, weak) VivaldiTextView* noteTextView;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// Editing an existing item
@property(nonatomic, assign) BOOL editingExistingItem;

// A view that holds the parent folder details
@property(nonatomic,weak) NoteParentFolderView* parentFolderView;

// Should the controller setup Cancel and Done buttons instead of a back button.
@property(nonatomic, assign) BOOL allowsCancel;

// Reports the changes to the delegate, that has the responsibility to save the
// note.
- (void)commitNoteChanges;

// Changes |self.folder| and updates the UI accordingly.
// The change is not committed until the user taps the Save button.
- (void)changeFolder:(const NoteNode*)folder;

// The Save button is disabled if the form values are deemed non-valid. This
// method updates the state of the Save button accordingly.
- (void)updateSaveButtonState;

// Populates the UI with information from the models.
- (void)updateUIFromNote;

// Called when the Delete button is pressed.
- (void)deleteNote;

- (void)moveNodesToTrash;

// Called when the Folder button is pressed.
- (void)moveNote;

// Called when the Cancel button is pressed.
- (void)cancel;

// Called when the Done button is pressed.
- (void)saveNote;

// Bottom constraint for the note text view.
@property (nonatomic, strong) NSLayoutConstraint *noteTextViewBottomConstraint;

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
@synthesize editingExistingItem = _editingExistingItem;
@synthesize parentFolderView = _parentFolderView;
@synthesize noteTextView = _noteTextView;
@synthesize allowsCancel = _allowsCancel;
@synthesize noteTextViewBottomConstraint = _noteTextViewBottomConstraint;

#pragma mark - Lifecycle

+ (instancetype)initWithBrowser:(Browser*)browser
                           item:(const NoteNode*)note
                         parent:(const NoteNode*)parent
                      isEditing:(BOOL)isEditing
                   allowsCancel:(BOOL)allowsCancel {
  DCHECK(note || parent);
  DCHECK(browser);
  NoteAddEditViewController* controller = [[NoteAddEditViewController alloc]
                                           initWithBrowser:browser];
  controller.note = note;
  controller.folder = parent;
  controller.allowsCancel = allowsCancel;
  if (controller.note) {
    controller.folder = note->parent();
    controller.editingExistingItem = YES;
  } else
    controller.editingExistingItem = NO;
  return controller;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super init];
  if (self) {
    // Browser may be OTR, which is why the original browser state is being
    // explicitly requested.
    _browser = browser;
    _browserState = browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _noteModel =
      vivaldi::NotesModelFactory::GetForBrowserState(_browserState);

    // Set up the note model oberver.
    _modelBridge.reset(
      new notes::NoteModelBridge(self, _noteModel));
  }
  return self;
}

- (void)dealloc {
  _folderViewController.delegate = nil;
}

#pragma mark View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setUpUI];
  [self updateNoteUI];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Whevener this VC is displayed the bottom toolbar will be shown.
  self.navigationController.toolbarHidden = NO;
  [self updateFolderState];
}

/// Updates the textfields if editing an item, and the parent folder components.
- (void)updateNoteUI {
  if (self.note) {
    [self updateUIFromNote];
  } else {
    [self.noteTextView setFocus];
  }
  // Update the textfield with folder title if in editing mode
  if (self.editingExistingItem) {
    [self updateFolderState];
  }
}

- (void)setUpUI {
  self.view.backgroundColor =
    [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  // Disable interactive dismissal of the view controller.
  // This prevents accidental close by slide in gesture or any other reason.
  // Note editor should be closed only by button tap to make sure users
  // do not lose any text on accidental close.
  self.modalInPresentation = true;
  self.view.accessibilityIdentifier = kNoteAddEditViewContainerIdentifier;
  [self setupKeyboardObservers];
  [self setUpNavBarView];
  [self setupContentView];
  [self setupToolbar];
}

-(void)setUpNavBarView {
  // Set up navigation bar
  if (self.note)
    self.title = l10n_util::GetNSString(IDS_VIVALDI_NOTE_EDIT_SCREEN_TITLE);
  else
    self.title = l10n_util::GetNSString(IDS_VIVALDI_NOTE_NEW_NOTE);

  self.navigationItem.hidesBackButton = YES;

  if (self.allowsCancel) {
    UIBarButtonItem* cancelItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancel)];
    cancelItem.accessibilityIdentifier = @"Cancel";
    self.navigationItem.leftBarButtonItem = cancelItem;
    self.cancelItem = cancelItem;
  }

  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
    initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                         target:self
                         action:@selector(saveNote)];
  doneItem.accessibilityIdentifier =
    kNoteEditNavigationBarDoneButtonIdentifier;
  self.navigationItem.rightBarButtonItem = doneItem;
  self.doneItem = doneItem;
}

- (void)setupToolbar{
  // Setup the bottom toolbar.
  NSString* moveTitleString = l10n_util::GetNSString(IDS_VIVALDI_NOTE_MOVE);
  UIBarButtonItem* moveButton =
    [[UIBarButtonItem alloc] initWithTitle:moveTitleString
                                    style:UIBarButtonItemStylePlain
                                   target:self
                                   action:@selector(moveNote)];
  moveButton.accessibilityIdentifier = kNoteEditDeleteButtonIdentifier;
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
    initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                         target:nil
                         action:nil];
  moveButton.tintColor = [UIColor colorNamed:kBlueColor];

  NSString* titleString = l10n_util::GetNSString(IDS_VIVALDI_NOTE_DELETE);
  UIBarButtonItem* deleteButton =
  [[UIBarButtonItem alloc] initWithTitle:titleString
                                   style:UIBarButtonItemStylePlain
                                  target:self
                                  action:@selector(deleteNote)];
  deleteButton.accessibilityIdentifier = kNoteEditDeleteButtonIdentifier;
  deleteButton.tintColor = [UIColor colorNamed:kRedColor];
  [self setToolbarItems:@[ moveButton, spaceButton, deleteButton ]
               animated:NO];
}

-(void)setupContentView {
  // Set up views
  UIView* bodyContainerView = [UIView new];
  bodyContainerView.backgroundColor =
    [UIColor colorNamed: kGroupedSecondaryBackgroundColor];
  bodyContainerView.layer.cornerRadius = bodyContainerCornerRadius;
  bodyContainerView.clipsToBounds = YES;

  [self.view addSubview:bodyContainerView];
  [bodyContainerView
    fillSuperviewToSafeAreaInsetWithPadding:bodyContainerViewPadding];

  // Note text view
  VivaldiTextView* noteTextView = [[VivaldiTextView alloc] init];
  _noteTextView = noteTextView;

  [bodyContainerView addSubview:self.noteTextView];
  // Add anchoring of note textView
  [noteTextView anchorTop:bodyContainerView.topAnchor
                  leading:bodyContainerView.leadingAnchor
                   bottom:nil
                 trailing:bodyContainerView.trailingAnchor
                  padding:noteTextViewPadding];

  self.noteTextViewBottomConstraint =
    [noteTextView.bottomAnchor
     constraintEqualToAnchor:bodyContainerView.bottomAnchor
     constant:-noteTextViewBottomPadding];
  [self.noteTextViewBottomConstraint setActive:YES];
}

- (void)setupKeyboardObservers {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(handleKeyboardNotification:)
                        name:UIKeyboardWillShowNotification
                      object:nil];

  [defaultCenter addObserver:self
                    selector:@selector(handleKeyboardNotification:)
                        name:UIKeyboardWillHideNotification
                      object:nil];
}

- (void)removeKeyboardObservers {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter removeObserver:self
                           name:UIKeyboardWillShowNotification
                         object:nil];
  [defaultCenter removeObserver:self
                           name:UIKeyboardWillHideNotification
                         object:nil];
}

- (void)handleKeyboardNotification:(NSNotification *)notification {
  // Extract the keyboard frame from the notification's user info
  NSDictionary* userInfo = [notification userInfo];
  CGRect keyboardFrame =
    [[userInfo objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
  // Determine if the keyboard is showing based on the notification name
  BOOL isKeyboardShowing =
    [notification.name isEqualToString:UIKeyboardWillShowNotification];

  // The constant we're changing when the keyboard state changes.
  // 1: If the keyboard is closed, the constant is noteTextViewBottomPadding.
  // 2: If the keyboard is open, the constant is calculated to position the
  // view just above the keyboard.

  // Calculate the bottom padding, which should include the bottom safe area
  // height.
  // This is necessary to account for the space occupied by the bottom toolbar
  // with the 'Delete' button.
  // However, modal presentations on iPads have extra bottom padding apart from
  // the safe area.
  // In this case, we need to calculate the bottom padding based on the window
  // size and current view size.
  CGFloat bottomPadding = 0;
  UIWindow *window = self.view.window;
  if (window) {
    // Convert the view's bounds to window coordinates
    CGRect viewFrameInWindow =
      [self.view convertRect:self.view.bounds toView:window];
    // Calculate the gap between the bottom of the window and the bottom of
    // the view
    CGFloat visibleGap =
      CGRectGetHeight(window.bounds) - CGRectGetMaxY(viewFrameInWindow);
    // If there is a gap (for modally presented views on iPad), add it to the
    // bottom padding
    if (visibleGap > 0) {
      bottomPadding = visibleGap + self.view.safeAreaInsets.bottom;
    } else {
      // If there's no gap (other cases), just use the bottom safe area inset
      bottomPadding = self.view.safeAreaInsets.bottom;
    }
  } else {
    // If there's no window (shouldn't happen, but better to be safe), use the
    // bottom safe area inset
    bottomPadding = self.view.safeAreaInsets.bottom;
  }

  // Calculate the total height used by the layout, including the height of
  // the keyboard,
  // the bottom padding and the note text view bottom padding
  CGFloat usedHeight = bottomPadding + noteTextViewBottomPadding;

  // Determine the constant for the bottom constraint of the note text view
  CGFloat keyboardVisibleConstant = 0;
  // If the used height is greater or equal to the height of the keyboard,
  // just use the note text view bottom padding. Otherwise, calculate the extra
  // padding needed.
  if (usedHeight >= keyboardFrame.size.height) {
    keyboardVisibleConstant = noteTextViewBottomPadding;
  } else {
    keyboardVisibleConstant = keyboardFrame.size.height - usedHeight;
  }

  // Set the new constant for the note text view's bottom constraint
  // If the keyboard is showing, the constant is negative to move the view up.
  // Otherwise, it's the negative of noteTextViewBottomPadding
  self.noteTextViewBottomConstraint.constant =
    isKeyboardShowing ? -keyboardVisibleConstant : -noteTextViewBottomPadding;

  // Animate the change in the view's frame
  [UIView animateWithDuration:0
                        delay:0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
    [self.view layoutIfNeeded];
  } completion: nil];
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
  return self.noteTextView.getText;
}

- (void)commitNoteChanges {
  // To stop getting recursive events from committed note editing changes
  // ignore note model updates notifications.
  base::AutoReset<BOOL> autoReset(&_ignoresNotesModelChanges, YES);

  if (self.editingExistingItem && self.note) {
    // Tell delegate if note name or title has been changed.
    if (self.note &&
        self.note->GetTitle() !=
        base::SysNSStringToUTF16([self inputNoteName]) ) {
      [self.delegate noteEditorWillCommitContentChange:self];
    }
    [self.snackbarCommandsHandler
     showSnackbarMessage:
       note_utils_ios::CreateOrUpdateNoteWithToast(
         self.note, [self inputNoteName], GURL(),
         self.folder, self.noteModel, self.browserState)];
  } else {
    std::u16string folderTitle =
    l10n_util::GetStringUTF16(IDS_VIVALDI_NOTE_CONTEXT_BAR_NEW_NOTE);
    std::u16string titleString =
    base::SysNSStringToUTF16([self inputNoteName]);
    self.noteModel->AddNote(self.folder,
                            self.folder->children().size(),
                            titleString,
                            GURL(),
                            titleString);
  }
}

- (void)changeFolder:(const NoteNode*)folder {
  DCHECK(folder->is_folder());
  self.folder = folder;
  [NoteMediator setFolderForNewNotes:self.folder
                      inBrowserState:self.browserState];
}

/// Updates the parent folder view componets, i.e. title and icon.
- (void)updateFolderState {
  [self.parentFolderView setParentFolderAttributesWithItem:self.folder];
}

- (void)dismiss {
  [self.view endEditing:YES];
  // Dismiss this controller.
  [self cancel];
  [self removeKeyboardObservers];
  [self.delegate noteEditorWantsDismissal:self];
}

#pragma mark - Layout

- (void)updateSaveButtonState {
  self.doneItem.enabled = YES;
}

- (void)updateUIFromNote {
  self.noteTextView.text = note_utils_ios::TitleForNoteNode(self.note);
  // Save button state.
  [self updateSaveButtonState];
}

#pragma mark - Actions

- (void)deleteNote {
  if (self.note) {
    if (!self.folder->is_trash()) {
      [self moveNodesToTrash];
    } else {
      if (self.note && self.noteModel->loaded()) {
        // To stop getting recursive events from committed note editing
        // changes, ignore note model updates notifications.
        base::AutoReset<BOOL> autoReset(&_ignoresNotesModelChanges, YES);
        std::set<const NoteNode*> nodes;
        nodes.insert(self.note);
        [self.snackbarCommandsHandler
         showSnackbarMessage:note_utils_ios::DeleteNotesWithToast(
           nodes, self.noteModel, self.browserState)];
        self.note = nil;
      }
    }
  }
  [self cancel];
}

- (void)moveNodesToTrash {
  std::set<const vivaldi::NoteNode*> nodes;
  nodes.insert(self.note);
  DCHECK_GE(nodes.size(), 1u);

  const NoteNode* trashFolder = self.noteModel->trash_node();
  [self.snackbarCommandsHandler
   showSnackbarMessage:note_utils_ios::MoveNotesWithToast(
     nodes, self.noteModel, trashFolder,
     self.browserState)];
}

- (void)moveNote {
  DCHECK(self.noteModel);
  DCHECK(!self.folderViewController);

  std::set<const NoteNode*> editedNodes;
  editedNodes.insert(self.note);
  NoteFolderChooserViewController* folderViewController =
  [[NoteFolderChooserViewController alloc]
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
  self.allowsCancel = YES;
  if (self.allowsCancel) {
    [self dismissViewControllerAnimated:YES completion:nil];
  } else {
    [self.navigationController popViewControllerAnimated:YES];
  }
}

- (void)saveNote {
  [self commitNoteChanges];
  [self dismiss];
}

- (void)stop {
  self.folderViewController = nil;
  self.folderViewController.delegate = nil;
}

#pragma mark - NoteFolderChooserViewControllerDelegate

- (void)folderPicker:(NoteFolderChooserViewController*)folderPicker
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
  [self stop];
}

- (void)folderPickerDidCancel:(NoteFolderChooserViewController*)folderPicker {
  // This delegate method can only be called from the folder picker, which is
  // the only view controller on top of this note editor (|self|). Thus the
  // call to |popViewControllerAnimated:|.
  [self.navigationController popViewControllerAnimated:YES];
  [self stop];
}

- (void)folderPickerDidDismiss:(NoteFolderChooserViewController*)folderPicker {
  [self stop];
  [self dismiss];
}

#pragma mark - NotesModelBridgeObserver

- (void)noteModelLoaded {
  // No-op.
}

- (void)noteNodeChanged:(const NoteNode*)noteNode {
  if (_ignoresNotesModelChanges)
    return;

  if (self.note && self.note == noteNode)
    [self updateUIFromNote];
}

- (void)noteNodeChildrenChanged:(const NoteNode*)noteNode {
  if (_ignoresNotesModelChanges)
    return;
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
    [weakSelf saveNote];
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
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [self.view endEditing:YES];
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  [self dismiss];
}

@end
