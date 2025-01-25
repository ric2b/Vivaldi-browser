// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/notes/note_add_edit_folder_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/notes/notes_model.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/notes/notes_factory.h"
#import "ios/ui/custom_views/vivaldi_text_field_view.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/notes/note_parent_folder_view.h"
#import "ios/ui/notes/note_utils_ios.h"
#import "ios/ui/notes/note_ui_constants.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using vivaldi::NoteNode;

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

@interface NoteAddEditFolderViewController ()
                        <NoteParentFolderViewDelegate,
                        NoteFolderChooserViewControllerDelegate> {}
// Textview for note folder name
@property(nonatomic,weak) VivaldiTextFieldView* folderNameTextView;
// A view for holding the parent folder components
@property(nonatomic,weak) NoteParentFolderView* parentFolderView;
// View controller for folder selection.
@property(nonatomic, strong) NoteFolderChooserViewController*
    folderViewController;
// The note model used.
@property(nonatomic, assign) vivaldi::NotesModel* notes;
// The Browser in which notes are presented
@property(nonatomic, assign) Browser* browser;
// The user's profile used.
@property(nonatomic, assign) ProfileIOS* profile;
// The parent item of the child about to create/or going to be edited.
@property(nonatomic,assign) NoteNode* parentItem;
// The item that keeps track of the current parent selected. This only lives on
// this view and doesn't affect the source parent in case user changes
// the parent by folder selection option.
@property(nonatomic,assign) NoteNode* folderItem;
// The item about to be edited. This can be nil if adding a new item.
@property(nonatomic,assign) NoteNode* editingItem;
// A BOOL to keep track of whether an existing folder is being edited or a new
// folder creation is taking place.
@property(nonatomic, assign) BOOL editingExistingFolder;
// Should the controller setup Cancel and Done buttons instead of a back button.
@property(nonatomic, assign) BOOL allowsCancel;

@end

@implementation NoteAddEditFolderViewController

@synthesize folderNameTextView = _folderNameTextView;
@synthesize parentFolderView = _parentFolderView;
@synthesize folderViewController = _folderViewController;
@synthesize notes = _notes;
@synthesize browser = _browser;
@synthesize profile = _profile;
@synthesize parentItem = _parentItem;
@synthesize folderItem = _folderItem;
@synthesize editingItem = _editingItem;
@synthesize editingExistingFolder = _editingExistingFolder;
@synthesize allowsCancel = _allowsCancel;

#pragma mark - INITIALIZERS
+ (instancetype)initWithBrowser:(Browser*)browser
                           item:(const NoteNode*)item
                         parent:(const NoteNode*)parent
                      isEditing:(BOOL)isEditing
                   allowsCancel:(BOOL)allowsCancel {
  DCHECK(browser);
  NoteAddEditFolderViewController* controller =
    [[NoteAddEditFolderViewController alloc] initWithBrowser:browser];
  controller.editingItem = (NoteNode*)item;
  controller.parentItem = (NoteNode*)parent;

  if (item)
    controller.parentItem = controller.editingItem->parent();
  if (controller.notes->loaded() &&
      !controller.notes->is_permanent_node(item)) {
    controller.editingExistingFolder = isEditing;
  }
  controller.allowsCancel = allowsCancel;

  // Construct the folder item from the parent item
  controller.folderItem = controller.parentItem;
  return controller;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super init];
  if (self) {
    _browser = browser;
    _profile = _browser->GetProfile()->GetOriginalProfile();
    _notes = vivaldi::NotesModelFactory::GetForProfile(_profile);
  }
  return self;
}

- (void)dealloc {
  _folderViewController.delegate = nil;
}

#pragma mark - VIEW CONTROLLER LIFECYCLE
- (void)viewDidLoad {
  [super viewDidLoad];
  [self setUpUI];
  [self updateUIFromFolder];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if (self.editingExistingFolder)
    [self.navigationController setToolbarHidden:NO];
  [self updateFolderState];
}

#pragma mark - PRIVATE METHODS
/// Updates the textfields if editing an item, and the parent folder components.
- (void)updateUIFromFolder {
  if (self.editingExistingFolder && self.editingItem) {
    self.folderNameTextView.text =
      note_utils_ios::TitleForNoteNode(self.editingItem);
    return;
  }
  [self.folderNameTextView setFocus];
  [self updateFolderState];
}

- (void)changeFolder:(NoteNode*)folder {
  DCHECK(folder->is_folder());
  self.parentItem = folder;
  [self updateFolderState];
}

/// Updates the parent folder view componets, i.e. title and icon.
- (void)updateFolderState {
  [self.parentFolderView setParentFolderAttributesWithItem:self.parentItem];
}

- (void)dismiss {
  [self.view endEditing:YES];
  [self cancel];
}

#pragma mark - SET UP UI COMPONENTS

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
  self.title = [self titleForViewController];
  // Add Done button.
  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(saveFolder)];
  doneItem.accessibilityIdentifier =
      kNoteFolderEditNavigationBarDoneButtonIdentifier;
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

