// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/panel/panel_interaction_controller.h"

#include <stdint.h>

#import "app/vivaldi_apptools.h"
#import "base/ios/ios_util.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmarks_coordinator.h"
#import "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/history/history_coordinator_delegate.h"
#import "ios/chrome/browser/ui/history/history_table_view_controller.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator_delegate.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/panel/panel_constants.h"
#import "ios/panel/panel_transitioning_delegate.h"
#import "ios/panel/panel_view_controller.h"
#import "ios/panel/sidebar_panel_view_controller.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/notes/note_home_view_controller.h"
#import "ios/ui/notes/note_interaction_controller.h"
#import "ios/ui/notes/note_navigation_controller.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"


using l10n_util::GetNSString;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif
/**
  PanelInteractionController
 */
namespace {

// Tracks the type of UI that is currently being presented.
enum class PresentedState {
  NONE,
  PANEL_BROWSER,
};

}  // namespace

@interface PanelInteractionController ()<HistoryCoordinatorDelegate,
                                        ReadingListCoordinatorDelegate> {
  // The browser panels are presented in.
  Browser* _browser;  // weak

  // The browser state to use, might be different from _currentBrowserState if
  // it is incognito.
  ChromeBrowserState* _browserState;  // weak

  // The parent controller on top of which the UI needs to be presented.
  __weak UIViewController* _parentController;

  NoteInteractionController* _noteInteractionController;
  BookmarksCoordinator* _bookmarksCoordinator;
  ReadingListCoordinator* _readinglistCoordinator;
}

@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;
@property(nonatomic, strong) PanelTransitioningDelegate* tc;

// The type of view controller that is being presented.
@property(nonatomic, assign) PresentedState currentPresentedState;

// Panel controller for phone
@property(nonatomic, strong) PanelViewController* panelController;

// Panel controller for iPad
@property(nonatomic, strong) SidebarPanelViewController* sidebarPanelController;

@property(nonatomic, readonly, weak) id<ApplicationCommands, BrowserCommands>
    handler;

// Dismisses the panel browser.  If |urlsToOpen| is not empty, then the user
// has selected to navigate to those URLs with specified tab mode.
- (void)dismissPanelBrowserAnimated:(BOOL)animated
                           inIncognito:(BOOL)inIncognito
                                newTab:(BOOL)newTab;
@end

@implementation PanelInteractionController
@synthesize panelController = _panelController;
@synthesize currentPresentedState = _currentPresentedState;

- (instancetype)initWithBrowser:(Browser*)browser{
  self = [super init];
  if (self) {
    _browser = browser;
    _currentPresentedState = PresentedState::NONE;
  }
  return self;
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
    _noteInteractionController = nil;
    // shutdown
    _historyCoordinator = nil;
    _bookmarksCoordinator = nil;
    _readinglistCoordinator = nil;
}

- (void)dismissPanelBrowserAnimated:(BOOL)animated
                           inIncognito:(BOOL)inIncognito
                                newTab:(BOOL)newTab {
  if (_parentController.presentedViewController) {
    [_parentController dismissViewControllerAnimated:animated
                                          completion:nil];
  }
  self.currentPresentedState = PresentedState::NONE;
}

- (void)presentPanel:(PanelPage)page
    withSearchString:(NSString*)searchString {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  DCHECK(!self.panelController);

  UIViewController* vc;
  if (self.showSidePanel) {
      SidebarPanelViewController* sidebar =
        [[SidebarPanelViewController alloc] init];
      self.tc = [[PanelTransitioningDelegate alloc] init];
      sidebar.transitioningDelegate = self.tc;
      self.sidebarPanelController = sidebar;
      [self.sidebarPanelController
       setModalPresentationStyle:UIModalPresentationCustom];
      self.sidebarPanelController.modalInPresentation = NO;
      vc = self.sidebarPanelController;
  } else {
      PanelViewController* panelController = [[PanelViewController alloc] init];
      self.panelController = panelController;
      [panelController setModalPresentationStyle:UIModalPresentationPageSheet];
      vc = self.panelController;
  }

  [self initializeNoteInteractionController:vc];
  [_noteInteractionController showNotes];
  [self initializeBookmarksCoordinator:vc];
  [_bookmarksCoordinator presentBookmarks];
  [self showHistory:vc withSearchString:searchString];
  [self showReadinglist:vc];
  _noteInteractionController.panelDelegate = self;
  _bookmarksCoordinator.panelDelegate = self;
  _readinglistCoordinator.panelDelegate = self;
  _historyCoordinator.panelDelegate = self;
  int index = 0;
  switch (page) {
    case PanelPage::BookmarksPage: index = 0; break;
    case PanelPage::ReadinglistPage: index = 1; break;
    case PanelPage::HistoryPage: index = 2; break;
    case PanelPage::NotesPage: index = 3; break;
  }

  if (self.showSidePanel) {
      [self setupAndPresentiPadPanel:index];
  } else {
      [self setupAndPresentPhonePanel:index];
  }
  self.currentPresentedState = PresentedState::PANEL_BROWSER;
}

