// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_home_view_controller.h"

#import "app/vivaldi_apptools.h"
#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/notes/notes_model.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/drag_and_drop/model/table_view_url_drag_drop_handler.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/elements/home_waiting_view.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_illustrated_empty_view.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/notes/notes_factory.h"
#import "ios/panel/panel_constants.h"
#import "ios/ui/custom_views/vivaldi_search_bar_view.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/notes/cells/note_folder_item.h"
#import "ios/ui/notes/cells/note_home_node_item.h"
#import "ios/ui/notes/cells/note_table_cell_title_edit_delegate.h"
#import "ios/ui/notes/cells/table_view_note_cell.h"
#import "ios/ui/notes/note_add_edit_view_controller.h"
#import "ios/ui/notes/note_empty_background.h"
#import "ios/ui/notes/note_folder_chooser_view_controller.h"
#import "ios/ui/notes/note_home_consumer.h"
#import "ios/ui/notes/note_home_mediator.h"
#import "ios/ui/notes/note_home_shared_state.h"
#import "ios/ui/notes/note_interaction_controller_delegate.h"
#import "ios/ui/notes/note_interaction_controller.h"
#import "ios/ui/notes/note_model_bridge_observer.h"
#import "ios/ui/notes/note_navigation_controller.h"
#import "ios/ui/notes/note_path_cache.h"
#import "ios/ui/notes/note_sorting_mode.h"
#import "ios/ui/notes/note_ui_constants.h"
#import "ios/ui/notes/note_utils_ios.h"
#import "ios/ui/notes/vivaldi_notes_pref.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using vivaldi::NoteNode;
using l10n_util::GetNSString;

// Used to store a pair of NSIntegers when storing a NSIndexPath in C++
// collections.
using IntegerPair = std::pair<NSInteger, NSInteger>;

namespace {
typedef NS_ENUM(NSInteger, NotesContextBarState) {
  NotesContextBarNone,            // No state.
  NotesContextBarDefault,         // No selection is possible in this state.
  NotesContextBarBeginSelection,  // This is the clean start state,
  // selection is possible, but nothing is
  // selected yet.
  NotesContextBarSingleNoteSelection,       // Single note selected state.
  NotesContextBarMultipleNoteSelection,     // Multiple notes selected state.
  NotesContextBarSingleFolderSelection,    // Single folder selected.
  NotesContextBarMultipleFolderSelection,  // Multiple folders selected.
  NotesContextBarMixedSelection,  // Multiple notes / Folders selected.
};

// Estimated TableView row height.
const CGFloat kEstimatedRowHeight = 65.0;

// TableView rows that are hidden by the NavigationBar, causing them to be
// "visible" for the tableView but not for the user. This is used to calculate
// the top most visibile table view indexPath row.
// TODO(crbug.com/879001): This value is aproximate based on the standard (no
// dynamic type) height. If the dynamic font is too large or too small it will
// result in a small offset on the cache, in order to prevent this we need to
// calculate this value dynamically.
const int kRowsHiddenByNavigationBar = 3;

}  // namespace

@interface NoteHomeViewController () <NoteFolderChooserViewControllerDelegate,
                                          NoteHomeConsumer,
                                          NoteHomeSharedStateObserver,
                                          NoteInteractionControllerDelegate,
                                          NoteModelBridgeObserver,
                                          NoteTableCellTitleEditDelegate,
                                          TableViewURLDragDataSource,
                                          TableViewURLDropDelegate,
                                          UIGestureRecognizerDelegate,
                                          VivaldiSearchBarViewDelegate,
                                          SettingsNavigationControllerDelegate,
                                          UITableViewDataSource,
                                          UITableViewDelegate> {
  // Bridge to register for note changes.
  std::unique_ptr<notes::NoteModelBridge> _bridge;

  // The root node, whose child nodes are shown in the note table view.
  const vivaldi::NoteNode* _rootNode;
}
// Navigation View controller for the settings.
@property(nonatomic, strong)
    SettingsNavigationController* settingsNavigationController;

// Sort button
@property(nonatomic, weak) UIButton* sortButton;

// Array to hold the sort button context menu options
@property (strong, nonatomic) NSMutableArray *notesSortActions;

// Array to hold ascendeing decending buttons for sort context menu options
@property (strong, nonatomic) NSMutableArray *sortOrderActions;

// Shared state between NoteHome classes.  Used as a temporary refactoring
// aid.
@property(nonatomic, strong) NoteHomeSharedState* sharedState;

// The note model used.
@property(nonatomic, assign) vivaldi::NotesModel* notes;

// The Browser in which notes are presented
@property(nonatomic, assign) Browser* browser;

// The user's profile used.
@property(nonatomic, assign) ProfileIOS* profile;

// The mediator that provides data for this view controller.
@property(nonatomic, strong) NoteHomeMediator* mediator;

// The view controller used to pick a folder in which to move the selected
// notes.
@property(nonatomic, strong) NoteFolderChooserViewController* folderSelector;

// The current state of the context bar UI.
@property(nonatomic, assign) NotesContextBarState contextBarState;

// When the view is first shown on the screen, this property represents the
// cached value of the top most visible indexPath row of the table view. This
// property is set to nil after it is used.
@property(nonatomic, assign) int cachedIndexPathRow;

// Set to YES, only when this view controller instance is being created
// from cached path. Once the view controller is shown, this is set to NO.
// This is so that the cache code is called only once in loadNoteViews.
@property(nonatomic, assign) BOOL isReconstructingFromCache;

// Handler for commands.
@property(nonatomic, readonly, weak) id<ApplicationCommands, BrowserCommands>
    handler;

// The current search term.  Set to the empty string when no search is active.
@property(nonatomic, copy) NSString* searchTerm;

// Navigation UIToolbar Delete button.
@property(nonatomic, strong) UIBarButtonItem* deleteButton;

// Navigation UIToolbar Plus button.
@property(nonatomic, strong) UIBarButtonItem* plusButton;

// Navigation UIToolbar More button.
@property(nonatomic, strong) UIBarButtonItem* moreButton;

// Background shown when there is no notes or folders at the current root
// node.
@property(nonatomic, strong) NoteEmptyBackground* emptyTableBackgroundView;

// Illustrated View displayed when the current root node is empty.
@property(nonatomic, strong) TableViewIllustratedEmptyView* emptyViewBackground;

// The loading spinner background which appears when loading the NotesModel
// or syncing.
@property(nonatomic, strong) HomeWaitingView* spinnerView;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) AlertCoordinator* actionSheetCoordinator;

@property(nonatomic, strong)
    NoteInteractionController* noteInteractionController;

@property(nonatomic, assign) WebStateList* webStateList;


// Handler for URL drag and drop interactions.
@property(nonatomic, strong) TableViewURLDragDropHandler* dragDropHandler;

// Coordinator in charge of handling sharing use cases.
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;

// Custom tableHeaderView to host search bar.
@property(nonatomic, strong) VivaldiSearchBarView* vivaldiSearchBarView;

// Sorting actions
@property(nonatomic, strong) UIAction* manualSortAction;
@property(nonatomic, strong) UIAction* titleSortAction;
@property(nonatomic, strong) UIAction* dateCreatedSortAction;
@property(nonatomic, strong) UIAction* dateEditedSortAction;
@property(nonatomic, strong) UIAction* kindSortAction;

// The current sort order Action
@property(nonatomic, strong) UIAction* ascendingSortAction;
@property(nonatomic, strong) UIAction* descendingSortAction;

@end

@implementation NoteHomeViewController
// synthesized properties
@synthesize manualSortAction = _manualSortAction;
@synthesize titleSortAction = _titleSortAction;
@synthesize dateCreatedSortAction = _dateCreatedSortAction;
@synthesize dateEditedSortAction = _dateEditedSortAction;
@synthesize kindSortAction = _kindSortAction;
@synthesize ascendingSortAction = _ascendingSortAction;
@synthesize descendingSortAction = _descendingSortAction;
#pragma mark - Initializer

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _browser = browser;
    _profile = _browser->GetProfile()->GetOriginalProfile();
    [VivaldiNotesPrefs setPrefService:_profile->GetPrefs()];
    _webStateList = _browser->GetWebStateList();
    _notes = vivaldi::NotesModelFactory::GetForProfile(_profile);
    _bridge.reset(new notes::NoteModelBridge(self, _notes));
  }
  return self;
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
  [_noteInteractionController shutdown];
  _noteInteractionController = nil;

  [self.mediator disconnect];
  _sharedState.tableView.dataSource = nil;
  _sharedState.tableView.delegate = nil;

  _bridge.reset();
}

- (void)setRootNode:(const vivaldi::NoteNode*)rootNode {
  _rootNode = rootNode;
}

- (NSArray<NoteHomeViewController*>*)cachedViewControllerStack {
  // This method is only designed to be called for the view controller
  // associated with the root node.
  DCHECK(self.notes->loaded());
  DCHECK([self isDisplayingNoteRoot]);

  NSMutableArray<NoteHomeViewController*>* stack = [NSMutableArray array];
  // Configure the root controller Navigationbar at this time when
  // reconstructing from cache, or there will be a loading flicker if this gets
  // done on viewDidLoad.
  [self setupNavigationForNoteHomeViewController:self
                                   usingNoteNode:_rootNode];
  [stack addObject:self];

  int64_t cachedFolderID;
  int cachedIndexPathRow;
  // If cache is present then reconstruct the last visited note from
  // cache.
  if (![NotePathCache
          getNoteTopMostRowCacheWithPrefService:self.profile
                                                        ->GetPrefs()
                                              model:self.notes
                                           folderId:&cachedFolderID
                                         topMostRow:&cachedIndexPathRow] ||
      cachedFolderID == self.notes->root_node()->id()) {
    return stack;
  }

  NSArray* path =
      note_utils_ios::CreateNotePath(self.notes, cachedFolderID);
  if (!path) {
    return stack;
  }

  DCHECK_EQ(self.notes->root_node()->id(),
            [[path firstObject] longLongValue]);
  for (NSUInteger ii = 1; ii < [path count]; ii++) {
    int64_t nodeID = [[path objectAtIndex:ii] longLongValue];
    const NoteNode* node =
        note_utils_ios::FindFolderById(self.notes, nodeID);
    DCHECK(node);
    // if node is an empty permanent node, stop.
    if (node->children().empty() /*&&
        IsPrimaryPermanentNode(node, self.notes)*/) {
      break;
    }

    NoteHomeViewController* controller =
        [self createControllerWithRootFolder:node];
    // Configure the controller's Navigationbar at this time when
    // reconstructing from cache, or there will be a loading flicker if this
    // gets done on viewDidLoad.
    [self setupNavigationForNoteHomeViewController:controller
                                     usingNoteNode:node];
    if (nodeID == cachedFolderID) {
      controller.cachedIndexPathRow = cachedIndexPathRow;
    }
    [stack addObject:controller];
  }
  return stack;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set Navigation Bar, Toolbar and TableView appearance.
  self.navigationController.navigationBarHidden = NO;
  self.navigationController.toolbar.accessibilityIdentifier =
      kNoteHomeUIToolbarIdentifier;

  [self setupHeaderWithSearch];

  self.searchTerm = @"";

  if (self.notes->loaded()) {
    [self loadNoteViews];
  } else {
    [self showLoadingSpinnerBackground];
  }
  [self.tableView reloadData];
}

