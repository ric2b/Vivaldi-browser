// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/panel/panel_interaction_controller.h"
#include <stdint.h>

#include "app/vivaldi_apptools.h"
#import "base/ios/ios_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_interaction_controller.h"
#include "ios/chrome/browser/ui/history/history_coordinator.h"
#include "ios/chrome/browser/ui/history/history_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller_delegate.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/notes/note_interaction_controller.h"
#import "ios/notes/note_home_view_controller.h"
#import "ios/panel/panel_view_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
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

// The type of view controller that is being presented.
@property(nonatomic, assign) PresentedState currentPresentedState;

@property(nonatomic, strong) PanelViewController* panelController;
// The delegate provided to |self.panelTabBarController|.

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
- (void)presentPanel:(PanelPage)page {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  DCHECK(!self.panelController);

  PanelViewController* panelController = [[PanelViewController alloc] init];
  self.panelController = panelController;
  [panelController setModalPresentationStyle:UIModalPresentationPageSheet];
  [self initializeNoteInteractionController];
  [_noteInteractionController showNotes];
  [self initializeBookmarkInteractionController];
  [_bookmarkInteractionController presentBookmarks];
  [self showHistory];
  _noteInteractionController.panelDelegate = self;
  _bookmarkInteractionController.panelDelegate = self;
  self.historyCoordinator.panelDelegate = self;
  [_panelController setupControllers:
     (NoteNavigationController*)_noteInteractionController.noteNavigationController
    withBookmarkController:
     _bookmarkInteractionController.bookmarkNavigationController
    andHistoryController:self.historyCoordinator.historyNavigationController];
  self.panelController.view.backgroundColor =
    [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  int index = 0;
  switch (page) {
      case PanelPage::BookmarksPage: index = 0; break;
      case PanelPage::NotesPage: index = 1; break;
      case PanelPage::HistoryPage: index = 2; break;
  }
  [self.panelController.segmentControl setSelectedSegmentIndex:index];
  [self.panelController setIndexForControl:index];
  if (base::ios::IsRunningOnIOS15OrLater()) {
      if (@available(iOS 15.0, *)) {
          UISheetPresentationController* sheetPc =
            _panelController.sheetPresentationController;
          sheetPc.detents = @[UISheetPresentationControllerDetent.mediumDetent,
                              UISheetPresentationControllerDetent.largeDetent];
          sheetPc.preferredCornerRadius = 20.0;
          sheetPc.prefersScrollingExpandsWhenScrolledToEdge = NO;
          sheetPc.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
          [_parentController presentViewController:_panelController
                                        animated:YES
                                      completion:nil];
      }
  } else {
      [_parentController presentViewController:_panelController
                                    animated:YES
                                completion:nil];
  }

  self.currentPresentedState = PresentedState::PANEL_BROWSER;
}

#pragma mark - Private

- (void)showHistory {
  if (!self.historyCoordinator) {
      self.historyCoordinator = [[HistoryCoordinator alloc]
        initWithBaseViewController:self.panelController
                           browser:_browser];
  }
  self.historyCoordinator.loadStrategy = UrlLoadStrategy::NORMAL;
  [self.historyCoordinator start];
}

// Initializes the note interaction controller if not already initialized.
- (void)initializeNoteInteractionController {
  if (_noteInteractionController)
    return;
  _noteInteractionController =
      [[NoteInteractionController alloc] initWithBrowser:_browser
                                parentController:self.panelController];
}

// Initializes the bookmark interaction controller if not already initialized.
- (void)initializeBookmarkInteractionController {
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
    [self.panelController panelDismissed];
}

@end
