// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_MEDIATOR_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/ui/ntp/nsd/vivaldi_nsd_view_delegate.h"
#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

class ProfileIOS;
@protocol VivaldiNSDConsumer;

// Mediator for the new speed dial dialog
@interface VivaldiNSDMediator: NSObject<VivaldiNSDViewDelegate>

// Consumer to reflect user’s change in the model.
@property(nonatomic, weak) id<VivaldiNSDConsumer> consumer;

- (instancetype)initWithBookmarkModel:(BookmarkModel*)bookmarkModel
                              profile:(ProfileIOS*)profile
                   locationFolderItem:(VivaldiSpeedDialItem*)folderItem
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// Public
- (void)saveItemWithTitle:(NSString*)title
                      url:(NSString*)urlString;

// Changes `self.locationFolderItem`, updates the UI accordingly.
- (void)manuallyChangeFolder:(VivaldiSpeedDialItem*)folder;

@end

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_EDITOR_MEDIATOR_H_