- (void)viewWillLayoutSubviews {
  if (self.cachedIndexPathRow &&
      [self.tableView numberOfRowsInSection:0] > self.cachedIndexPathRow) {
      NSIndexPath* indexPath =
       [NSIndexPath indexPathForRow:self.cachedIndexPathRow inSection:0];
      [self.tableView scrollToRowAtIndexPath:indexPath
                            atScrollPosition:UITableViewScrollPositionTop
                                    animated:NO];
      self.cachedIndexPathRow = 0;
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Set the delegate here to make sure it is working when navigating in the
  // ViewController hierarchy (as each view controller is setting itself as
  // delegate).
  self.navigationController.interactivePopGestureRecognizer.delegate = self;

  // Hide the toolbar if we're displaying the root node.
  if (self.notes->loaded() &&
      (![self isDisplayingNoteRoot] ||
     self.sharedState.currentlyShowingSearchResults)) {
      self.navigationController.toolbarHidden = NO;
  } else {
     if (_notes->main_node()->children().empty())
        self.navigationController.toolbarHidden = NO;
     else {
        self.navigationController.toolbarHidden = YES;
     }
  }

  // If we navigate back to the root level, we need to make sure the root level
  // folders are created or deleted if needed.
  if ([self isDisplayingNoteRoot]) {
    [self refreshContents];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  // Restore to default origin offset for cancel button proxy style.
  UIBarButtonItem* cancelButton = [UIBarButtonItem
      appearanceWhenContainedInInstancesOfClasses:@[ [UISearchBar class] ]];
  [cancelButton setTitlePositionAdjustment:UIOffsetZero
                             forBarMetrics:UIBarMetricsDefault];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Check that the tableView still contains as many rows, and that
  // |self.cachedIndexPathRow| is not 0.
  if (self.cachedIndexPathRow &&
      [self.tableView numberOfRowsInSection:0] > self.cachedIndexPathRow) {
    NSIndexPath* indexPath =
        [NSIndexPath indexPathForRow:self.cachedIndexPathRow inSection:0];
    [self.tableView scrollToRowAtIndexPath:indexPath
                          atScrollPosition:UITableViewScrollPositionTop
                                  animated:NO];
    self.cachedIndexPathRow = 0;
  }
}

- (BOOL)prefersStatusBarHidden {
  return NO;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // Stop edit of current note folder name, if any.
  [self.sharedState.editingFolderCell stopEdit];
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  [self navigationBarCancel:nil];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleDefault;
}

#pragma mark - Protected

- (void)loadNoteViews {
  DCHECK(_rootNode);
  [self loadModel];
  self.sharedState =
      [[NoteHomeSharedState alloc] initWithNotesModel:_notes
                                           displayedRootNode:_rootNode];
  self.sharedState.tableViewModel = self.tableViewModel;
  self.sharedState.tableView = self.tableView;
  self.sharedState.observer = self;
  self.sharedState.currentlyShowingSearchResults = NO;

  self.dragDropHandler = [[TableViewURLDragDropHandler alloc] init];
  self.dragDropHandler.origin = WindowActivityToolsOrigin;
  self.dragDropHandler.dragDataSource = self;
  self.dragDropHandler.dropDelegate = self;
  self.tableView.dragDelegate = self.dragDropHandler;
  self.tableView.dropDelegate = self.dragDropHandler;
  self.tableView.dragInteractionEnabled = true;

  // Configure the table view.
  self.sharedState.tableView.accessibilityIdentifier =
      kNoteHomeTableViewIdentifier;
  self.sharedState.tableView.estimatedRowHeight = kEstimatedRowHeight;
  self.tableView.sectionHeaderHeight = 0;
  // Setting a sectionFooterHeight of 0 will be the same as not having a
  // footerView, which shows a cell separator for the last cell. Removing this
  // line will also create a default footer of height 30.
  self.tableView.sectionFooterHeight = 1;
  self.sharedState.tableView.allowsMultipleSelectionDuringEditing = YES;

  // Create the mediator and hook up the table view.
  self.mediator =
      [[NoteHomeMediator alloc] initWithSharedState:self.sharedState
                                            profile:self.profile];
  self.mediator.consumer = self;
  [self.mediator startMediating];

  [self setupNavigationForNoteHomeViewController:self
                                   usingNoteNode:_rootNode];

  [self setupContextBar];

  if (self.isReconstructingFromCache) {
    [self setupUIStackCacheIfApplicable];
  }

  DCHECK(self.notes->loaded());
  DCHECK([self isViewLoaded]);
}

- (void)cacheIndexPathRow {
  // Cache IndexPathRow for NoteTableView.
  int topMostVisibleIndexPathRow = [self topMostVisibleIndexPathRow];
  if (_rootNode) {
    [NotePathCache
        cacheNoteTopMostRowWithPrefService:self.profile->GetPrefs()
                                      folderId:_rootNode->id()
                                    topMostRow:topMostVisibleIndexPathRow];
  } else {
    // TODO(crbug.com/1061882):Remove DCHECK once we know the root cause of the
    // bug, for now this will cause a crash on Dev/Canary and we should get
    // breadcrumbs.
    DCHECK(NO);
  }
}

#pragma mark - NoteHomeConsumer

- (void)refreshContents {
  if (self.sharedState.currentlyShowingSearchResults) {
    NSString* noResults = GetNSString(IDS_HISTORY_NO_SEARCH_RESULTS);
    [self.mediator computeNoteTableViewDataMatching:self.searchTerm
                             orShowMessageWhenNoResults:noResults];
  } else {
    [self.mediator computeNoteTableViewData];
  }
  [self handleRefreshContextBar];
  [self.sharedState.editingFolderCell stopEdit];
  [self.sharedState.editingNoteCell stopEdit];
  [self.sharedState.tableView reloadData];
  if (self.sharedState.currentlyInEditMode &&
      !self.sharedState.editNodes.empty()) {
    [self restoreRowSelection];
  }
}

- (void)updateTableViewBackgroundStyle:(NoteHomeBackgroundStyle)style {
  if (style == NoteHomeBackgroundStyleDefault) {
    [self hideLoadingSpinnerBackground];
    [self hideEmptyBackground];
  } else if (style == NoteHomeBackgroundStyleLoading) {
    [self hideEmptyBackground];
    [self showLoadingSpinnerBackground];
  } else if (style == NoteHomeBackgroundStyleEmpty) {
    [self hideLoadingSpinnerBackground];
    [self showEmptyBackground];
  }
}

- (void)showSignin:(ShowSigninCommand*)command {
  [self.applicationCommandsHandler showSignin:command
        baseViewController:self.navigationController];
}

#pragma mark - Action sheet callbacks

// Opens the folder move editor for the given node.
- (void)moveNodes:(const std::set<const NoteNode*>&)nodes {
  DCHECK(!self.folderSelector);
  DCHECK(nodes.size() > 0);
  const NoteNode* editedNode = *(nodes.begin());
  const NoteNode* selectedFolder = editedNode->parent();
  self.folderSelector =
      [[NoteFolderChooserViewController alloc] initWithNotesModel:self.notes
                                                 allowsNewFolders:YES
                                                      editedNodes:nodes
                                                     allowsCancel:YES
                                                   selectedFolder:selectedFolder
                                                          browser:self.browser];
  self.folderSelector.delegate = self;
  UINavigationController* navController = [[NoteNavigationController alloc]
      initWithRootViewController:self.folderSelector];
  [navController setModalPresentationStyle:UIModalPresentationFormSheet];
  [self presentViewController:navController animated:YES completion:NULL];
}

- (void)moveNodesToTrash:(const std::set<const NoteNode*>&)nodes {
  DCHECK_GE(nodes.size(), 1u);

  const NoteNode* trashFolder = self.notes->trash_node();
  [self.snackbarCommandsHandler
      showSnackbarMessage:note_utils_ios::MoveNotesWithToast(
                              nodes, self.notes, trashFolder,
                              self.profile)];
  [self setTableViewEditing:NO];
}

// Deletes the current node.
- (void)deleteNodes:(const std::set<const NoteNode*>&)nodes {
  DCHECK_GE(nodes.size(), 1u);
  [self.snackbarCommandsHandler
      showSnackbarMessage:note_utils_ios::DeleteNotesWithToast(
                              nodes, self.notes, self.profile)];
  [self setTableViewEditing:NO];
}

// Opens the editor on the given node.
- (void)editNode:(const NoteNode*)node {
  if (!self.noteInteractionController) {
    self.noteInteractionController =
        [[NoteInteractionController alloc] initWithBrowser:self.browser
                                              parentController:self];
    self.noteInteractionController.delegate = self;
  }
  [self.noteInteractionController presentEditorForNode:node];
}


/// Open the add note dialog
- (void)addNote {
  if (!self.noteInteractionController) {
    self.noteInteractionController =
        [[NoteInteractionController alloc] initWithBrowser:self.browser
                                              parentController:self];
    self.noteInteractionController.delegate = self;
  }
  [self.noteInteractionController presentAddViewController:
    self.sharedState.tableViewDisplayedRootNode];
}


- (void)openAllURLs:(std::vector<GURL>)urls
        inIncognito:(BOOL)inIncognito
             newTab:(BOOL)newTab {
  if (inIncognito) {
    IncognitoReauthSceneAgent* reauthAgent = [IncognitoReauthSceneAgent
        agentFromScene:self.browser->GetSceneState()];
    if (reauthAgent.authenticationRequired) {
      __weak NoteHomeViewController* weakSelf = self;
      [reauthAgent
          authenticateIncognitoContentWithCompletionBlock:^(BOOL success) {
            if (success) {
              [weakSelf openAllURLs:urls inIncognito:inIncognito newTab:newTab];
            }
          }];
      return;
    }
  }

  [self cacheIndexPathRow];
  [self.homeDelegate noteHomeViewControllerWantsDismissal:self
                                             navigationToUrls:urls
                                                  inIncognito:inIncognito
                                                       newTab:newTab];
}

#pragma mark - Navigation Bar Callbacks

- (void)navigationBarCancel:(id)sender {
  [self navigateAway];
  [self dismissWithURL:GURL()];
}

#pragma mark - More Private Methods

- (void)handleSelectUrlForNavigation:(const GURL&)url {
  [self dismissWithURL:url];
}

- (void)handleSelectFolderForNavigation:(const vivaldi::NoteNode*)folder {
  if (self.sharedState.currentlyShowingSearchResults) {
    // Clear note path cache.
    int64_t unusedFolderId;
    int unusedIndexPathRow;
    while ([NotePathCache
        getNoteTopMostRowCacheWithPrefService:self.profile->GetPrefs()
                                            model:self.notes
                                         folderId:&unusedFolderId
                                       topMostRow:&unusedIndexPathRow]) {
      [NotePathCache
          clearNoteTopMostRowCacheWithPrefService:self.profile
                                                          ->GetPrefs()];
    }

    // Rebuild folder controller list, going back up the tree.
    NSMutableArray<NoteHomeViewController*>* stack = [NSMutableArray array];
    std::vector<const vivaldi::NoteNode*> nodes;
    const vivaldi::NoteNode* cursor = folder;
    while (cursor) {
      // Build reversed list of nodes to restore note path below.
      nodes.insert(nodes.begin(), cursor);

      // Build reversed list of controllers.
      NoteHomeViewController* controller =
          [self createControllerWithRootFolder:cursor];
      [stack insertObject:controller atIndex:0];

      // Setup now, so that the back button labels shows parent folder
      // title and that we don't show large title everywhere.
      [self setupNavigationForNoteHomeViewController:controller
                                       usingNoteNode:cursor];

      cursor = cursor->parent();
    }
    // Reconstruct note path cache.
    for (const vivaldi::NoteNode* node : nodes) {
      [NotePathCache
          cacheNoteTopMostRowWithPrefService:self.profile->GetPrefs()
                                        folderId:node->id()
                                      topMostRow:0];
    }

    [self navigateAway];
    [self.navigationController setViewControllers:stack animated:YES];
    return;
  }
  NoteHomeViewController* controller =
      [self createControllerWithRootFolder:folder];
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)handleSelectNodesForDeletion:
    (const std::set<const vivaldi::NoteNode*>&)nodes {
  if (_rootNode->is_trash()) {
      [self deleteNodes:nodes];
  } else {
    [self moveNodesToTrash:nodes];
  }
}

- (void)handleSelectEditNodes:
    (const std::set<const vivaldi::NoteNode*>&)nodes {
  // Early return if notes table is not in edit mode.
  if (!self.sharedState.currentlyInEditMode) {
    return;
  }

  if (nodes.size() == 0) {
    // if nothing to select, exit edit mode.
    if (![self hasNotesOrFolders]) {
      [self setTableViewEditing:NO];
      return;
    }
    [self setContextBarState:NotesContextBarBeginSelection];
    return;
  }
  if (nodes.size() == 1) {
    const vivaldi::NoteNode* node = *nodes.begin();
    if (node->is_note()) {
      [self setContextBarState:NotesContextBarSingleNoteSelection];
    } else if (node->is_folder()) {
      [self setContextBarState:NotesContextBarSingleFolderSelection];
    }
    return;
  }

  BOOL foundNote = NO;
  BOOL foundFolder = NO;

  for (const NoteNode* node : nodes) {
    if (!foundNote && node->is_note()) {
      foundNote = YES;
    } else if (!foundFolder && node->is_folder()) {
      foundFolder = YES;
    }
    // Break early, if we found both types of nodes.
    if (foundNote && foundFolder) {
      break;
    }
  }


  // Only Notes are selected.
  if (foundNote && !foundFolder) {
    [self setContextBarState:NotesContextBarMultipleNoteSelection];
    return;
  }
  // Only Folders are selected.
  if (!foundNote && foundFolder) {
    [self setContextBarState:NotesContextBarMultipleFolderSelection];
    return;
  }
  // Mixed selection.
  if (foundNote && foundFolder) {
    [self setContextBarState:NotesContextBarMixedSelection];
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

- (void)handleMoveNode:(const vivaldi::NoteNode*)node
            toPosition:(int)position {
  [self.snackbarCommandsHandler
      showSnackbarMessage:
          note_utils_ios::UpdateNotePositionWithToast(
              node, _rootNode, position, self.notes, self.profile)];
}

- (BOOL)isAtTopOfNavigation {
  return (self.navigationController.topViewController == self);
}

#pragma mark - NoteTableCellTitleEditDelegate

- (void)textDidChangeTo:(NSString*)newName {
  DCHECK(self.sharedState.editingFolderNode);
  if (newName.length > 0 && self.sharedState.editingFolderNode != nil) {
    self.sharedState.notesModel->SetTitle(self.sharedState.editingFolderNode,
                                             base::SysNSStringToUTF16(newName));
  } else if (newName.length > 0 && self.sharedState.editingNoteNode != nil) {
      self.sharedState.notesModel->SetTitle(self.sharedState.editingNoteNode,
                                               base::SysNSStringToUTF16(newName));
  }
  self.sharedState.addingNewFolder = NO;
  self.sharedState.addingNewNote = NO;
  self.sharedState.editingFolderNode = nullptr;
  self.sharedState.editingFolderCell = nil;
  self.sharedState.editingNoteNode = nullptr;
  self.sharedState.editingNoteCell = nil;
  [self refreshContents];
}

#pragma mark - NoteFolderChooserViewControllerDelegate

- (void)folderPicker:(NoteFolderChooserViewController*)folderPicker
    didFinishWithFolder:(const NoteNode*)folder {
  DCHECK(folder);
  DCHECK_GE(folderPicker.editedNodes.size(), 1u);

  [self.snackbarCommandsHandler
      showSnackbarMessage:note_utils_ios::MoveNotesWithToast(
                              folderPicker.editedNodes, self.notes, folder,
                              self.profile)];

  [self setTableViewEditing:NO];
  [self.navigationController dismissViewControllerAnimated:YES completion:NULL];
  self.folderSelector.delegate = nil;
  self.folderSelector = nil;
}

- (void)folderPickerDidCancel:(NoteFolderChooserViewController*)folderPicker {
  [self setTableViewEditing:NO];
  [self.navigationController dismissViewControllerAnimated:YES completion:NULL];
  self.folderSelector.delegate = nil;
  self.folderSelector = nil;
}

- (void)folderPickerDidDismiss:(NoteFolderChooserViewController*)folderPicker {
  self.folderSelector.delegate = nil;
  self.folderSelector = nil;
}

#pragma mark - NoteInteractionControllerDelegate

- (void)noteInteractionControllerWillCommitContentChange:
    (NoteInteractionController*)controller {
  [self setTableViewEditing:NO];
}

- (void)noteInteractionControllerDidStop:
    (NoteInteractionController*)controller {
  // TODO(crbug.com/805182): Use this method to tear down
  // |self.noteInteractionController|.
}

#pragma mark - NoteModelBridgeObserver

- (void)noteModelLoaded {
  DCHECK(!_rootNode);
  [self setRootNode:self.notes->main_node()];

  // If the view hasn't loaded yet, then return early. The eventual call to
  // viewDidLoad will properly initialize the views.  This early return must
  // come *after* the call to setRootNode above.
  if (![self isViewLoaded])
    return;

  int64_t unusedFolderId;
  int unusedIndexPathRow;
  // Note Model is loaded after presenting Notes,  we need to check
  // again here if restoring of cache position is needed.  It is to prevent
  // crbug.com/765503.
  if ([NotePathCache
          getNoteTopMostRowCacheWithPrefService:self.profile->GetPrefs()
                                              model:self.notes
                                           folderId:&unusedFolderId
                                         topMostRow:&unusedIndexPathRow]) {
    self.isReconstructingFromCache = YES;
  }

  DCHECK(self.spinnerView);
  __weak NoteHomeViewController* weakSelf = self;
  [self.spinnerView stopWaitingWithCompletion:^{
    // Early return if the controller has been deallocated.
    NoteHomeViewController* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [UIView animateWithDuration:0.2f
        animations:^{
          weakSelf.spinnerView.alpha = 0.0;
        }
        completion:^(BOOL finished) {
          NoteHomeViewController* innerStrongSelf = weakSelf;
          if (!innerStrongSelf)
            return;

          // By the time completion block is called, the backgroundView could be
          // another view, like the empty view background. Only clear the
          // background if it is still the spinner.
          if (innerStrongSelf.sharedState.tableView.backgroundView ==
              innerStrongSelf.spinnerView) {
            innerStrongSelf.sharedState.tableView.backgroundView = nil;
          }
          innerStrongSelf.spinnerView = nil;
        }];
    [strongSelf loadNoteViews];
    [strongSelf.sharedState.tableView reloadData];
  }];
}

- (void)noteNodeChanged:(const NoteNode*)node {
  // No-op here.  Notes might be refreshed in NoteHomeMediator.
}

- (void)noteNodeChildrenChanged:(const NoteNode*)noteNode {
  // No-op here.  Notes might be refreshed in NoteHomeMediator.
}

- (void)noteNode:(const NoteNode*)noteNode
     movedFromParent:(const NoteNode*)oldParent
            toParent:(const NoteNode*)newParent {
  // No-op here.  Notes might be refreshed in NoteHomeMediator.
}

- (void)noteNodeDeleted:(const NoteNode*)node
                 fromFolder:(const NoteNode*)folder {
  if (_rootNode == node) {
    [self setTableViewEditing:NO];
  }
}

- (void)noteModelRemovedAllNodes {
  // No-op
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self dismissWithURL:GURL()];
  return YES;
}

#pragma mark - private

- (BOOL)isDisplayingNoteRoot {
  return _rootNode == self.notes->root_node();
}

// Check if any of our controller is presenting. We don't consider when this
// controller is presenting the search controller.
// Note that when adding a controller that can present, it should be added in
// context here.
- (BOOL)isAnyControllerPresenting {
  return ([self.navigationController presentedViewController]);
}

- (void)setupUIStackCacheIfApplicable {
  self.isReconstructingFromCache = NO;

  NSArray<NoteHomeViewController*>* replacementViewControllers =
      [self cachedViewControllerStack];
  DCHECK(replacementViewControllers);
  [self.navigationController setViewControllers:replacementViewControllers];
}

// Set up context bar for the new UI.
- (void)setupContextBar {
  if (![self isDisplayingNoteRoot] ||
      self.sharedState.currentlyShowingSearchResults) {
    self.navigationController.toolbarHidden = NO;
    [self setContextBarState:NotesContextBarDefault];
  } else {
    self.navigationController.toolbarHidden = YES;
    [self setContextBarState:NotesContextBarNone];
  }
}


// Back button callback for the new ui.
- (void)back {
  [self navigateAway];
  [self.navigationController popViewControllerAnimated:YES];
}


// Saves the current position and asks the delegate to open the url, if delegate
// is set, otherwise opens the URL using URL loading service.
- (void)dismissWithURL:(const GURL&)url {
  [self cacheIndexPathRow];
  if (self.homeDelegate) {
    std::vector<GURL> urls;
    if (url.is_valid())
      urls.push_back(url);
    [self.homeDelegate noteHomeViewControllerWantsDismissal:self
                                               navigationToUrls:urls];
  } else {
    // Before passing the URL to the block, make sure the block has a copy of
    // the URL and not just a reference.
    const GURL localUrl(url);
    __weak NoteHomeViewController* weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf loadURL:localUrl];
    });
  }
}

