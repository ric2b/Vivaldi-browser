// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/thumbnail/vivaldi_thumbnail_service.h"

#import <ImageIO/ImageIO.h>

#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

using vivaldi_bookmark_kit::SetNodeThumbnail;

// Namespace
namespace {
// Maximum resolution for the image is 1440px. Anything less than that could
// show blurry thumbnails.
const CGFloat thumbnailMaxResolution = 1440.f;

NSString* thumbnailDirectory = @"Vivaldi/sd-thumbnails";
}

@implementation VivaldiThumbnailService

#pragma mark - Setters

- (void)storeThumbnailForSDItem:(VivaldiSpeedDialItem*)sdItem
                       snapshot:(UIImage*)snapshot
                        replace:(BOOL)replace
                    isMigrating:(BOOL)isMigrating
                      bookmarks:(BookmarkModel*)bookmarks {
  NSString* snapshotPath = [self storeThumbnailFromSnapshot:snapshot
                                                     SDItem:sdItem
                                                    replace:replace];
  if (snapshotPath) {
    if (isMigrating) {
      [self removeLegacyThumbnailForSDItem:sdItem];
    }

    const std::string imagePathU16 =
        base::SysNSStringToUTF8(snapshotPath);
    [[NSOperationQueue mainQueue] addOperationWithBlock:^{
      SetNodeThumbnail(bookmarks,
                       sdItem.bookmarkNode, imagePathU16);
    }];
  }
}

- (void)removeThumbnailForSDItem:(VivaldiSpeedDialItem*)item {
  if (!item || !item.bookmarkNode) {
    return;
  }
  NSString *imagePathWithName = [self thumbnailPathForItem:item];
  [self removeFileFromPath:imagePathWithName];
}

- (void)removeLegacyThumbnailForSDItem:(VivaldiSpeedDialItem*)item {
  if (!item || !item.bookmarkNode) {
    return;
  }
  NSString *imagePathWithName = [self legacyFilePathFromItem:item];
  [self removeFileFromPath:imagePathWithName];
}

#pragma mark - Getters
- (UIImage*)thumbnailForSDItem:(VivaldiSpeedDialItem*)item {
  if (!item || !item.bookmarkNode) {
    return nil;
  }

  NSString *imagePathWithName = [self thumbnailPathForItem:item];
  NSFileManager *fileManager = [NSFileManager defaultManager];
  BOOL isFileExist = [fileManager fileExistsAtPath: imagePathWithName];

  if (isFileExist) {
    return [[UIImage alloc] initWithContentsOfFile: imagePathWithName];
  } else {
    return nil;
  }
}

- (BOOL)shouldMigrateForSDItem:(VivaldiSpeedDialItem*)item {
  return ([self legacyFilePathFromItem:item] &&
          ![self filePathFromItem:item]);
}

