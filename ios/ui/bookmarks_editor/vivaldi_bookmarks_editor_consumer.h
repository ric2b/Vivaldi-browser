// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_CONSUMER_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_CONSUMER_H_

#import "components/bookmarks/browser/bookmark_model.h"

@class VivaldiBookmarksEditorTopSitesItem;
@protocol VivaldiBookmarksEditorConsumer

@optional
/// Notifies the consumer that new folder is created.
- (void)didCreateNewFolder:(const bookmarks::BookmarkNode*)folder;
/// Notifies the consumer to close the bookmark editor
@optional
- (void)bookmarksEditorShouldClose;
/// Notifies the consumer to update top sites
@optional
- (void)bookmarksEditorTopSitesDidUpdate:
    (NSMutableArray<VivaldiBookmarksEditorTopSitesItem*>*)topSites;
/// Notifies the consumer to update single top site
@optional
- (void)bookmarksEditorTopSiteDidUpdate:
    (VivaldiBookmarksEditorTopSitesItem*)topSite;
/// Updates the state with the show speed dials preference value.
@optional
- (void)setPreferenceShowSpeedDials:(BOOL)showSpeedDials;
@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_CONSUMER_H_