- (void)loadURL:(const GURL&)url {
  if (url.is_empty() || url.SchemeIs(url::kJavaScriptScheme))
    return;

  UrlLoadParams params = UrlLoadParams::InCurrentTab(url);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

- (void)addNewFolder {
  [self.sharedState.editingFolderCell stopEdit];
  if (!self.sharedState.tableViewDisplayedRootNode) {
    return;
  }
    if (!self.noteInteractionController) {
      self.noteInteractionController =
          [[NoteInteractionController alloc] initWithBrowser:self.browser
                                                parentController:self];
      self.noteInteractionController.delegate = self;
    }
    [self.noteInteractionController presentNoteFolderEditor:nil
      parent:self.sharedState.tableViewDisplayedRootNode
        isEditing:NO];
}

- (void)handleAddBarButtonTap {
    [self addNote];
}

- (NoteHomeViewController*)createControllerWithRootFolder:
    (const vivaldi::NoteNode*)folder {
  NoteHomeViewController* controller =
      [[NoteHomeViewController alloc] initWithBrowser:self.browser];
  [controller setRootNode:folder];
  controller.homeDelegate = self.homeDelegate;
  controller.snackbarCommandsHandler = self.snackbarCommandsHandler;
  return controller;
}

// Sets the editing mode for tableView, update context bar and search state
// accordingly.
- (void)setTableViewEditing:(BOOL)editing {
  self.sharedState.currentlyInEditMode = editing;
  [self setContextBarState:editing ? NotesContextBarBeginSelection
                                   : NotesContextBarDefault];
  self.tableView.dragInteractionEnabled = !editing;
}

// Row selection of the tableView will be cleared after reloadData.  This
// function is used to restore the row selection.  It also updates editNodes in
// case some selected nodes are removed.
- (void)restoreRowSelection {
  // Create a new editNodes set to check if some selected nodes are removed.
  std::set<const vivaldi::NoteNode*> newEditNodes;

  // Add selected nodes to editNodes only if they are not removed (still exist
  // in the table).
  NSArray<TableViewItem*>* items = [self.sharedState.tableViewModel
      itemsInSectionWithIdentifier:NoteHomeSectionIdentifierNotes];
  for (TableViewItem* item in items) {
    NoteHomeNodeItem* nodeItem =
        base::apple::ObjCCastStrict<NoteHomeNodeItem>(item);
    const NoteNode* node = nodeItem.noteNode;
    if (self.sharedState.editNodes.find(node) !=
        self.sharedState.editNodes.end()) {
      newEditNodes.insert(node);
      // Reselect the row of this node.
      NSIndexPath* itemPath =
          [self.sharedState.tableViewModel indexPathForItem:nodeItem];
      [self.sharedState.tableView
          selectRowAtIndexPath:itemPath
                      animated:NO
                scrollPosition:UITableViewScrollPositionNone];
    }
  }

  // if editNodes is changed, update it.
  if (self.sharedState.editNodes.size() != newEditNodes.size()) {
    self.sharedState.editNodes = newEditNodes;
    [self handleSelectEditNodes:self.sharedState.editNodes];
  }
}

- (BOOL)allowsNewFolder {
  // When the current root node has been removed remotely (becomes NULL),
  // or when displaying search results, creating new folder is forbidden.
  // The root folder displayed by the table view must also be editable to allow
  // creation of new folders. Note that Notes Bar, Mobile Notes, and
  // Other Notes return as "editable" since the user can edit the contents
  // of those folders. Editing notes must also be allowed.
  return self.sharedState.tableViewDisplayedRootNode != NULL &&
        !self.sharedState.tableViewDisplayedRootNode->is_trash() &&
         !self.sharedState.currentlyShowingSearchResults &&
         [self isEditNotesEnabled] &&
         [self
             isNodeEditableByUser:self.sharedState.tableViewDisplayedRootNode];
}

- (int)topMostVisibleIndexPathRow {
  // If on root node screen, return 0.
  if (self.sharedState.notesModel && [self isDisplayingNoteRoot]) {
    return 0;
  }

  // If no rows in table, return 0.
  NSArray* visibleIndexPaths = [self.tableView indexPathsForVisibleRows];
  if (!visibleIndexPaths.count)
    return 0;

  // If the first row is still visible, return 0.
  NSIndexPath* topMostIndexPath = [visibleIndexPaths objectAtIndex:0];
  if (topMostIndexPath.row == 0)
    return 0;

  // To avoid an index out of bounds, check if there are less or equal
  // kRowsHiddenByNavigationBar than number of visibleIndexPaths.
  if ([visibleIndexPaths count] <= kRowsHiddenByNavigationBar)
    return 0;

  // Return the first visible row not covered by the NavigationBar.
  topMostIndexPath =
      [visibleIndexPaths objectAtIndex:kRowsHiddenByNavigationBar];
  return topMostIndexPath.row;
}

- (void)navigateAway {
  [self.sharedState.editingFolderCell stopEdit];
  [self.sharedState.editingNoteCell stopEdit];
}

// Returns YES if the given node is a url or folder node.
- (BOOL)isUrlOrFolder:(const NoteNode*)node {
  return node->type() == NoteNode::NOTE ||
         node->type() == NoteNode::FOLDER;
}

// Returns YES if the given node can be edited by user.
- (BOOL)isNodeEditableByUser:(const NoteNode*)node {
  // Note that CanBeEditedByUser() below returns true for Notes
  //  since the user can add, delete, and edit
  // items within those folders. CanBeEditedByUser() returns false for the
  // managed_node and all nodes that are descendants of managed_node.
  return YES;
}

// Returns YES if user is allowed to edit any notes.
- (BOOL)isEditNotesEnabled {
    return YES; //self.browserState->GetPrefs()->GetBoolean(
        //prefs::kEditNotesEnabled);
}

// Returns the note node associated with |indexPath|.
- (const NoteNode*)nodeAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item =
      [self.sharedState.tableViewModel itemAtIndexPath:indexPath];

  if (item.type == NoteHomeItemTypeNote) {
    NoteHomeNodeItem* nodeItem =
        base::apple::ObjCCastStrict<NoteHomeNodeItem>(item);
    return nodeItem.noteNode;
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

- (BOOL)hasItemAtIndexPath:(NSIndexPath*)indexPath {
  return [self.sharedState.tableViewModel hasItemAtIndexPath:indexPath];
}

- (BOOL)hasNotesOrFolders {
  if (!self.sharedState.tableViewDisplayedRootNode)
    return NO;
  if (self.sharedState.currentlyShowingSearchResults) {
    return [self
        hasItemsInSectionIdentifier:NoteHomeSectionIdentifierNotes];
  } else {
    return !self.sharedState.tableViewDisplayedRootNode->children().empty();
  }
}

- (BOOL)hasItemsInSectionIdentifier:(NSInteger)sectionIdentifier {
  BOOL hasSection = [self.sharedState.tableViewModel
      hasSectionForSectionIdentifier:sectionIdentifier];
  if (!hasSection)
    return NO;
  NSInteger section = [self.sharedState.tableViewModel
      sectionForSectionIdentifier:sectionIdentifier];
  return [self.sharedState.tableViewModel numberOfItemsInSection:section] > 0;
}

- (std::vector<const vivaldi::NoteNode*>)editNodes {
  std::vector<const vivaldi::NoteNode*> nodes;
  if (self.sharedState.currentlyShowingSearchResults) {
    // Create a vector of edit nodes in the same order as the selected nodes.
    const std::set<const vivaldi::NoteNode*> editNodes =
        self.sharedState.editNodes;
    std::copy(editNodes.begin(), editNodes.end(), std::back_inserter(nodes));
  } else {
    // Create a vector of edit nodes in the same order as the nodes in folder.
    for (const auto& child :
         self.sharedState.tableViewDisplayedRootNode->children()) {
      if (self.sharedState.editNodes.find(child.get()) !=
          self.sharedState.editNodes.end()) {
        nodes.push_back(child.get());
      }
    }
  }
  return nodes;
}

// Triggers the URL sharing flow for the given |URL| and |title|, with the
// |indexPath| for the cell representing the UI component for that URL.
- (void)shareText:(NSString*)noteText
           title:(NSString*)title
       indexPath:(NSIndexPath*)indexPath {
  UIView* cellView = [self.tableView cellForRowAtIndexPath:indexPath];
  SharingParams* params =
      [[SharingParams alloc] initWithText:noteText
                                     title:title
                                  scenario:SharingScenario::VivaldiNote];
  self.sharingCoordinator =
      [[SharingCoordinator alloc] initWithBaseViewController:self
                                                     browser:self.browser
                                                      params:params
                                                  originView:cellView];
  [self.sharingCoordinator start];
}

// Returns whether the incognito mode is forced.
- (BOOL)isIncognitoForced {
  return IsIncognitoModeForced(self.browser->GetProfile()->GetPrefs());
}

// Returns whether the incognito mode is available.
- (BOOL)isIncognitoAvailable {
  return !IsIncognitoModeDisabled(self.browser->GetProfile()->GetPrefs());
}

#pragma mark - Loading and Empty States

// Shows loading spinner background view.
- (void)showLoadingSpinnerBackground {
  if (!self.spinnerView) {
    self.spinnerView =
        [[HomeWaitingView alloc] initWithFrame:self.sharedState.tableView.bounds
                               backgroundColor:UIColor.clearColor];
    [self.spinnerView startWaiting];
  }
  self.tableView.backgroundView = self.spinnerView;
}

// Hide the loading spinner if it is showing.
- (void)hideLoadingSpinnerBackground {
  if (self.spinnerView) {
    __weak NoteHomeViewController* weakSelf = self;
    [self.spinnerView stopWaitingWithCompletion:^{
      [UIView animateWithDuration:0.2
          animations:^{
            weakSelf.spinnerView.alpha = 0.0;
          }
          completion:^(BOOL finished) {
            NoteHomeViewController* strongSelf = weakSelf;
            if (!strongSelf)
              return;
            // By the time completion block is called, the backgroundView could
            // be another view, like the empty view background. Only clear the
            // background if it is still the spinner.
            if (strongSelf.sharedState.tableView.backgroundView ==
                strongSelf.spinnerView) {
              strongSelf.sharedState.tableView.backgroundView = nil;
            }
            strongSelf.spinnerView = nil;
          }];
    }];
  }
}

// Shows empty notes background view.
- (void)showEmptyBackground {
  if (!self.emptyViewBackground) {
    NSString* titleString = [self isDisplayingNoteRoot]
    ? GetNSString(IDS_VIVALDI_NOTE_EMPTY_TITLE)
    : GetNSString(IDS_VIVALDI_FOLDER_NO_ITEMS);

    NSString* subtitleString = [self isDisplayingNoteRoot]
    ? GetNSString(IDS_VIVALDI_NOTE_EMPTY_MESSAGE)
    : @"";

    self.emptyViewBackground = [[TableViewIllustratedEmptyView alloc]
                                initWithFrame:self.sharedState.tableView.bounds
                                image:[UIImage imageNamed:@"vivaldi_notes_empty"]
                                title:titleString
                                subtitle:subtitleString];
  }
  // If the Signin promo is visible on the root view, we have to shift the
  // empty TableView background to make it fully visible on all devices.
  if ([self isDisplayingNoteRoot]) {
    // Reload the data to ensure consistency between the model and the table
    // (an example scenario can be found at crbug.com/1116408). Reloading the
    // data should only be done for the root note folder since it can be
    // very expensive in other folders.
    [self.sharedState.tableView reloadData];

    self.navigationItem.largeTitleDisplayMode =
    UINavigationItemLargeTitleDisplayModeNever;
    self.emptyViewBackground.scrollViewContentInsets =
    self.view.safeAreaInsets;
  }

  self.sharedState.tableView.backgroundView = self.emptyViewBackground;
  self.navigationItem.searchController = nil;

  self.vivaldiSearchBarView.hidden = YES;
}

- (void)hideEmptyBackground {
  if (self.sharedState.tableView.backgroundView == self.emptyViewBackground) {
    self.sharedState.tableView.backgroundView = nil;
  }

  self.vivaldiSearchBarView.hidden = NO;
}

#pragma mark - ContextBarDelegate implementation

// Called when the leading button is clicked. (New Folder)
- (void)leadingButtonClicked {
  // Ignore the button tap if any of our controllers is presenting.
  if ([self isAnyControllerPresenting]) {
    return;
  }
  const std::set<const vivaldi::NoteNode*> nodes =
      self.sharedState.editNodes;
  switch (self.contextBarState) {
    case NotesContextBarDefault:
      // New Folder clicked.
      [self addNewFolder];
      break;
    case NotesContextBarBeginSelection:
      // This must never happen, as the leading button is disabled at this
      // point.
      NOTREACHED_IN_MIGRATION();
      break;
    case NotesContextBarSingleNoteSelection:
    case NotesContextBarMultipleNoteSelection:
    case NotesContextBarSingleFolderSelection:
    case NotesContextBarMultipleFolderSelection:
    case NotesContextBarMixedSelection:
      // Delete clicked.
      if (_rootNode->is_trash()) {
        [self deleteNodes:nodes];
      } else {
        [self moveNodesToTrash:nodes];
      }
      break;
    case NotesContextBarNone:
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

// Called when the center button is clicked.
- (void)centerButtonClicked {
  // Ignore the button tap if any of our controller is presenting.
  if ([self isAnyControllerPresenting]) {
    return;
  }
  const std::set<const vivaldi::NoteNode*> nodes =
      self.sharedState.editNodes;
  // Center button is shown and is clickable only when at least
  // one node is selected.
  DCHECK(nodes.size() > 0);

  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                           title:nil
                         message:nil
                   barButtonItem:self.moreButton];

  switch (self.contextBarState) {
    case NotesContextBarSingleNoteSelection:
      [self configureCoordinator:self.actionSheetCoordinator
            forSingleNote:*(nodes.begin())];
      break;
    case NotesContextBarMultipleNoteSelection:
      [self configureCoordinator:self.actionSheetCoordinator
          forMultipleNotes:nodes];
      break;
    case NotesContextBarSingleFolderSelection:
      [self configureCoordinator:self.actionSheetCoordinator
          forSingleNoteFolder:*(nodes.begin())];
      break;
    case NotesContextBarMultipleFolderSelection:
    case NotesContextBarMixedSelection:
      [self configureCoordinator:self.actionSheetCoordinator
          forMixedAndMultiFolderSelection:nodes];
      break;
    case NotesContextBarDefault:
    case NotesContextBarBeginSelection:
    case NotesContextBarNone:
      // Center button is disabled in these states.
      NOTREACHED_IN_MIGRATION();
      break;
  }

  [self.actionSheetCoordinator start];
}

// Called when the trailing button, "Select" or "Cancel" is clicked.
- (void)trailingButtonClicked {
  // Ignore the button tap if any of our controller is presenting.
  if ([self isAnyControllerPresenting]) {
    return;
  }
  // Toggle edit mode.
  [self setTableViewEditing:!self.sharedState.currentlyInEditMode];
}

// Called when the empty trash button is clicked.
- (void)emptyTrashButtonClicked {
  // Ignore the button tap if any of our controller is presenting.
  if ([self isAnyControllerPresenting]) {
    return;
  }
  std::set<const NoteNode*> nodes;
  for (const auto& child : _rootNode->children()) {
      nodes.insert(child.get());
  }
  DCHECK(nodes.size() > 0);
  [self deleteNodes:nodes];
}

- (void)handleRefreshContextBar {
  // At default state, the enable state of context bar buttons could change
  // during refresh.
  if (self.contextBarState == NotesContextBarDefault) {
    [self setNotesContextBarButtonsDefaultState];
  }
}

#pragma mark - ContextBarStates

// Customizes the context bar buttons based the |state| passed in.
- (void)setContextBarState:(NotesContextBarState)state {
  _contextBarState = state;
  switch (state) {
    case NotesContextBarDefault:
      [self setNotesContextBarButtonsDefaultState];
      break;
    case NotesContextBarBeginSelection:
      [self setNotesContextBarSelectionStartState];
      break;
    case NotesContextBarSingleNoteSelection:
    case NotesContextBarMultipleNoteSelection:
    case NotesContextBarMultipleFolderSelection:
    case NotesContextBarMixedSelection:
    case NotesContextBarSingleFolderSelection:
      // Reset to start state, and then override with customizations that apply.
      [self setNotesContextBarSelectionStartState];
      self.moreButton.enabled = YES;
      self.deleteButton.enabled = YES;
      break;
    case NotesContextBarNone:
    default:
      break;
  }
}

- (void)setNotesContextBarButtonsDefaultState {
  // Set Context Menu button for sorting and new folder
  UIImage* dotsIcon = [UIImage imageNamed:vPanelMoreAction];
  UIBarButtonItem* contextMenu =
  [[UIBarButtonItem alloc]
   initWithImage:dotsIcon
           style:UIBarButtonItemStyleDone
          target:self
          action:nil];
  contextMenu.menu = [self contextMenuForNotesSortButton];

  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  // Set Edit button.
  NSString* titleString = GetNSString(IDS_IOS_NOTE_CONTEXT_BAR_EDIT);
  UIBarButtonItem* editButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(trailingButtonClicked)];
  editButton.accessibilityIdentifier = kNoteHomeTrailingButtonIdentifier;
  // The edit button is only enabled if the displayed root folder is editable
  // and has items.
  editButton.enabled =
      [self isEditNotesEnabled] && [self hasNotesOrFolders] &&
      [self isNodeEditableByUser:self.sharedState.tableViewDisplayedRootNode];

  if (!_rootNode->is_trash()) {
      UIImage* image = [UIImage systemImageNamed:@"plus"];
      UIBarButtonItem* plusButton =
       [[UIBarButtonItem alloc]
        initWithImage:image
        style:UIBarButtonItemStyleDone
        target:self
        action:@selector(handleAddBarButtonTap)];
      [self setToolbarItems:@[ contextMenu, spaceButton,
                             plusButton, spaceButton, editButton ]
                             animated:NO];
  } else {
    // Set empty trash button.
    titleString = GetNSString(IDS_IOS_NOTE_CONTEXT_BAR_EMPTY_TRASH);
    UIBarButtonItem* emptyTrashButton =
        [[UIBarButtonItem alloc] initWithTitle:titleString
                                      style:UIBarButtonItemStylePlain
                                     target:self
                                     action:@selector(emptyTrashButtonClicked)];
    emptyTrashButton.accessibilityIdentifier =
      kNoteHomeTrailingButtonIdentifier;
    emptyTrashButton.enabled = [self hasNotesOrFolders];

    [self setToolbarItems:@[ spaceButton, spaceButton,
                           spaceButton, spaceButton, emptyTrashButton ]
                           animated:NO];
  }
}

/// Returns the context menu actions for notes sort button action
- (UIMenu*)contextMenuForNotesSortButton {
  // Sync action button
  NSString* syncTitleString =
      GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_SYNCHRONIZATION);
  UIAction* syncAction =
  [UIAction actionWithTitle:syncTitleString
                      image:nil
                 identifier:nil
                    handler:^(__kindof UIAction*_Nonnull action) {
    [self showVivaldiSync];
  }];

  //New Folder action Button
  NSString* titleString = GetNSString(IDS_IOS_NOTE_CONTEXT_BAR_NEW_FOLDER);
  UIAction* newFolderAction =
      [UIAction actionWithTitle:titleString
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                  action) {
    [self leadingButtonClicked];
  }];

  // Sort by manual action button
  titleString = GetNSString(IDS_IOS_NOTE_SORTING_MANUAL);
  UIAction* manualSortAction =
      [UIAction actionWithTitle:titleString
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                 action) {
    [self sortNotesWithMode:NotesSortingModeManual];
  }];
  self.manualSortAction = manualSortAction;

  // Sort by title action button
  titleString = GetNSString(IDS_IOS_NOTE_SORTING_BY_TITLE);
  UIAction* titleSortAction =
      [UIAction actionWithTitle:titleString
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                  action) {
    [self sortNotesWithMode:NotesSortingModeTitle];
  }];
  self.titleSortAction = titleSortAction;

  // Sort by date created action button
  titleString = GetNSString(IDS_IOS_NOTE_SORTING_DATE_CREATED);
  UIAction* dateCreatedSortAction =
      [UIAction actionWithTitle:titleString
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                  action) {
    [self sortNotesWithMode:NotesSortingModeDateCreated];
  }];
  self.dateCreatedSortAction = dateCreatedSortAction;

  // Sort by date edited action button
  titleString = GetNSString(IDS_IOS_NOTE_SORTING_DATE_EDITED);
  UIAction* dateEditedSortAction =
      [UIAction actionWithTitle:titleString
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                 action) {
    [self sortNotesWithMode:NotesSortingModeDateEdited];
  }];
  self.dateEditedSortAction = dateEditedSortAction;

  // Sort by kind action button
  titleString = GetNSString(IDS_IOS_SORT_BY_KIND);
  UIAction* kindSortAction =
      [UIAction actionWithTitle:titleString
                          image:nil
                     identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                 action) {
    [self sortNotesWithMode:NotesSortingModeByKind];
  }];
  self.kindSortAction = kindSortAction;

  // Ascending sort action button
  titleString = GetNSString(IDS_IOS_NOTE_ASCENDING_SORT_ORDER);
  UIAction* ascendingSortAction =
      [UIAction actionWithTitle:titleString
                          image:nil
                      identifier:nil
                         handler:^(__kindof UIAction*_Nonnull
                                  action) {
        [self refreshSortOrder:NotesSortingOrderAscending];
      }];
  self.ascendingSortAction = ascendingSortAction;

  // Descending sort action button
  titleString = GetNSString(IDS_IOS_NOTE_DESCENDING_SORT_ORDER);
  UIAction* descendingSortAction =
      [UIAction actionWithTitle:titleString
                          image:nil
                      identifier:nil
                        handler:^(__kindof UIAction*_Nonnull
                                  action) {
        [self refreshSortOrder:NotesSortingOrderDescending];
      }];
  self.descendingSortAction = descendingSortAction;

  if (!self.isSortedByManual) {
    _sortOrderActions = [[NSMutableArray alloc] initWithArray:@[
      ascendingSortAction, descendingSortAction]
    ];
  } else {
    _sortOrderActions = nil;
  }

  // Refresh actions buttons states
  [self updateSortOrderStateOnContextMenuOption];

  UIMenu *sortMenu =
      [UIMenu menuWithTitle:@""
                      image:nil
                  identifier:nil
                    options:UIMenuOptionsDisplayInline
                   children:_sortOrderActions];

  _notesSortActions = [[NSMutableArray alloc] initWithArray:@[
    manualSortAction, titleSortAction,
    dateCreatedSortAction, dateEditedSortAction, kindSortAction]
  ];

  // Refresh actions buttons states
  [self setSortingStateOnContextMenuOption];

  titleString = GetNSString(IDS_IOS_NOTE_SORTING_SORT_ORDER);
  UIMenu *sortOrderMenu =
    [UIMenu menuWithTitle:@""
                    image:nil
               identifier:nil
                  options:UIMenuOptionsDisplayInline
                 children:_notesSortActions];

  UIMenuOptions options = UIMenuOptionsDisplayInline;

  // iOS allowed multiple selection from iOS17 onwords
  // see https://developer.apple.com/documentation/uikit/uimenu/options/4195440-displayaspalette
  // for fallback users it will show as inline (in single list format not the collapsable list format)
  if (@available(iOS 17.0, *)) {
    options = UIMenuOptionsDisplayAsPalette;
  }

  UIMenu* subMenu =
    [UIMenu menuWithTitle:titleString
                    image:[UIImage imageNamed:vPanelSortOrderAction]
               identifier:nil
                  options:options
                 children:@[sortOrderMenu, sortMenu]];
  UIMenu* menu = [UIMenu menuWithTitle:@""
                              children:@[subMenu, newFolderAction, syncAction]];
  return menu;
}

