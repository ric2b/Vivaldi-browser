// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_interaction_controller.h"

#import <stdint.h>

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/notes/note_node.h"
#import "components/notes/notes_model.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/notes/notes_factory.h"
#import "ios/ui/notes/note_add_edit_folder_view_controller.h"
#import "ios/ui/notes/note_add_edit_view_controller.h"
#import "ios/ui/notes/note_folder_chooser_view_controller.h"
#import "ios/ui/notes/note_home_view_controller.h"
#import "ios/ui/notes/note_interaction_controller_delegate.h"
#import "ios/ui/notes/note_mediator.h"
#import "ios/ui/notes/note_navigation_controller_delegate.h"
#import "ios/ui/notes/note_navigation_controller.h"
#import "ios/ui/notes/note_path_cache.h"
#import "ios/ui/notes/note_transitioning_delegate.h"
#import "ios/ui/notes/note_utils_ios.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using vivaldi::NotesModel;
using vivaldi::NoteNode;

namespace {

// Tracks the type of UI that is currently being presented.
enum class PresentedState {
  NONE,
  NOTE_BROWSER,
  NOTE_EDITOR,
  NOTE_FOLDER_EDITOR,
  NOTE_FOLDER_SELECTION,
};

}  // namespace

@interface NoteInteractionController () <
    NoteFolderChooserViewControllerDelegate,
    NoteAddEditViewControllerDelegate,
    NoteHomeViewControllerDelegate> {
  // The browser notes are presented in.
  Browser* _browser;  // weak

  // The browser state of the current user.
  ProfileIOS* _currentProfile;  // weak

  // The browser state to use, might be different from _currentBrowserState if
  // it is incognito.
  ProfileIOS* _profile;  // weak

  // The parent controller on top of which the UI needs to be presented.
  __weak UIViewController* _parentController;

  // The web state list currently in use.
  WebStateList* _webStateList;
}

// The type of view controller that is being presented.
@property(nonatomic, assign) PresentedState currentPresentedState;

// The delegate provided to |self.noteNavigationController|.
@property(nonatomic, strong)
    NoteNavigationControllerDelegate* noteNavigationControllerDelegate;

// The note model in use.
@property(nonatomic, assign) NotesModel* noteModel;

// A reference to the potentially presented note browser. This will be
// non-nil when |currentPresentedState| is NOTE_BROWSER.
@property(nonatomic, strong) NoteHomeViewController* noteBrowser;

// A reference to the potentially presented single note editor. This will be
// non-nil when |currentPresentedState| is NOTE_EDITOR.
@property(nonatomic, strong) NoteAddEditViewController* noteEditor;

// A reference to the potentially presented folder selector. This will be
// non-nil when |currentPresentedState| is FOLDER_SELECTION.
@property(nonatomic, strong) NoteFolderChooserViewController* folderSelector;

@property(nonatomic, copy) void (^folderSelectionCompletionBlock)
    (const vivaldi::NoteNode*);

@property(nonatomic, strong) NoteMediator* mediator;

@property(nonatomic, readonly, weak) id<ApplicationCommands, BrowserCommands>
    handler;

// The transitioning delegate that is used when presenting
// |self.noteBrowser|.
@property(nonatomic, strong)
    NoteTransitioningDelegate* noteTransitioningDelegate;

// Handler for Snackbar Commands.
@property(nonatomic, readonly, weak) id<SnackbarCommands>
    snackbarCommandsHandler;

// Dismisses the note browser.  If |urlsToOpen| is not empty, then the user
// has selected to navigate to those URLs with specified tab mode.
- (void)dismissNoteBrowserAnimated:(BOOL)animated
                            urlsToOpen:(const std::vector<GURL>&)urlsToOpen
                           inIncognito:(BOOL)inIncognito
                                newTab:(BOOL)newTab;

// Dismisses the note editor.
- (void)dismissNoteEditorAnimated:(BOOL)animated;

@end

@implementation NoteInteractionController
@synthesize snackbarCommandsHandler = _snackbarCommandsHandler;
@synthesize noteBrowser = _noteBrowser;
@synthesize noteEditor = _noteEditor;
@synthesize noteModel = _noteModel;
@synthesize noteNavigationController = _noteNavigationController;
@synthesize noteNavigationControllerDelegate =
    _noteNavigationControllerDelegate;