#pragma mark - Private
- (NSString*)storeThumbnailFromSnapshot:(UIImage*)capturedSnapshot
                                 SDItem:(VivaldiSpeedDialItem*)item
                                replace:(BOOL)replace {
  // Return early if no snapshot or item provided.
  if (!capturedSnapshot || !item || !item.bookmarkNode)
    return nil;

  NSError *error;

  // Get the Application Support directory.
  NSFileManager *fileManager = [NSFileManager defaultManager];
  NSURL *applicationSupportDir =
      [[fileManager URLsForDirectory:NSApplicationSupportDirectory
                           inDomains:NSUserDomainMask] firstObject];

  // Append thumbnails folder to Application Support
  NSURL *dataPathURL =
      [applicationSupportDir URLByAppendingPathComponent:thumbnailDirectory];
  NSString *dataPath = [dataPathURL path];

  // Create a folder inside Application Support Directory if it does not exist.
  if (![fileManager fileExistsAtPath:dataPath])
    [fileManager createDirectoryAtPath:dataPath
           withIntermediateDirectories:YES
                            attributes:nil
                                 error:&error];

  if (error) {
    return nil;
  }

  if (replace) {
    // Delete if same item already has a thumbnail for the item
    NSString* oldItemPath = [self filePathFromItem:item];
    if (oldItemPath) {
      // Delete the file with the same name if it exists.
      if ([fileManager fileExistsAtPath:oldItemPath]) {
        [fileManager removeItemAtPath:oldItemPath error:nil];
      }
    }
  }

  // Use a unique name for the file.
  NSUUID *newUniqueId = [NSUUID UUID];
  NSString *newUniqueIdString = [newUniqueId UUIDString];
  NSString *imagePathWithName =
      [dataPath stringByAppendingPathComponent:
          [NSString stringWithFormat:@"sd_%@.png", newUniqueIdString]];

  // Delete the file with the same name if it exists.
  if ([fileManager fileExistsAtPath:imagePathWithName]) {
    [fileManager removeItemAtPath:imagePathWithName error:nil];
  }

  UIImage* thumbnail = [self generateThumbnailFromSnapshot:capturedSnapshot];
  NSData *thumbnailData = UIImagePNGRepresentation(thumbnail);

  // Store the file
  [thumbnailData writeToFile:imagePathWithName atomically:YES];

  // Exclude the thumbnail from iCloud backup
  NSURL *fileURL = [NSURL fileURLWithPath:imagePathWithName];
  [fileURL setResourceValue:@YES
                     forKey:NSURLIsExcludedFromBackupKey
                      error:&error];

  // Return the relative path of the saved file
  NSString *relativePath =
      [NSString stringWithFormat:@"%@/sd_%@.png",
          thumbnailDirectory, newUniqueIdString];
  return relativePath;
}


#pragma mark - Private

/// Returns the file path for provided speed dial item if any.
/// This checks the new directory as well as the legacy directory.
- (NSString*)thumbnailPathForItem:(VivaldiSpeedDialItem*)item {
  if ([self filePathFromItem:item]) {
    return [self filePathFromItem:item];
  } else if ([self legacyFilePathFromItem:item]) {
    return [self legacyFilePathFromItem:item];
  } else {
    return nil;
  }
}

/// Returns the file path of the thumbnail which is within application
/// support directory and not exposed to user otherwise.
- (NSString*)filePathFromItem:(VivaldiSpeedDialItem*)item {
  // Get the Application Support directory
  NSFileManager *fileManager = [NSFileManager defaultManager];
  NSURL *applicationSupportDir =
      [[fileManager URLsForDirectory:NSApplicationSupportDirectory
                           inDomains:NSUserDomainMask] firstObject];

  NSString *dataPath = [applicationSupportDir path];

  if (!dataPath) return nil;

  NSString *thumbnailPathString = item.thumbnail;
  if (!thumbnailPathString || thumbnailPathString.length == 0)
    return nil;

  NSString *finalPath =
      [dataPath stringByAppendingPathComponent:thumbnailPathString];
  return finalPath;
}

/// Returns the path for legacy items that were unexpectedly
/// exposed on user documents directory.
- (NSString*)legacyFilePathFromItem:(VivaldiSpeedDialItem*)item {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask,
                                                       YES);
  NSString *documentsDirectory = [paths firstObject];
  if (!documentsDirectory)
    return nil;

  NSString *thumbnailPathString = item.thumbnail;
  if (!thumbnailPathString || thumbnailPathString.length == 0)
    return nil;

  NSString *finalPath =
    [documentsDirectory
        stringByAppendingPathComponent: thumbnailPathString];
  return finalPath;
}

/// Removes the file from path if exists.
- (void)removeFileFromPath:(NSString*)path {
  NSFileManager *fileManager = [NSFileManager defaultManager];
  BOOL isFileExist = [fileManager fileExistsAtPath:path];

  if (isFileExist) {
    [[NSFileManager defaultManager] removeItemAtPath:path
                                               error:nil];
  }
}

/// Crop the snapshot to provided size and returns.
/// Original snapshot size could be unnecessarily higher res than required.
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
