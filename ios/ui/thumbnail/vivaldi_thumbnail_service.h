// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_THUMBNAIL_VIVALDI_THUMBNAIL_SERVICE_H_
#define IOS_UI_THUMBNAIL_VIVALDI_THUMBNAIL_SERVICE_H_

#import <UIKit/UIKit.h>

#import "components/bookmarks/browser/bookmark_model.h"

using bookmarks::BookmarkModel;

@class VivaldiSpeedDialItem;

// Class responsible for storing, updating and returning speed dial thumbnails.
@interface VivaldiThumbnailService: NSObject

#pragma mark - Setters
/// Stores the thumbnail to new directory. When Replace is true, it overrides
/// any previous thumbnail of the item.
- (void)storeThumbnailForSDItem:(VivaldiSpeedDialItem*)sdItem
                       snapshot:(UIImage*)snapshot
                        replace:(BOOL)replace
                    isMigrating:(BOOL)isMigrating
                      bookmarks:(BookmarkModel*)bookmarks;
/// Removes the thumbnail from either legacy or new directory if any.
- (void)removeThumbnailForSDItem:(VivaldiSpeedDialItem*)item;
/// Removes the thumbnail from legacy directory. Should be triggered
/// after a migration.
- (void)removeLegacyThumbnailForSDItem:(VivaldiSpeedDialItem*)item;

#pragma mark - Getters
/// Returns true if user has legacy thumbnail.
/// A migration is triggered if this is True which removes the old one,
/// and create a new one to new directory.
- (BOOL)shouldMigrateForSDItem:(VivaldiSpeedDialItem*)item;
/// Returns the thumbnail for SD item from legacy or new directory.
- (UIImage*)thumbnailForSDItem:(VivaldiSpeedDialItem*)item;

@end

#endif  // IOS_UI_THUMBNAIL_VIVALDI_THUMBNAIL_SERVICE_H_
