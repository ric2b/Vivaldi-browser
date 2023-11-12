// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/panel/panel_interaction_controller.h"

#include <stdint.h>

#import "app/vivaldi_apptools.h"
#import "base/ios/ios_util.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_interaction_controller.h"
#import "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/history/history_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller_delegate.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/notes/note_interaction_controller.h"
#import "ios/notes/note_home_view_controller.h"
#import "ios/panel/panel_constants.h"
#import "ios/panel/panel_transitioning_delegate.h"
#import "ios/panel/panel_view_controller.h"
#import "ios/panel/sidebar_panel_view_controller.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiviewcontroller_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"


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

@interface PanelInteractionController () {
  // The browser panels are presented in.
  Browser* _browser;  // weak

  // The browser state to use, might be different from _currentBrowserState if
  // it is incognito.
  ChromeBrowserState* _browserState;  // weak

  // The parent controller on top of which the UI needs to be presented.
  __weak UIViewController* _parentController;

  NoteInteractionController* _noteInteractionController;
  BookmarkInteractionController* _bookmarkInteractionController;
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
- (void)panelBrowserDismissed;
@end

@implementation PanelInteractionController
@synthesize panelController = _panelController;
@synthesize currentPresentedState = _currentPresentedState;

- (instancetype)initWithBrowser:(Browser*)browser{
  self = [super init];
  if (self) {
    _browser = browser;
    _currentPresentedState = PresentedState::NONE;
    DCHECK(_parentController);
  }
  return self;
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
  [self panelBrowserDismissed];
    _noteInteractionController = nil;
    // shutdown
    _historyCoordinator = nil;
    _bookmarkInteractionController = nil;
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

- (void)panelBrowserDismissed {

}
- (void)presentPanel:(PanelPage)page
    withSearchString:(NSString*)searchString {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  DCHECK(!self.panelController);

  UIViewController* vc;
  if (_parentController.isDeviceIPad) {
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
  [self initializeBookmarkInteractionController:vc];
  [_bookmarkInteractionController presentBookmarks];
  [self showHistory:vc withSearchString:searchString];
  _noteInteractionController.panelDelegate = self;
  _bookmarkInteractionController.panelDelegate = self;
  self.historyCoordinator.panelDelegate = self;
  int index = 0;
  switch (page) {
    case PanelPage::BookmarksPage: index = 0; break;
    case PanelPage::NotesPage: index = 1; break;
    case PanelPage::HistoryPage: index = 2; break;
  }

  if (_parentController.isDeviceIPad) {
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
       _bookmarkInteractionController.bookmarkNavigationController
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
       (NoteNavigationController*)
     _noteInteractionController.noteNavigationController
      withBookmarkController:
       _bookmarkInteractionController.bookmarkNavigationController
      andHistoryController:self.historyCoordinator.historyNavigationController];
    self.panelController.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
    [self.panelController.segmentControl setSelectedSegmentIndex:index];
    [self.panelController setIndexForControl:index];
    if (@available(iOS 15.0, *)) {
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
    } else {
        [_parentController presentViewController:_panelController
                                    animated:YES
                                completion:nil];
    }
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
- (void)initializeBookmarkInteractionController:(UIViewController*)vc {
  if (_bookmarkInteractionController)
    return;
  _bookmarkInteractionController =
    [[BookmarkInteractionController alloc] initWithBrowser:_browser];
}

- (void)dismissPanelModalControllerAnimated:(BOOL)animated {
  [self dismissPanelBrowserAnimated:animated
                           inIncognito:NO
                                newTab:NO];
}

- (void)panelDismissed {
    if (self.panelController) {
        [self.panelController panelDismissed];
        self.panelController = nil;
    }
    if (self.sidebarPanelController) {
        [self.sidebarPanelController panelDismissed];
        if (_parentController.isDeviceIPad) {
            [self.sidebarPanelController.view removeFromSuperview];
            [self.sidebarPanelController removeFromParentViewController];
            self.sidebarPanelController = nil;
        }
    }
}
@end