-(void)refreshSortOrder: (NotesSortingOrder) order {
  if (self.currentSortingOrder == order)
    return;
  [self setSortingOrder:order];
  [self refreshSortingViewWith:self.currentSortingMode];
}

/// Sets current sorting mode selected by the user on the pref and calls the mediator to handle the sorting
/// and refreshes the UI with sorted items.
- (void)sortNotesWithMode:(NotesSortingMode)mode {
  if (self.currentSortingMode == mode)
    return;
  // Reset the sorting order to ascending
  [self setSortingOrder:NotesSortingOrderAscending];
  [self refreshSortingViewWith: mode];
}

-(void)refreshSortingViewWith: (NotesSortingMode)mode {
  // Set new mode to the pref.
  [self setCurrentSortingMode:mode];

  // Refresh context bar
  [self handleRefreshContextBar];
   // Restore current list.
  [self.mediator computeNoteTableViewData];
  [self.sharedState.tableView reloadData];
}

/// Returns current sorting mode
- (NotesSortingMode)currentSortingMode {
  return [VivaldiNotesPrefs getNotesSortingMode];
}

/// Sets the user selected sorting mode on the prefs
- (void)setCurrentSortingMode:(NotesSortingMode)mode {
  // need to initalize the prefs first
  [VivaldiNotesPrefs setNotesSortingMode:mode];
}