@synthesize noteTransitioningDelegate = _noteTransitioningDelegate;
@synthesize currentPresentedState = _currentPresentedState;
@synthesize delegate = _delegate;
@synthesize handler = _handler;
@synthesize mediator = _mediator;
@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBrowser:(Browser*)browser
               parentController:(UIViewController*)parentController {
  self = [super init];
  if (self) {
    _browser = browser;
    // notes are always opened with the main browser state, even in
    // incognito mode.
    _currentProfile = browser->GetProfile();
    _profile = _currentProfile->GetOriginalProfile();
    _parentController = parentController;
    // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
    // clean up.
    _handler = static_cast<id<ApplicationCommands, BrowserCommands>>(
        browser->GetCommandDispatcher());
    _webStateList = browser->GetWebStateList();
    _noteModel = vivaldi::NotesModelFactory::GetForProfile(_profile);
    _mediator = [[NoteMediator alloc] initWithProfile:_profile];
    _currentPresentedState = PresentedState::NONE;
    DCHECK(_noteModel);
    DCHECK(_parentController);
  }
  return self;
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
  [self noteBrowserDismissed];

  _noteBrowser.homeDelegate = nil;
  [_noteBrowser shutdown];
  _noteBrowser = nil;

  _noteEditor.delegate = nil;
  _noteEditor = nil;

  _panelDelegate = nil;
  _noteNavigationController = nil;
}

- (id<SnackbarCommands>)snackbarCommandsHandler {
  // Using lazy loading here to avoid potential crashes with SnackbarCommands
  // not being yet dispatched.
  if (!_snackbarCommandsHandler) {
    _snackbarCommandsHandler =
        HandlerForProtocol(_browser->GetCommandDispatcher(), SnackbarCommands);
  }
  return _snackbarCommandsHandler;
}

- (void)showNotes {
  self.noteBrowser =
      [[NoteHomeViewController alloc] initWithBrowser:_browser];
  self.noteBrowser.homeDelegate = self;
  self.noteBrowser.snackbarCommandsHandler = self.snackbarCommandsHandler;

  NSArray<NoteHomeViewController*>* replacementViewControllers = nil;
  if (self.noteModel->loaded()) {
    // Set the root node if the model has been loaded. If the model has not been
    // loaded yet, the root node will be set in NoteHomeViewController after
    // the model is finished loading.
    [self.noteBrowser setRootNode:self.noteModel->root_node()];
    replacementViewControllers =
       [self.noteBrowser cachedViewControllerStack];
  }

  self.currentPresentedState = PresentedState::NOTE_BROWSER;
  [self showHomeViewController:self.noteBrowser
      withReplacementViewControllers:replacementViewControllers];
}

- (void)presentEditorForNode:(const vivaldi::NoteNode*)node {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  [self dismissSnackbar];

  if (!node) {
    return;
  }

  if (!(node->type() == NoteNode::NOTE ||
        node->type() == NoteNode::FOLDER)) {
    return;
  }

  if (node->type() == vivaldi::NoteNode::NOTE)
      [self presentNoteEditorWithEditingNode:node
                                  parentNode:nil isEditing:YES isFolder:NO];
  else {
      [self presentNoteFolderEditor:node parent:nil isEditing:YES];
  }
}

- (void)presentAddViewController:(const NoteNode*)parent {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  [self dismissSnackbar];
  [self presentNoteEditorWithEditingNode:nil parentNode:parent
                               isEditing:NO isFolder:NO];
  [self.baseViewController showViewController:_noteEditor sender:self];
}

- (void)dismissNoteBrowserAnimated:(BOOL)animated
                            urlsToOpen:(const std::vector<GURL>&)urlsToOpen
                           inIncognito:(BOOL)inIncognito
                                newTab:(BOOL)newTab {
  if (self.currentPresentedState != PresentedState::NOTE_BROWSER) {
    return;
  }
  ProceduralBlock completion = ^{
    [self noteBrowserDismissed];
    [self.panelDelegate panelDismissed];
  };
  [self.noteBrowser dismissViewControllerAnimated:animated completion:completion];

  DCHECK(self.noteNavigationController);
  if (_parentController) {
    [_parentController dismissViewControllerAnimated:animated
                                          completion:nil];
  }
  self.currentPresentedState = PresentedState::NONE;
}

- (void)noteBrowserDismissed {
  // TODO(crbug.com/940856): Make sure navigaton
  // controller doesn't keep any controllers. Without
  // this there's a memory leak of (almost) every BHVC
  // the user visits.
  [self.noteNavigationController setViewControllers:@[] animated:NO];

  self.noteBrowser.homeDelegate = nil;
  self.noteBrowser = nil;
  self.noteTransitioningDelegate = nil;
  self.noteNavigationController = nil;
  self.noteNavigationControllerDelegate = nil;
}

