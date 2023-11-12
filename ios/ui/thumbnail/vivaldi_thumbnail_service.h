// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_THUMBNAIL_VIVALDI_THUMBNAIL_SERVICE_H_
#define IOS_UI_THUMBNAIL_VIVALDI_THUMBNAIL_SERVICE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@class VivaldiSpeedDialItem;
namespace web {
class WebState;
}
namespace bookmarks {
class BookmarkModel;
}

// Service responsible for downloading and updating snapshot for speed-dial item
@interface VivaldiThumbnailService: NSObject

// Setters
- (void)generateAndStoreThumbnailForSDItem:(VivaldiSpeedDialItem*)sdItem
                                  webState:(web::WebState*)webState
                                 bookmarks:(bookmarks::BookmarkModel*)bookmarks;
- (void)removeThumbnailForSDItem:(VivaldiSpeedDialItem*)item;

// Getters
- (UIImage*)thumbnailForSDItem:(VivaldiSpeedDialItem*)item;

@end

#endif  // IOS_UI_THUMBNAIL_VIVALDI_THUMBNAIL_SERVICE_H_