- (void)setupAndPresentiPadPanel:(NSInteger)index {
    [_sidebarPanelController setupControllers:
     (NoteNavigationController*)
       _noteInteractionController.noteNavigationController
      withBookmarkController:
       _bookmarksCoordinator.bookmarkNavigationController
      andReadinglistController:
       _readinglistCoordinator.navigationController
      andHistoryController:self.historyCoordinator.historyNavigationController];
    self.sidebarPanelController.view.backgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
    [self.sidebarPanelController.segmentControl setSelectedSegmentIndex:index];
    [self.sidebarPanelController setIndexForControl:index];
    [_parentController presentViewController:self.sidebarPanelController
                              animated:YES
                            completion:nil];
}

- (void)setupAndPresentPhonePanel:(NSInteger)index {
  [_panelController setupControllers:
      (NoteNavigationController*)_noteInteractionController
            .noteNavigationController
              withBookmarkController:
      _bookmarksCoordinator.bookmarkNavigationController
            andReadinglistController:
      _readinglistCoordinator.navigationController
                andHistoryController:
      _historyCoordinator.historyNavigationController];

  self.panelController.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  [self.panelController.segmentControl setSelectedSegmentIndex:index];
  [self.panelController setIndexForControl:index];

  UISheetPresentationController* sheetPc =
       _panelController.sheetPresentationController;
  sheetPc.detents = @[UISheetPresentationControllerDetent.mediumDetent,
                       UISheetPresentationControllerDetent.largeDetent];
  sheetPc.preferredCornerRadius = panel_sheet_corner_radius;
  sheetPc.prefersScrollingExpandsWhenScrolledToEdge = NO;
  sheetPc.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  [_parentController presentViewController:_panelController
                                  animated:YES
                                completion:nil];
}

#pragma mark - Private

- (void)showHistory:(UIViewController*)vc
   withSearchString:(NSString*)searchString {
  if (!self.historyCoordinator) {
      self.historyCoordinator = [[HistoryCoordinator alloc]
        initWithBaseViewController:vc
                           browser:_browser];
  }
  self.historyCoordinator.searchTerms = searchString;
  self.historyCoordinator.loadStrategy = UrlLoadStrategy::NORMAL;
  self.historyCoordinator.delegate = self;
  [self.historyCoordinator start];
}

// Initializes the note interaction controller if not already initialized.
- (void)initializeNoteInteractionController:(UIViewController*)vc {
  if (_noteInteractionController)
    return;
  _noteInteractionController =
      [[NoteInteractionController alloc] initWithBrowser:_browser
                                parentController:vc];
}

// Initializes the bookmark interaction controller if not already initialized.
- (void)initializeBookmarksCoordinator:(UIViewController*)vc {
  if (_bookmarksCoordinator)
    return;
  _bookmarksCoordinator =
    [[BookmarksCoordinator alloc] initWithBrowser:_browser];
}

// Initializes the reading list interaction controller
// if not already initialized.
- (void)showReadinglist:(UIViewController*)vc {
  if (_readinglistCoordinator)
    return;
  _readinglistCoordinator =
  [[ReadingListCoordinator alloc] initWithBaseViewController:vc
                                                     browser:_browser];
  _readinglistCoordinator.delegate = self;
  [_readinglistCoordinator start];
}

- (void)dismissPanelModalControllerAnimated:(BOOL)animated {
  [self dismissPanelBrowserAnimated:animated
                           inIncognito:NO
                                newTab:NO];
}

- (void)panelDismissed {
  if (self.panelController) {
    [self.panelController panelDismissed];
    [self dismissPanelModalControllerAnimated:YES];
    self.panelController = nil;
  }
  if (self.sidebarPanelController) {
    [self.sidebarPanelController panelDismissed];
    if (self.showSidePanel) {
      [self.sidebarPanelController.view removeFromSuperview];
      [self.sidebarPanelController removeFromParentViewController];
      self.sidebarPanelController = nil;
    }
  }
}

/// Returns true if device is iPad and multitasking UI has
/// enough space to show iPad side panel.
- (BOOL)showSidePanel {
  return VivaldiGlobalHelpers.canShowSidePanel;
}

#pragma mark - HistoryCoordinatorDelegate

- (void)closeHistoryWithCompletion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  [self.historyCoordinator dismissWithCompletion:^{
    if (completion) {
      completion();
    }
    [weakSelf.historyCoordinator stop];
    weakSelf.historyCoordinator = nil;
  }];
}

#pragma mark - ReadingListCoordinatorDelegate

- (void)closeReadingList {
  [_readinglistCoordinator stop];
  _readinglistCoordinator.delegate = nil;
  _readinglistCoordinator = nil;
}

@end
