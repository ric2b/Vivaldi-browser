// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_ITEM_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_ITEM_H_

#import <Foundation/Foundation.h>

#import "url/gurl.h"

namespace bookmarks {
class BookmarkNode;
}

// VivaldiSpeedDialItem provides data for a the speed dial item. It is backed
// by BookmarkNode
@interface VivaldiSpeedDialItem: NSObject <NSItemProviderWriting>

// The BookmarkNode that backs this item.
@property(nonatomic, readwrite, assign)
    const bookmarks::BookmarkNode* bookmarkNode;

// INITIALIZER
- (instancetype) initWithBookmark:(const bookmarks::BookmarkNode*)node;

// GETTERS
- (int64_t)id;
- (NSNumber*)idValue;
- (BOOL)isFolder;
- (BOOL)isSpeedDial;
- (NSString*)title;
- (NSString*)nickname;
- (GURL)url;
- (NSString*)urlString;
- (NSString*)host;
- (BOOL)isInternalPage;
- (NSString*)thumbnail;
- (NSString*)description;
- (NSDate*)createdAt;
- (const bookmarks::BookmarkNode*)parent;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_ITEM_H_