- (void)dismissNoteEditorAnimated:(BOOL)animated {
  if (self.currentPresentedState != PresentedState::NOTE_EDITOR) {
    return;
  }
  DCHECK(self.noteNavigationController);

  self.noteEditor.delegate = nil;
  self.noteEditor = nil;
  [self.noteNavigationController
      dismissViewControllerAnimated:animated
                         completion:^{
                           self.noteNavigationController = nil;
                           self.noteTransitioningDelegate = nil;
                         }];
  self.currentPresentedState = PresentedState::NONE;
}

- (void)dismissFolderSelectionAnimated:(BOOL)animated {
  if (self.currentPresentedState != PresentedState::NOTE_FOLDER_SELECTION)
    return;
  DCHECK(self.noteNavigationController);

  [self.noteNavigationController
      dismissViewControllerAnimated:animated
                         completion:^{
                           self.folderSelector.delegate = nil;
                           self.folderSelector = nil;
                           self.noteNavigationController = nil;
                           self.noteTransitioningDelegate = nil;
                         }];
  self.currentPresentedState = PresentedState::NONE;
}

- (void)dismissNoteModalControllerAnimated:(BOOL)animated {
  // No urls to open.  So it does not care about inIncognito and newTab.
  [self dismissNoteBrowserAnimated:animated
                            urlsToOpen:std::vector<GURL>()
                           inIncognito:NO
                                newTab:NO];
  [self dismissNoteEditorAnimated:animated];
}

- (void)dismissSnackbar {
   //Dismiss any note related snackbar this controller could have presented.
  [MDCSnackbarManager.defaultManager
      dismissAndCallCompletionBlocksWithCategory:
          note_utils_ios::kNotesSnackbarCategory];
}

#pragma mark - NoteAddEditViewControllerDelegate

- (BOOL)noteEditor:(NoteAddEditViewController*)controller
    shoudDeleteAllOccurencesOfNote:(const NoteNode*)note {
  return YES;
}

- (void)noteEditorWantsDismissal:(NoteAddEditViewController*)controller {
  [self dismissNoteEditorAnimated:YES];
}

- (void)noteEditorWillCommitContentChange:
    (NoteAddEditViewController*)controller {
  [self.delegate noteInteractionControllerWillCommitContentChange:self];
}

#pragma mark - NoteFolderChooserViewControllerDelegate

- (void)folderPicker:(NoteFolderChooserViewController*)folderPicker
    didFinishWithFolder:(const  vivaldi::NoteNode*)folder {
  [self dismissFolderSelectionAnimated:YES];

  if (self.folderSelectionCompletionBlock) {
    self.folderSelectionCompletionBlock(folder);
  }
}

- (void)folderPickerDidCancel:(NoteFolderChooserViewController*)folderPicker {
  [self dismissFolderSelectionAnimated:YES];
}

- (void)folderPickerDidDismiss:(NoteFolderChooserViewController*)folderPicker {
  [self dismissFolderSelectionAnimated:YES];
}

#pragma mark - NoteHomeViewControllerDelegate

- (void)
noteHomeViewControllerWantsDismissal:(NoteHomeViewController*)controller
                        navigationToUrls:(const std::vector<GURL>&)urls {
  [self noteHomeViewControllerWantsDismissal:controller
                                navigationToUrls:urls
                                     inIncognito:_currentProfile
                                                     ->IsOffTheRecord()
                                          newTab:NO];
}

- (void)noteHomeViewControllerWantsDismissal:
            (NoteHomeViewController*)controller
                                navigationToUrls:(const std::vector<GURL>&)urls
                                     inIncognito:(BOOL)inIncognito
                                          newTab:(BOOL)newTab {
  [self dismissNoteBrowserAnimated:YES
                            urlsToOpen:urls
                           inIncognito:inIncognito
                                newTab:newTab];
}

#pragma mark - Private