/// Returns YES if the current sorting mode is manual
- (BOOL)isSortedByManual {
  return self.currentSortingMode == NotesSortingModeManual;
}

/// Gets the sorting mode from prefs and update the selection state on the context menu.
- (void)setSortingStateOnContextMenuOption {
  switch (self.currentSortingMode) {
    case NotesSortingModeManual:
      [self updateSortActionButtonState:self.manualSortAction];
      break;
    case NotesSortingModeTitle:
      [self updateSortActionButtonState:self.titleSortAction];
      break;
    case NotesSortingModeDateCreated:
      [self updateSortActionButtonState:self.dateCreatedSortAction];
      break;
    case NotesSortingModeDateEdited:
      [self updateSortActionButtonState:self.dateEditedSortAction];
      break;
    case NotesSortingModeByKind:
      [self updateSortActionButtonState:self.kindSortAction];
      break;
  }
}

/// Updates the state on the context menu actions by unchecking the all apart from the selected one.
- (void)updateSortActionButtonState:(UIAction*)settable {
  for (UIAction* action in self.notesSortActions) {
    if (action == settable) {
      [action setState:UIMenuElementStateOn];
    } else {
      [action setState:UIMenuElementStateOff];
    }
  }
}

// Updates the state on the context menu actions for sorting order
// by unchecking the all apart from the selected one.
- (void) updateSortOrderStateOnContextMenuOption {
  if (self.currentSortingOrder == NotesSortingOrderAscending) {
    [self updateSortOrderActionButtonState:self.ascendingSortAction];
  } else {
    [self updateSortOrderActionButtonState:self.descendingSortAction];
  }
}

