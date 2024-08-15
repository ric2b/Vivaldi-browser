// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_HOME_VIEW_CONTROLLER_H_
#define IOS_UI_NOTES_NOTE_HOME_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <set>
#import <vector>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/panel/panel_interaction_controller.h"

@protocol ApplicationCommands;
@class NoteHomeViewController;
class Browser;
namespace vivaldi {
class NoteNode;
}  // namespace vivaldi
class GURL;
@protocol SnackbarCommands;

@protocol NoteHomeViewControllerDelegate
// The view controller wants to be dismissed. If |urls| is not empty, then
// the user has selected to navigate to those URLs in the current tab mode.
- (void)noteHomeViewControllerWantsDismissal:
            (NoteHomeViewController*)controller
                                navigationToUrls:(const std::vector<GURL>&)urls;

// The view controller wants to be dismissed. If |urls| is not empty, then
// the user has selected to navigate to those URLs with specified tab mode.
- (void)noteHomeViewControllerWantsDismissal:
            (NoteHomeViewController*)controller
                                navigationToUrls:(const std::vector<GURL>&)urls
                                     inIncognito:(BOOL)inIncognito
                                          newTab:(BOOL)newTab;


@end

// Class to navigate the note hierarchy.
@interface NoteHomeViewController
    : LegacyChromeTableViewController <UIAdaptivePresentationControllerDelegate>

// Delegate for presenters. Note that this delegate is currently being set only
// in case of handset, and not tablet. In the future it will be used by both
// cases.
@property(nonatomic, weak) id<NoteHomeViewControllerDelegate> homeDelegate;

// Handler for Application Commands.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandsHandler;

// Handler for Snackbar Commands.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// Initializers.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)tableViewStyle NS_UNAVAILABLE;


// Called before the instance is deallocated.
- (void)shutdown;

// Setter to set _rootNode value.
- (void)setRootNode:(const vivaldi::NoteNode*)rootNode;

// Returns an array of NoteHomeViewControllers, one per NoteNode in the
// path from this view controller's node to the latest cached node (as
// determined by NotePathCache).  Includes |self| as the first element of
// the returned array.  Sets the cached scroll position for the last element of
// the returned array, if appropriate.
- (NSArray<NoteHomeViewController*>*)cachedViewControllerStack;

- (void)handleAddBarButtonTap;

@end

#endif  // IOS_UI_NOTES_NOTE_HOME_VIEW_CONTROLLER_H_