// Presents |viewController| using the appropriate presentation and styling,
// depending on whether the UIRefresh experiment is enabled or disabled. Sets
// |self.noteNavigationController| to the UINavigationController subclass
// used, and may set |self.noteNavigationControllerDelegate| or
// |self.noteTransitioningDelegate| depending on whether or not the desired
// transition requires those objects.  If |replacementViewControllers| is not
// nil, those controllers are swapped in to the UINavigationController instead
// of |viewController|.
// Presents the diff controllers from note browser.
// Note browser is not presented but shown in page view controller
- (void)presentTableViewController:
            (LegacyChromeTableViewController<
                UIAdaptivePresentationControllerDelegate>*)viewController
    withReplacementViewControllers:
        (NSArray<LegacyChromeTableViewController*>*)replacementViewControllers {
  TableViewNavigationController* navController =
      [[TableViewNavigationController alloc] initWithTable:viewController];

  UINavigationBarAppearance* transparentAppearance =
      [[UINavigationBarAppearance alloc] init];
  [transparentAppearance configureWithTransparentBackground];
  navController.navigationBar.standardAppearance = transparentAppearance;
  navController.navigationBar.compactAppearance = transparentAppearance;
  navController.navigationBar.scrollEdgeAppearance = transparentAppearance;
  self.noteNavigationController = navController;
  if (replacementViewControllers) {
    [navController setViewControllers:replacementViewControllers];
  }

  navController.toolbarHidden = YES;
  self.noteNavigationControllerDelegate =
      [[NoteNavigationControllerDelegate alloc] init];
  navController.delegate = self.noteNavigationControllerDelegate;
  if (self.currentPresentedState != PresentedState::NOTE_BROWSER) {
    [_parentController presentViewController:navController
                                  animated:YES
                                completion:nil];
  }
}

- (void)showHomeViewController:
            (LegacyChromeTableViewController<
                UIAdaptivePresentationControllerDelegate>*)viewController
    withReplacementViewControllers:
        (NSArray<LegacyChromeTableViewController*>*)replacementViewControllers {
  TableViewNavigationController* navController =
      [[TableViewNavigationController alloc] initWithTable:viewController];
  self.noteNavigationController = navController;
  UINavigationBarAppearance* transparentAppearance =
      [[UINavigationBarAppearance alloc] init];
  [transparentAppearance configureWithTransparentBackground];
  navController.navigationBar.standardAppearance = transparentAppearance;
  navController.navigationBar.compactAppearance = transparentAppearance;
  navController.navigationBar.scrollEdgeAppearance = transparentAppearance;

  if (replacementViewControllers) {
    [navController setViewControllers:replacementViewControllers];
  }

  navController.toolbarHidden = YES;
  self.noteNavigationControllerDelegate =
      [[NoteNavigationControllerDelegate alloc] init];
  navController.delegate = self.noteNavigationControllerDelegate;
}

- (void)presentNoteEditorWithEditingNode:(const NoteNode*)editingNode
                                  parentNode:(const NoteNode*)parentNode
                                   isEditing:(BOOL)isEditing
                                    isFolder:(BOOL)isFolder {
  if (isFolder)
    [self presentNoteFolderEditor:editingNode
                                  parent:parentNode
                                   isEditing:isEditing];
  else {
      [self presentNoteEditor:editingNode
                   parentNode:parentNode
                    isEditing:isEditing];
  }
}

/// 'editingItem' can be nil as this editor will be presented for both adding
/// and editing item
- (void)presentNoteEditor:(const NoteNode*)node
               parentNode:(const NoteNode*)parentNode
                isEditing:(BOOL)isEditing {
  NoteAddEditViewController* controller =
    [NoteAddEditViewController
     initWithBrowser:_browser
                item:node
              parent:parentNode
           isEditing:isEditing
        allowsCancel:YES];
  self.noteEditor = controller;
  self.noteEditor.delegate = self;

  UINavigationController *newVC =
      [[UINavigationController alloc]
        initWithRootViewController:controller];

  self.currentPresentedState = PresentedState::NOTE_EDITOR;

  // Present the nav bar controller on top of the parent
  [_parentController presentViewController:newVC
                                      animated:YES
                                    completion:nil];
}

/// 'editingItem' can be nil as this editor will be presented for both adding
/// and editing item
- (void)presentNoteFolderEditor:(const NoteNode*)node
                                parent:(const NoteNode*)parentNode
                                 isEditing:(BOOL)isEditing {
   NoteAddEditFolderViewController* controller =
    [NoteAddEditFolderViewController
       initWithBrowser:_browser
                  item:node
                parent:parentNode
             isEditing:isEditing
          allowsCancel:YES];
  UINavigationController *newVC =
      [[UINavigationController alloc]
        initWithRootViewController:controller];

  self.currentPresentedState = PresentedState::NOTE_FOLDER_EDITOR;

  // Present the nav bar controller on top of the parent
  [_parentController presentViewController:newVC
                                      animated:YES
                                    completion:nil];
}

@end
