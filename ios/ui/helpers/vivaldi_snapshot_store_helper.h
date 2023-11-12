// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_HELPERS_VIVALDI_SNAPSHOT_STORE_HELPER_H_
#define IOS_UI_HELPERS_VIVALDI_SNAPSHOT_STORE_HELPER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_item.h"

@interface NSFileManager(Vivaldi)


- (NSString*)storeThumbnailFromSnapshot:(UIImage*)capturedSnapshot
                                 SDItem:(VivaldiSpeedDialItem*)SDItem;
- (UIImage*)thumbnailForSDItem:(VivaldiSpeedDialItem*)item;
- (void)removeThumbnailForSDItem:(VivaldiSpeedDialItem*)item;

@end

#endif  // IOS_UI_HELPERS_VIVALDI_SNAPSHOT_STORE_HELPER_H_
