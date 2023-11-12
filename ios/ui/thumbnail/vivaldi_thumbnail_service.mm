// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/thumbnail/vivaldi_thumbnail_service.h"

#import <ImageIO/ImageIO.h>

#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/ui/ntp/vivaldi_speed_dial_item.h"
#import "ios/web/public/web_state.h"

using vivaldi_bookmark_kit::SetNodeThumbnail;
using bookmarks::BookmarkModel;

// Namespace
namespace {
// Maximum resolution for the image is 400px.
const CGFloat thumbnailMaxResolution = 400.f;
}

@implementation VivaldiThumbnailService

// Setters
- (void)generateAndStoreThumbnailForSDItem:(VivaldiSpeedDialItem*)sdItem
                                  webState:(web::WebState*)webState
                                 bookmarks:(BookmarkModel*)bookmarks {
  SnapshotTabHelper::FromWebState(webState) ->UpdateSnapshotWithCallback(
      ^(UIImage* snapshot) {
        NSString* snapshotPath = [self storeThumbnailFromSnapshot:snapshot
                                                           SDItem:sdItem];
        if (snapshotPath) {
          const std::string imagePathU16 =
              base::SysNSStringToUTF8(snapshotPath);
          [[NSOperationQueue mainQueue] addOperationWithBlock:^{
            SetNodeThumbnail(bookmarks, sdItem.bookmarkNode, imagePathU16);
          }];
        }
  });
}

- (void)removeThumbnailForSDItem:(VivaldiSpeedDialItem*)item {
  if (!item || !item.bookmarkNode) {
    return;
  }

  NSString *imagePathWithName = [self filePathFromItem:item];

  NSFileManager *fileManager = [NSFileManager defaultManager];
  BOOL isFileExist = [fileManager fileExistsAtPath: imagePathWithName];

  if (isFileExist) {
    [[NSFileManager defaultManager] removeItemAtPath:imagePathWithName
                                               error:nil];
  }
}

// Getters
- (UIImage*)thumbnailForSDItem:(VivaldiSpeedDialItem*)item {
  if (!item || !item.bookmarkNode) {
    return nil;
  }

  NSString *imagePathWithName = [self filePathFromItem:item];
  NSFileManager *fileManager = [NSFileManager defaultManager];
  BOOL isFileExist = [fileManager fileExistsAtPath: imagePathWithName];

  if (isFileExist) {
    return [[UIImage alloc] initWithContentsOfFile: imagePathWithName];
  } else {
    return nil;
  }
}

#pragma mark - Private
- (NSString*)folderDirectory {
  return @"/sd_thumbnails";
}

- (NSString*)storeThumbnailFromSnapshot:(UIImage*)capturedSnapshot
                                 SDItem:(VivaldiSpeedDialItem*)item {
  // Return early if no snapshot or item provided.
  if (!capturedSnapshot || !item || !item.bookmarkNode)
    return nil;

  NSError *error;
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask,
                                                       YES);
  NSString *documentsDirectory = [paths firstObject];
  if (!documentsDirectory)
    return nil;

  NSString *dataPath =
    [documentsDirectory
     stringByAppendingPathComponent: [self folderDirectory]];

  // Create a folder inside Document Directory only if there is none already.
  if (![[NSFileManager defaultManager] fileExistsAtPath:dataPath])
    [[NSFileManager defaultManager] createDirectoryAtPath:dataPath
                              withIntermediateDirectories:NO
                                               attributes:nil
                                                    error:&error];

  // Use the unique name for the file.
  NSUUID *newUniqueId = [NSUUID UUID];
  NSString *newUniqueIdString = [newUniqueId UUIDString];

  NSString *imagePathWithName =
    [NSString stringWithFormat:@"%@/sd_%@.png", dataPath, newUniqueIdString];

  // Delete the file with same name if exists.
  if ([[NSFileManager defaultManager] fileExistsAtPath:imagePathWithName]) {
    [[NSFileManager defaultManager] removeItemAtPath:imagePathWithName
                                               error:nil];
  }

  UIImage* thumbnail = [self generateThumbnailFromSnapshot:capturedSnapshot];
  NSData *thumbnailData =
    [NSData dataWithData:UIImagePNGRepresentation(thumbnail)];

  // Store the file
  [thumbnailData writeToFile: imagePathWithName atomically: YES];

  NSString* finalPath = [NSString
                         stringWithFormat:@"%@/sd_%@.png",
                         [self folderDirectory], newUniqueIdString];
  return finalPath;
}

- (NSString*)filePathFromItem:(VivaldiSpeedDialItem*)item {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask,
                                                       YES);
  NSString *documentsDirectory = [paths firstObject];
  if (!documentsDirectory)
    return nil;

  NSString *thumbnailPathString = item.thumbnail;
  NSString *finalPath =
    [documentsDirectory
     stringByAppendingPathComponent: thumbnailPathString];
  return finalPath;
}

- (UIImage*)generateThumbnailFromSnapshot:(UIImage*)snapshot {

  NSData *imageData =
    [NSData dataWithData:UIImagePNGRepresentation(snapshot)];
  CGImageSourceRef imageSource =
    CGImageSourceCreateWithData((CFDataRef)imageData, NULL);

  NSDictionary *imageOptions = @{
    (NSString const *)kCGImageSourceCreateThumbnailFromImageIfAbsent:
        (NSNumber const *)kCFBooleanTrue,
    (NSString const *)kCGImageSourceThumbnailMaxPixelSize:
        @(thumbnailMaxResolution),
    (NSString const *)kCGImageSourceCreateThumbnailWithTransform:
        (NSNumber const *)kCFBooleanTrue
  };

  CGImageRef thumbnail =
    CGImageSourceCreateThumbnailAtIndex(imageSource, 0,
                                        (__bridge CFDictionaryRef)imageOptions);
  CFRelease(imageSource);

  UIImage *thumbnailImage = [[UIImage alloc] initWithCGImage:thumbnail];
  CGImageRelease(thumbnail);

  return thumbnailImage;
}

@end