/// set sorting order on the prefs
- (void) setSortingOrder: (NotesSortingOrder) order {
  [VivaldiNotesPrefs setNotesSortingOrder:order];
}

/// Returns current sorting order
- (NotesSortingOrder)currentSortingOrder {
  return [VivaldiNotesPrefs getNotesSortingOrder];
}

- (void)updateSortOrderActionButtonState:(UIAction*)settable {
  for (UIAction* action in self.sortOrderActions) {
    if (action == settable) {
      [action setState:UIMenuElementStateOn];
    } else {
      [action setState:UIMenuElementStateOff];
    }
  }
}

- (void)setNotesContextBarSelectionStartState {
  self.navigationItem.rightBarButtonItem.enabled = NO;
  // Disabled Delete button.
  NSString* titleString = GetNSString(IDS_VIVALDI_NOTE_CONTEXT_BAR_DELETE);
  self.deleteButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(leadingButtonClicked)];
  self.deleteButton.tintColor = [UIColor colorNamed:kRedColor];
  self.deleteButton.enabled = NO;
  self.deleteButton.accessibilityIdentifier =
      kNoteHomeLeadingButtonIdentifier;

  // Disabled More button. // More button in right corner, open popup menu
  titleString = GetNSString(IDS_VIVALDI_NOTE_CONTEXT_BAR_MORE);
  self.moreButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(centerButtonClicked)];
  self.moreButton.enabled = NO;
  self.moreButton.accessibilityIdentifier = kNoteHomeCenterButtonIdentifier;

  // Enabled Cancel button.
  titleString = GetNSString(IDS_CANCEL);
  UIBarButtonItem* cancelButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(trailingButtonClicked)];
  cancelButton.accessibilityIdentifier = kNoteHomeTrailingButtonIdentifier;

  // Spacer button.
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  [self setToolbarItems:@[
    self.deleteButton, spaceButton, self.moreButton, spaceButton, cancelButton
  ]
               animated:NO];
}

#pragma mark - Context Menu

- (void)configureCoordinator:(AlertCoordinator*)coordinator
     forMultipleNotes:(const std::set<const NoteNode*>)nodes {
  __weak NoteHomeViewController* weakSelf = self;
  coordinator.alertController.view.accessibilityIdentifier =
      kNoteHomeContextMenuIdentifier;
  std::set<int64_t> nodeIds;
  for (const NoteNode* node : nodes) {
    nodeIds.insert(node->id());
  }

  NSString* titleString = GetNSString(IDS_VIVALDI_NOTE_CONTEXT_MENU_MOVE);
  [coordinator
      addItemWithTitle:titleString
                action:^{
                  NoteHomeViewController* strongSelf = weakSelf;
                  if (!strongSelf)
                    return;

                  std::optional<std::set<const NoteNode*>> nodesFromIds =
                      note_utils_ios::FindNodesByIds(strongSelf.notes,
                                                         nodeIds);
                  if (nodesFromIds)
                    [strongSelf moveNodes:*nodesFromIds];
                }
                 style:UIAlertActionStyleDefault];
}

- (void)configureCoordinator:(AlertCoordinator*)coordinator
        forSingleNote:(const NoteNode*)node {
  __weak NoteHomeViewController* weakSelf = self;
  std::string urlString = node->GetURL().possibly_invalid_spec();
  coordinator.alertController.view.accessibilityIdentifier =
      kNoteHomeContextMenuIdentifier;

  int64_t nodeId = node->id();
  NSString* titleString = GetNSString(IDS_VIVALDI_NOTE_CONTEXT_MENU_EDIT);

  // Disable the edit menu option if the node is not editable by user, or if
  // editing notes is not allowed.
  BOOL editEnabled =
      [self isEditNotesEnabled] && [self isNodeEditableByUser:node];

  [coordinator addItemWithTitle:titleString
                         action:^{
                           NoteHomeViewController* strongSelf = weakSelf;
                           if (!strongSelf)
                             return;
                           const vivaldi::NoteNode* nodeFromId =
                               note_utils_ios::FindNodeById(
                                   strongSelf.notes, nodeId);
                           if (nodeFromId)
                             [strongSelf editNode:nodeFromId];
                         }
                          style:UIAlertActionStyleDefault
                        enabled:editEnabled];

    std::set<int64_t> nodeIds;
    nodeIds.insert(node->id());
    titleString = GetNSString(IDS_VIVALDI_NOTE_CONTEXT_MENU_MOVE);
    [coordinator
        addItemWithTitle:titleString
                  action:^{
                    NoteHomeViewController* strongSelf = weakSelf;
                    if (!strongSelf)
                      return;

                    std::optional<std::set<const NoteNode*>> nodesFromIds =
                        note_utils_ios::FindNodesByIds(strongSelf.notes,
                                                           nodeIds);
                    if (nodesFromIds)
                      [strongSelf moveNodes:*nodesFromIds];
                  }
     style:UIAlertActionStyleDefault];
  GURL nodeURL = node->GetURL();
  if (!nodeURL.is_empty()) {
      titleString = GetNSString(IDS_VIVALDI_NOTE_CONTEXT_MENU_OPEN);
      [coordinator addItemWithTitle:titleString
                         action:^{
                           if ([weakSelf isIncognitoForced])
                             return;
                           [weakSelf openAllURLs:{nodeURL}
                                     inIncognito:NO
                                          newTab:YES];
                         }
                          style:UIAlertActionStyleDefault
                        enabled:![self isIncognitoForced]];
      titleString = GetNSString(IDS_IOS_CONTENT_CONTEXT_COPY);
      [coordinator
       addItemWithTitle:titleString
                action:^{
                  // Use strongSelf even though the object is only used once
                  // because we do not want to change the global pasteboard
                  // if the view has been deallocated.
                  NoteHomeViewController* strongSelf = weakSelf;
                  if (!strongSelf)
                    return;
                  UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
                  pasteboard.string = base::SysUTF8ToNSString(urlString);
                  [strongSelf setTableViewEditing:NO];
                }
                 style:UIAlertActionStyleDefault];
  }
}

- (void)configureCoordinator:(AlertCoordinator*)coordinator
     forSingleNoteFolder:(const NoteNode*)node {
  __weak NoteHomeViewController* weakSelf = self;
  coordinator.alertController.view.accessibilityIdentifier =
      kNoteHomeContextMenuIdentifier;

  int64_t nodeId = node->id();
  NSString* titleString =
      GetNSString(IDS_VIVALDI_NOTE_CONTEXT_MENU_EDIT_FOLDER);
  // Disable the edit and move menu options if the folder is not editable by
  // user, or if editing notes is not allowed.
  BOOL editEnabled =
      [self isEditNotesEnabled] && [self isNodeEditableByUser:node];

  [coordinator addItemWithTitle:titleString
                         action:^{
                           NoteHomeViewController* strongSelf = weakSelf;
                           if (!strongSelf)
                             return;
                           const vivaldi::NoteNode* nodeFromId =
                               note_utils_ios::FindNodeById(
                                   strongSelf.notes, nodeId);
                           if (nodeFromId)
                             [strongSelf editNode:nodeFromId];
                         }
                          style:UIAlertActionStyleDefault
                        enabled:editEnabled];

  titleString = GetNSString(IDS_VIVALDI_NOTE_CONTEXT_MENU_MOVE);
  [coordinator addItemWithTitle:titleString
                         action:^{
                           NoteHomeViewController* strongSelf = weakSelf;
                           if (!strongSelf)
                             return;
                           const vivaldi::NoteNode* nodeFromId =
                               note_utils_ios::FindNodeById(
                                   strongSelf.notes, nodeId);
                           if (nodeFromId) {
                             std::set<const NoteNode*> nodes{nodeFromId};
                             [strongSelf moveNodes:nodes];
                           }
                         }
                          style:UIAlertActionStyleDefault
                        enabled:editEnabled];
}

