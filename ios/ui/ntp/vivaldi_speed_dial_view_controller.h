// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_VIEW_CONTROLLER_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_VIEW_CONTROLLER_H_

#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/ui/ntp/vivaldi_speed_dial_container_delegate.h"

using bookmarks::BookmarkModel;

class Browser;

// This class is presented when a folder is selected from the speed dial
// homepage or child page.
@interface VivaldiSpeedDialViewController
    : UIViewController

// INITIALIZERS
+ (instancetype)initWithItem:(VivaldiSpeedDialItem*)item
                      parent:(VivaldiSpeedDialItem*)parent
                   bookmarks:(BookmarkModel*)bookmarks
                     browser:(Browser*)browser
               faviconLoader:(FaviconLoader*)faviconLoader
             backgroundImage:(UIImage*)backgroundImage;

- (instancetype)initWithBookmarks:(BookmarkModel*)bookmarks
                          browser:(Browser*)browser;

// DELEGATE
@property (nonatomic, weak) id<VivaldiSpeedDialContainerDelegate> delegate;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_VIEW_CONTROLLER_H_
