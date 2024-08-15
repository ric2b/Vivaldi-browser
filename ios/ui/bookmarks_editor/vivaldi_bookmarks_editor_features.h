// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_FEATURES_H_
#define IOS_UI_BOOKMARKS_EDITOR_FEATURES_H_

#import <Foundation/Foundation.h>

@interface VivaldiBookmarksEditorFeatures: NSObject
// Whether new bookmark editor should be shown.
// Currently the difference between new and old editor is fundamentally
// a couple of strings.
+ (BOOL)shouldShowNewBookmarkEditor;
@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_FEATURES_H_