- (void)configureCoordinator:(AlertCoordinator*)coordinator
    forMixedAndMultiFolderSelection:
        (const std::set<const vivaldi::NoteNode*>)nodes {
  __weak NoteHomeViewController* weakSelf = self;
  coordinator.alertController.view.accessibilityIdentifier =
      kNoteHomeContextMenuIdentifier;

  std::set<int64_t> nodeIds;
  for (const vivaldi::NoteNode* node : nodes) {
    nodeIds.insert(node->id());
  }

  NSString* titleString = GetNSString(IDS_VIVALDI_NOTE_CONTEXT_MENU_MOVE);
  [coordinator
      addItemWithTitle:titleString
                action:^{
                  NoteHomeViewController* strongSelf = weakSelf;
                  if (!strongSelf)
                    return;
                  std::optional<std::set<const vivaldi::NoteNode*>>
                      nodesFromIds = note_utils_ios::FindNodesByIds(
                          strongSelf.notes, nodeIds);
                  if (nodesFromIds) {
                    [strongSelf moveNodes:*nodesFromIds];
                  }
                }
                 style:UIAlertActionStyleDefault];
}

#pragma mark - UIGestureRecognizerDelegate and gesture handling

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  if (gestureRecognizer ==
      self.navigationController.interactivePopGestureRecognizer) {
    return self.navigationController.viewControllers.count > 1;
  }
  return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Ignore long press in edit mode or search mode.
  if (self.sharedState.currentlyInEditMode) {
    return NO;
  }
  return YES;
}

- (void)handleLongPress:(UILongPressGestureRecognizer*)gestureRecognizer {
  if (self.sharedState.currentlyInEditMode ||
      gestureRecognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }
  CGPoint touchPoint =
      [gestureRecognizer locationInView:self.sharedState.tableView];
  NSIndexPath* indexPath =
      [self.sharedState.tableView indexPathForRowAtPoint:touchPoint];

  if (![self canShowContextMenuFor:indexPath]) {
    return;
  }
  const NoteNode* node = [self nodeAtIndexPath:indexPath];

  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                           title:nil
                         message:nil
                            rect:CGRectMake(touchPoint.x, touchPoint.y, 1, 1)
                            view:self.tableView];

  if (node->is_folder()) {
    [self configureCoordinator:self.actionSheetCoordinator
        forSingleNoteFolder:node];
  } else {
    [self configureCoordinator:self.actionSheetCoordinator
            forSingleNote:node];
    return;
  }

  [self.actionSheetCoordinator start];
}

- (BOOL)canShowContextMenuFor:(NSIndexPath*)indexPath {
  if (indexPath == nil || [self.sharedState.tableViewModel
                              sectionIdentifierForSectionIndex:indexPath.section] !=
                              NoteHomeSectionIdentifierNotes) {
    return NO;
  }

  const NoteNode* node = [self nodeAtIndexPath:indexPath];
  // Don't show context menus for permanent nodes, which include Notes Main,
  // Trash, Other Notes. Permanent nodes
  // do not include descendants of Managed Notes. Also, context menus are
  // only supported on notes or folders.
  return (node && !node->is_permanent_node() &&
          (node->is_folder() || node->is_note()));
}

#pragma mark - NoteHomeSharedStateObserver

- (void)sharedStateDidClearEditNodes:(NoteHomeSharedState*)sharedState {
  [self handleSelectEditNodes:sharedState.editNodes];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [super tableView:tableView cellForRowAtIndexPath:indexPath];
  TableViewItem* item =
      [self.sharedState.tableViewModel itemAtIndexPath:indexPath];

  cell.userInteractionEnabled = (item.type != NoteHomeItemTypeMessage);

  if (item.type == NoteHomeItemTypeNote) {
    NoteHomeNodeItem* nodeItem =
        base::apple::ObjCCastStrict<NoteHomeNodeItem>(item);
    if ((nodeItem.noteNode->is_folder() &&
        nodeItem.noteNode == self.sharedState.editingFolderNode)
        ) {
      TableViewNoteFolderCell* tableCell =
          base::apple::ObjCCastStrict<TableViewNoteFolderCell>(cell);
      // Delay starting edit, so that the cell is fully created. This is
      // needed when scrolling away and then back into the editingCell,
      // without the delay the cell will resign first responder before its
      // created.
      __weak NoteHomeViewController* weakSelf = self;
      dispatch_async(dispatch_get_main_queue(), ^{
        NoteHomeViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        strongSelf.sharedState.editingFolderCell = tableCell;
        [tableCell startEdit];
        tableCell.textDelegate = strongSelf;
      });
    }
  }

  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item =
      [self.sharedState.tableViewModel itemAtIndexPath:indexPath];
  if (item.type != NoteHomeItemTypeNote) {
    // Can only edit notes.
    return NO;
  }

  // If the cell at |indexPath| is being edited (which happens when creating a
  // new Folder) return NO.
  if ([tableView indexPathForCell:self.sharedState.editingFolderCell] ==
      indexPath) {
    return NO;
  }
  // If the cell at |indexPath| is being edited (which happens when creating a
  // new Folder) return NO.
  if ([tableView indexPathForCell:self.sharedState.editingNoteCell] ==
      indexPath) {
    return NO;
  }

  // Enable the swipe-to-delete gesture and reordering control for editable
  // nodes of type Note or Folder, but not the permanent ones. Only enable
  // swipe-to-delete if editing notes is allowed.
  NoteHomeNodeItem* nodeItem =
      base::apple::ObjCCastStrict<NoteHomeNodeItem>(item);
  const NoteNode* node = nodeItem.noteNode;
  return [self isEditNotesEnabled] && [self isUrlOrFolder:node] &&
         [self isNodeEditableByUser:node];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item =
      [self.sharedState.tableViewModel itemAtIndexPath:indexPath];
  if (item.type != NoteHomeItemTypeNote) {
    // Can only commit edits for notes.
    return;
  }

  if (editingStyle == UITableViewCellEditingStyleDelete) {
    NoteHomeNodeItem* nodeItem =
        base::apple::ObjCCastStrict<NoteHomeNodeItem>(item);
    const NoteNode* node = nodeItem.noteNode;
    std::set<const NoteNode*> nodes;
    nodes.insert(node);
    [self handleSelectNodesForDeletion:nodes];
  }
}

- (BOOL)tableView:(UITableView*)tableView
    canMoveRowAtIndexPath:(NSIndexPath*)indexPath {
  // Can only move nodes if the current sorting mode is manual.
  if(self.currentSortingMode != NotesSortingModeManual) {
    return NO;
  }
  // No reorering with filtered results or when displaying the top-most
  // Notes node.
  if (self.sharedState.currentlyShowingSearchResults ||
      [self isDisplayingNoteRoot] || !self.tableView.editing) {
    return NO;
  }
  TableViewItem* item =
      [self.sharedState.tableViewModel itemAtIndexPath:indexPath];
  if (item.type != NoteHomeItemTypeNote) {
    // Can only move notes.
    return NO;
  }

  return YES;
}

