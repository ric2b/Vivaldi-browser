// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_ITEM_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_ITEM_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "url/gurl.h"

namespace bookmarks {
class BookmarkNode;
}
@protocol ContentSuggestionsImageDataSource;

// VivaldiSpeedDialItem provides data for a the speed dial item. It is backed
// by BookmarkNode
@interface VivaldiSpeedDialItem: NSObject <NSItemProviderWriting>

// The BookmarkNode that backs this item.
@property(nonatomic, readwrite, assign)
    const bookmarks::BookmarkNode* bookmarkNode;
@property(nonatomic, strong) NSString* title;
@property(nonatomic, assign) GURL url;
@property(nonatomic, assign) BOOL isFolder;
@property(nonatomic, assign) BOOL isSpeedDial;
@property(nonatomic, assign) BOOL isFrequentlyVisited;
@property(nonatomic, assign) BOOL isThumbnailRefreshing;

// Data source for the most visited tiles favicon.
@property(nonatomic, weak) id<ContentSuggestionsImageDataSource>
    imageDataSource;

// INITIALIZER
- (instancetype)initWithBookmark:(const bookmarks::BookmarkNode*)node;
- (instancetype)initWithTitle:(NSString*)title url:(GURL)url;

// GETTERS
- (int64_t)id;
- (NSNumber*)idValue;
- (NSString*)nickname;
- (NSString*)urlString;
- (NSString*)host;
- (BOOL)isInternalPage;
- (NSString*)thumbnail;
- (NSString*)description;
- (NSDate*)createdAt;
- (const bookmarks::BookmarkNode*)parent;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_ITEM_H_