- (NSString*)titleForViewController {
  std::u16string title = base::SysNSStringToUTF16(
      l10n_util::GetNSString(IDS_IOS_ITEM_TYPE_FOLDER));
  return self.editingExistingFolder ?
      l10n_util::GetNSStringF(IDS_IOS_EDIT_ITEM_WITH_TYPE, title) :
      l10n_util::GetNSStringF(IDS_IOS_ADD_ITEM_WITH_TYPE, title);
}

/// Set up the content view
-(void)setupContentView {
  // The container view to hold the title and folder views
  UIView* bodyContainerView = [UIView new];
  bodyContainerView.backgroundColor =
    [UIColor colorNamed: kGroupedSecondaryBackgroundColor];
  bodyContainerView.layer.cornerRadius = vNoteBodyCornerRadius;
  bodyContainerView.clipsToBounds = YES;
  [self.view addSubview:bodyContainerView];

  [bodyContainerView anchorTop:self.view.safeTopAnchor
                       leading:self.view.safeLeftAnchor
                        bottom:self.view.safeBottomAnchor
                      trailing:self.view.safeRightAnchor
                       padding:bodyContainerViewPadding];

  NSString* folderTitleString =
    l10n_util::GetNSString(IDS_IOS_NOTE_FOLDER_TITLE);
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
                           size:CGSizeMake(0, vNoteTextViewHeight)];

  NSString* parentFolderString =
    l10n_util::GetNSString(IDS_IOS_NOTE_PARENT_FOLDER);
  NoteParentFolderView* parentFolderView =
    [[NoteParentFolderView alloc]
      initWithTitle:[parentFolderString uppercaseString]];
  _parentFolderView = parentFolderView;
  parentFolderView.delegate = self;
  [bodyContainerView addSubview:parentFolderView];
  [parentFolderView anchorTop:folderNameTextView.bottomAnchor
                      leading:bodyContainerView.leadingAnchor
                       bottom:bodyContainerView.bottomAnchor
                     trailing:bodyContainerView.trailingAnchor
                      padding:parentFolderViewPadding];
}

/// Adds toolbar at the bottom when an existing item is being edited
- (void)addToolbar {
  self.navigationController.toolbarHidden = NO;
  NSString* titleString = l10n_util::GetNSString(IDS_IOS_DELETE_FOLDER);
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

/// Executes when the done button is tapped.
- (void)saveFolder {
  if (!self.notes->loaded())
    return;
  DCHECK(self.folderItem);
  NSString *folderString = self.folderNameTextView.getText;
  if (!self.folderNameTextView.hasText)
    return;

  std::u16string folderTitle = base::SysNSStringToUTF16(folderString);
  if (self.editingExistingFolder && self.editingItem) {
    DCHECK(self.editingItem);
    // Update title if changed
    if (self.editingItem->GetTitle() != folderTitle) {
      self.notes->SetTitle(self.editingItem,
                               folderTitle);
    }
    // Move folder if changed
    if (self.editingItem && self.editingItem->parent() && self.folderItem &&
        self.editingItem->parent()->id() != self.folderItem->id()) {
      std::set<const NoteNode*> editedNodes;
      editedNodes.insert(self.editingItem);
      note_utils_ios::MoveNotes(editedNodes,
                                          self.notes,
                                          self.folderItem);
    }
    [self notifyDelegateWithFolder:self.folderItem];
  } else {
    // Store the folder
    const NoteNode* folder =
      self.notes->AddFolder(self.folderItem,
                            self.folderItem->children().size(),
                                folderTitle);
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
  if (!self.notes->loaded())
    return;
  DCHECK(self.editingExistingFolder);
  DCHECK(self.editingItem);
  std::set<const NoteNode*> editedNodes;
  editedNodes.insert(self.editingItem);
  note_utils_ios::DeleteNotes(editedNodes, self.notes);
  [self notifyDelegateWithFolder:nil];
}

/// Notify the listeners that the note model is updated and associated UI
/// should be refreshed.
- (void)notifyDelegateWithFolder:(const NoteNode*)folder {
  if (self.delegate) {
    [self.delegate didCreateNewFolder:folder];
  }
  [self.view endEditing:YES];
  [self cancel];
}

#pragma mark - NoteParentFolderViewDelegate

- (void) didTapParentFolder {
  if (!self.notes->loaded() || !self.parentItem)
    return;
  std::set<const NoteNode*> editedNodes;

  if (self.editingItem)
    editedNodes.insert(self.editingItem);

  NoteFolderChooserViewController* folderViewController =
      [[NoteFolderChooserViewController alloc]
          initWithNotesModel:self.notes
               allowsNewFolders:YES
                    editedNodes:editedNodes
                   allowsCancel:NO
                 selectedFolder:self.folderItem
                        browser:self.browser];
  folderViewController.delegate = self;

  self.folderViewController = folderViewController;
  [self.navigationController pushViewController:folderViewController
                                       animated:YES];
}

#pragma mark - NoteFolderChooserViewControllerDelegate
- (void)folderPicker:(NoteFolderChooserViewController*)folderPicker
    didFinishWithFolder:(const NoteNode*)folder {
  self.folderItem = (NoteNode*)folder;
  [self updateFolderState];
  [self changeFolder:(NoteNode*)folder];
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
  [self dismiss];
  [self stop];
}

- (void)stop {
  self.folderViewController.delegate = nil;
  self.folderViewController = nil;
}
@end
