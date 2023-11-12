// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_PREFS_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_PREFS_H_

#import <UIKit/UIKit.h>

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

@interface VivaldiBookmarkPrefs : NSObject

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

/// Returns to show all folders or only speed dial folders on bookmark
/// folder view controller.
+ (BOOL)getFolderViewModeFromPrefService:(PrefService*)prefService;

/// Sets the setting whether to show only speed dial folders or all folders
/// in bookmark folder view controller.
+ (void)setFolderViewMode:(BOOL)showOnlySpeedDials
          inPrefServices:(PrefService*)prefService;

@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARK_PREFS_H_