- (void)tableView:(UITableView*)tableView
    moveRowAtIndexPath:(NSIndexPath*)sourceIndexPath
           toIndexPath:(NSIndexPath*)destinationIndexPath {
  if (sourceIndexPath.row == destinationIndexPath.row ||
      self.sharedState.currentlyShowingSearchResults) {
    return;
  }
  const NoteNode* node = [self nodeAtIndexPath:sourceIndexPath];
  // Calculations: Assume we have 3 nodes A B C. Node positions are A(0), B(1),
  // C(2) respectively. When we move A to after C, we are moving node at index 0
  // to 3 (position after C is 3, in terms of the existing contents). Hence add
  // 1 when moving forward. When moving backward, if C(2) is moved to Before B,
  // we move node at index 2 to index 1 (position before B is 1, in terms of the
  // existing contents), hence no change in index is necessary. It is required
  // to make these adjustments because this is how note_model handles move
  // operations.
  int newPosition = sourceIndexPath.row < destinationIndexPath.row
                        ? destinationIndexPath.row + 1
                        : destinationIndexPath.row;
  [self handleMoveNode:node toPosition:newPosition];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return ChromeTableViewHeightForHeaderInSection(section);
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewAutomaticDimension;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier = [self.sharedState.tableViewModel
      sectionIdentifierForSectionIndex:indexPath.section];
  if (sectionIdentifier == NoteHomeSectionIdentifierNotes) {
    const NoteNode* node = [self nodeAtIndexPath:indexPath];
    DCHECK(node);
    // If table is in edit mode, record all the nodes added to edit set.
    if (self.sharedState.currentlyInEditMode) {
      if ([self isNodeEditableByUser:node]) {
        // Only add nodes that are editable to the edit set.
        self.sharedState.editNodes.insert(node);
        [self handleSelectEditNodes:self.sharedState.editNodes];
        return;
      }
      // If the selected row is not editable, do not add it to the edit set.
      // Simply deselect the row.
      [tableView deselectRowAtIndexPath:indexPath animated:YES];
      return;
    }
    [self.sharedState.editingFolderCell stopEdit];
    [self.sharedState.editingNoteCell stopEdit];

    if (node->is_folder()) {
      [self handleSelectFolderForNavigation:node];
    } else {
      // Open note editor
      [self editNode:node];
    }
  }
  // Deselect row.
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier = [self.sharedState.tableViewModel
      sectionIdentifierForSectionIndex:indexPath.section];
  if (sectionIdentifier == NoteHomeSectionIdentifierNotes &&
      self.sharedState.currentlyInEditMode) {
    const NoteNode* node = [self nodeAtIndexPath:indexPath];
    DCHECK(node);
    self.sharedState.editNodes.erase(node);
    [self handleSelectEditNodes:self.sharedState.editNodes];
  }
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  if (self.sharedState.currentlyInEditMode) {
    // Don't show the context menu when currently in editing mode.
    return nil;
  }

  if (![self canShowContextMenuFor:indexPath]) {
    return nil;
  }
  const NoteNode* node = [self nodeAtIndexPath:indexPath];

  // Disable the edit and move menu options if the node is not editable by user,
  // or if editing notes is not allowed.
  BOOL canEditNode =
      [self isEditNotesEnabled] && [self isNodeEditableByUser:node];
  UIContextMenuActionProvider actionProvider;

  int64_t nodeId = node->id();
  __weak NoteHomeViewController* weakSelf = self;
  if (node->is_note()) {
    GURL nodeURL = node->GetURL();
    actionProvider = ^(NSArray<UIMenuElement*>* suggestedActions) {
      NoteHomeViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return [UIMenu menuWithTitle:@"" children:@[]];

      BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
          initWithBrowser:strongSelf.browser
                 scenario:kMenuScenarioHistogramNoteEntry];

      NSMutableArray<UIMenuElement*>* menuElements =
            [[NSMutableArray alloc] init];


        UIAction* editAction = [actionFactory actionToEditWithBlock:^{
          NoteHomeViewController* innerStrongSelf = weakSelf;
          if (!innerStrongSelf)
            return;
          const vivaldi::NoteNode* nodeFromId =
              note_utils_ios::FindNodeById(innerStrongSelf.notes, nodeId);
          if (nodeFromId) {
            [innerStrongSelf editNode:nodeFromId];
          }
        }];
        [menuElements addObject:editAction];

        [menuElements
            addObject:[actionFactory actionToShareWithBlock:^{
              NoteHomeViewController* innerStrongSelf = weakSelf;
              if (!innerStrongSelf)
                return;
              const vivaldi::NoteNode* nodeFromId =
                  note_utils_ios::FindNodeById(innerStrongSelf.notes, nodeId);
              if (nodeFromId) {
                [weakSelf
                    shareText:base::SysUTF16ToNSString(nodeFromId->GetContent())
                       title:note_utils_ios::TitleForNoteNode(nodeFromId)
                    indexPath:indexPath];
              }
            }]];

        UIAction* deleteAction = [actionFactory actionToDeleteWithBlock:^{
          NoteHomeViewController* innerStrongSelf = weakSelf;
          if (!innerStrongSelf)
            return;
          const vivaldi::NoteNode* nodeFromId =
              note_utils_ios::FindNodeById(innerStrongSelf.notes, nodeId);
          if (nodeFromId) {
            std::set<const NoteNode*> nodes{nodeFromId};
            [innerStrongSelf handleSelectNodesForDeletion:nodes];
          }
        }];
        [menuElements addObject:deleteAction];

        // Disable Edit and Delete if the node cannot be edited.
        if (!canEditNode) {
          editAction.attributes = UIMenuElementAttributesDisabled;
          deleteAction.attributes = UIMenuElementAttributesDisabled;
        }

        return [UIMenu menuWithTitle:@"" children:menuElements];
      };
    } else if (node->is_folder()) {
      actionProvider = ^(NSArray<UIMenuElement*>* suggestedActions) {
        NoteHomeViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return [UIMenu menuWithTitle:@"" children:@[]];

        ActionFactory* actionFactory = [[ActionFactory alloc]
            initWithScenario:kMenuScenarioHistogramNoteFolder];

        NSMutableArray<UIMenuElement*>* menuElements =
            [[NSMutableArray alloc] init];

        UIAction* editAction = [actionFactory actionToEditWithBlock:^{
          NoteHomeViewController* innerStrongSelf = weakSelf;
          if (!innerStrongSelf)
            return;
          const vivaldi::NoteNode* nodeFromId =
              note_utils_ios::FindNodeById(innerStrongSelf.notes, nodeId);
          if (nodeFromId) {
            [innerStrongSelf editNode:nodeFromId];
          }
        }];
        UIAction* moveAction = [actionFactory actionToMoveFolderWithBlock:^{
          NoteHomeViewController* innerStrongSelf = weakSelf;
          if (!innerStrongSelf)
            return;
          const vivaldi::NoteNode* nodeFromId =
              note_utils_ios::FindNodeById(innerStrongSelf.notes, nodeId);
          if (nodeFromId) {
            std::set<const NoteNode*> nodes{nodeFromId};
            [innerStrongSelf moveNodes:nodes];
          }
        }];

        if (!canEditNode) {
          editAction.attributes = UIMenuElementAttributesDisabled;
          moveAction.attributes = UIMenuElementAttributesDisabled;
        }

        [menuElements addObject:editAction];
        [menuElements addObject:moveAction];

        if (!node->is_permanent_node()) {
          UIAction* deleteAction = [actionFactory actionToDeleteWithBlock:^{
            NoteHomeViewController* innerStrongSelf = weakSelf;
            if (!innerStrongSelf)
                return;
            const vivaldi::NoteNode* nodeFromId =
                note_utils_ios::FindNodeById(innerStrongSelf.notes, nodeId);
            if (nodeFromId) {
              std::set<const NoteNode*> nodes{nodeFromId};
              [innerStrongSelf handleSelectNodesForDeletion:nodes];
            }
          }];
          [menuElements addObject:deleteAction];
          if (!canEditNode)
              deleteAction.attributes = UIMenuElementAttributesDisabled;
        }
        return [UIMenu menuWithTitle:@"" children:menuElements];
      };
    }
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark UIAdaptivePresentationControllerDelegate

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  // no op.
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // Cleanup once the dismissal is complete.
  [self dismissWithURL:GURL()];
}

#pragma mark - TableViewURLDragDataSource

- (URLInfo*)tableView:(UITableView*)tableView
    URLInfoAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section ==
      [self.tableViewModel
          sectionForSectionIdentifier:NoteHomeSectionIdentifierMessages]) {
    return nil;
  }

  const vivaldi::NoteNode* node = [self nodeAtIndexPath:indexPath];
  if (!node || node->is_folder()) {
    return nil;
  }
  return [[URLInfo alloc]
      initWithURL:node->GetURL()
            title:note_utils_ios::TitleForNoteNode(node)];
}

#pragma mark - TableViewURLDropDelegate

- (BOOL)canHandleURLDropInTableView:(UITableView*)tableView {
  return !self.sharedState.currentlyShowingSearchResults &&
         !self.tableView.hasActiveDrag && ![self isDisplayingNoteRoot];
}

- (void)tableView:(UITableView*)tableView
       didDropURL:(const GURL&)URL
      atIndexPath:(NSIndexPath*)indexPath {
  NSUInteger index = base::checked_cast<NSUInteger>(indexPath.item);
  [self.snackbarCommandsHandler showSnackbarMessage:
                    note_utils_ios::CreateNoteAtPositionWithToast(
                        base::SysUTF8ToNSString(URL.spec()), URL, _rootNode,
                        index, self.notes, self.profile)];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  __weak UIViewController* weakPresentingViewController =
      [self.settingsNavigationController presentingViewController];
  [self.settingsNavigationController cleanUpSettings];
  UIViewController* strongPresentingViewController =
      weakPresentingViewController;
  if (strongPresentingViewController) {
    [strongPresentingViewController
        dismissViewControllerAnimated:YES completion:nil];
  }
  self.settingsNavigationController = nil;
}

- (void)settingsWasDismissed {
  [self.settingsNavigationController cleanUpSettings];
  self.settingsNavigationController = nil;
}

- (UIBarButtonItem*)customizedDoneButton {
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithTitle:GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
                                 style:UIBarButtonItemStyleDone
                                target:self
                                action:@selector(navigationBarCancel:)];
  doneButton.accessibilityLabel =
      GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
  doneButton.accessibilityIdentifier =
      kNoteHomeNavigationBarDoneButtonIdentifier;

  return doneButton;
}

- (UIBarButtonItem*)customizedDoneTextButton {
    UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithTitle:GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(navigationBarCancel:)];
    doneButton.accessibilityLabel =
      GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
    doneButton.accessibilityIdentifier =
      kNoteHomeNavigationBarDoneButtonIdentifier;
    return doneButton;
}

// Set up navigation bar for |viewController|'s navigationBar using |node|.
- (void)setupNavigationForNoteHomeViewController:
            (NoteHomeViewController*)viewController
                                   usingNoteNode:
                                       (const vivaldi::NoteNode*)node {
  viewController.navigationItem.leftBarButtonItem.action = @selector(back);
  // Disable large titles on every VC but the root controller.
  if (node != self.notes->root_node()) {
    viewController.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
  }

  viewController.title = note_utils_ios::TitleForNoteNode(node);
  NSArray* items = nil;
  if ([self isDisplayingNoteRoot]) {
    viewController.navigationItem.rightBarButtonItem =
      [self customizedDoneTextButton];
  } else {
    viewController.title = note_utils_ios::TitleForNoteNode(node);
    items = @[[self customizedDoneButton]];
    viewController.navigationItem.rightBarButtonItems = items;
  }
}

- (void)setupHeaderWithSearch {
  UIView* tableHeaderView =
      [[UIView alloc] initWithFrame:
       CGRectMake(0, 0,self.tableView.bounds.size.width,
                  self.showingSidePanel ? panel_search_view_height :
                  panel_header_height)];
  VivaldiSearchBarView* searchBarView = [VivaldiSearchBarView new];
  _vivaldiSearchBarView = searchBarView;
  [tableHeaderView addSubview:searchBarView];
  [searchBarView
      fillSuperviewWithPadding:UIEdgeInsetsMake(self.showingSidePanel ? 0 :
                                                search_bar_height, 0, 0, 0)];
  searchBarView.delegate = self;
  [searchBarView setPlaceholder:GetNSString(IDS_VIVALDI_SEARCHBAR_PLACEHOLDER)];
  self.tableView.tableHeaderView = tableHeaderView;
}

#pragma mark: VIVALDI_SEARCH_BAR_VIEW_DELEGATE
- (void)searchBarTextDidChange:(NSString*)searchText {
  self.searchTerm = searchText;

  if (searchText.length == 0) {
    if (self.sharedState.currentlyShowingSearchResults) {
      self.sharedState.currentlyShowingSearchResults = NO;
      // Restore current list.
      [self.mediator computeNoteTableViewData];
      [self.sharedState.tableView reloadData];
    }
  } else {
    if (!self.sharedState.currentlyShowingSearchResults) {
      self.sharedState.currentlyShowingSearchResults = YES;
      [self.mediator computeNoteTableViewData];
    }
    // Replace current list with search result, but doesn't change
    // the 'regular' model for this page, which we can restore when search
    // is terminated.
    NSString* noResults = GetNSString(IDS_HISTORY_NO_SEARCH_RESULTS);
    [self.mediator computeNoteTableViewDataMatching:searchText
                         orShowMessageWhenNoResults:noResults];
    [self.sharedState.tableView reloadData];
    [self setupContextBar];
  }
}

/// Returns true if device is iPad and multitasking UI has
/// enough space to show iPad side panel.
- (BOOL)showingSidePanel {
  return [VivaldiGlobalHelpers
              canShowSidePanelForTrait:self.traitCollection];
}

- (void)showVivaldiSync {
  self.settingsNavigationController =
      [SettingsNavigationController
          vivaldiSyncViewControllerForBrowser:self.browser
              showCreateAccountFlow:NO
                  delegate:self];
  UISheetPresentationController *sheetPc =
      self.settingsNavigationController.sheetPresentationController;
  sheetPc.detents = @[UISheetPresentationControllerDetent.mediumDetent,
                      UISheetPresentationControllerDetent.largeDetent];
  sheetPc.prefersScrollingExpandsWhenScrolledToEdge = NO;
  sheetPc.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  [self presentViewController:self.settingsNavigationController
                     animated:YES
                   completion:nil];
}

@end
