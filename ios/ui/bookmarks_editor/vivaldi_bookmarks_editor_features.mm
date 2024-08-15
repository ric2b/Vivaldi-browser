// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_features.h"

#import "ios/chrome/browser/shared/public/features/features.h"

@implementation VivaldiBookmarksEditorFeatures

+ (BOOL)shouldShowNewBookmarkEditor {
  return IsNewStartPageIsEnabled();
}

@end
