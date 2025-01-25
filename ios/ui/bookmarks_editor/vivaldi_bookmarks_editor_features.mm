// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_editor_features.h"

#import "ios/chrome/browser/features/vivaldi_features.h"

@implementation VivaldiBookmarksEditorFeatures

+ (BOOL)shouldShowTopSites {
  return IsTopSitesEnabled();
}

@end
