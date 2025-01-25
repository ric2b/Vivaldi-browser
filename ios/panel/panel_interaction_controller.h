// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_PANEL_PANEL_INTERACTION_CONTROLLER_H_
#define IOS_PANEL_PANEL_INTERACTION_CONTROLLER_H_

#import <UIKit/UIKit.h>

class Browser;

enum PanelPage {
  BookmarksPage = 0,
  ReadinglistPage = 1,
  HistoryPage = 2,
  NotesPage = 3,
  TranslatePage = 4,
};

// Handles setting up and showing the panels
@interface PanelInteractionController : NSObject

- (instancetype)initWithBrowser:(Browser*)browser
NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Called before the instance is deallocated.
- (void)shutdown;

/// `searchString` argument is used to pass search string for history
/// and source text for translate panel. Other pages do not
/// use this value.
- (void)presentPanel:(PanelPage)page
    withSearchString:(NSString*)searchString;
- (void)dismissPanelModalControllerAnimated:(BOOL)animated;
- (void)panelDismissed;
// The parent controller on top of which the UI needs to be presented.
@property(nonatomic, weak) UIViewController* parentController;

@end

#endif  // IOS_PANEL_PANEL_INTERACTION_CONTROLLER_H_

