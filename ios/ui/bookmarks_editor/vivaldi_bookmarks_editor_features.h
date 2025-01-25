// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_FEATURES_H_
#define IOS_UI_BOOKMARKS_EDITOR_FEATURES_H_

#import <Foundation/Foundation.h>

@interface VivaldiBookmarksEditorFeatures: NSObject
// Whether top sites are visible in bookmarks editor.
+ (BOOL)shouldShowTopSites;
@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_FEATURES_H_
