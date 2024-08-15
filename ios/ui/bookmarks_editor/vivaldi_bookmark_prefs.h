// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_PREFS_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_PREFS_H_

#import <UIKit/UIKit.h>

#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_sorting_mode.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

@interface VivaldiBookmarkPrefs : NSObject

/// Static variable declaration
+ (PrefService*)prefService;

/// Static method to set the PrefService
+ (void)setPrefService:(PrefService *)pref;


+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

#pragma mark - Getters
/// Returns the bookmarks sorting mode from prefs.
+ (const VivaldiBookmarksSortingMode)getBookmarksSortingMode;
/// Returns the bookmarks sorting order from prefs.
+ (const VivaldiBookmarksSortingOrder)getBookmarksSortingOrder;
/// Returns to show all folders or only speed dial
/// folders on bookmark folder view controller.
+ (BOOL)getFolderViewMode;


#pragma mark - Setters
/// Sets the bookmarks sorting mode to the prefs.
+ (void)setBookmarksSortingMode:(const VivaldiBookmarksSortingMode)mode;
/// Sets the bookmarks sorting order to the prefs.
+ (void)setBookmarksSortingOrder:(const VivaldiBookmarksSortingOrder)order;
/// Sets the setting whether to show only speed dial
/// folders or all folders in bookmark folder view
/// controller.
+ (void)setFolderViewMode:(BOOL)showOnlySpeedDials;

@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_PREFS_H_
