#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/table_view_bookmarks_folder_item.h"

using vivaldi_bookmark_kit::GetNickname;
using vivaldi_bookmark_kit::GetThumbnail;
using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi_bookmark_kit::GetDescription;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface VivaldiSpeedDialItem() <NSItemProviderWriting>{}
@end

@implementation VivaldiSpeedDialItem

@synthesize bookmarkNode = _bookmarkNode;

#pragma mark - INITIALIZER
- (instancetype) initWithBookmark: (const bookmarks::BookmarkNode*)node {
  self = [super init];
  if (self) {
    _bookmarkNode = node;
  }
  return self;
}

#pragma mark - NSITEMPROVIDER_WRITING
+ (NSArray<NSString *> *)writableTypeIdentifiersForItemProvider {
  return @[@"public.url"];
}

- (nullable NSProgress*)loadDataWithTypeIdentifier:(NSString *)typeIdentifier
      forItemProviderCompletionHandler: (void (^)(NSData * _Nullable data,
          NSError * _Nullable error))completionHandler {
  return [NSProgress new];
}

#pragma mark - GETTERS
- (int64_t)id {
  return _bookmarkNode->id();
}

- (NSNumber*)idValue {
  return @(self.id);
}

- (BOOL)isFolder {
  return _bookmarkNode->is_folder();
}

- (BOOL)isSpeedDial {
  return GetSpeeddial(_bookmarkNode);
}

- (NSString*)title {
  return bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
}

- (NSString*)nickname {
  const std::string nick = GetNickname(_bookmarkNode);
  return [NSString stringWithUTF8String:nick.c_str()];
}

- (GURL)url {
  return _bookmarkNode->url();
}

- (NSString*)urlString {
  return base::SysUTF8ToNSString(self.url.spec());
}

- (NSString*)host {
  NSURL* nsURL = [NSURL URLWithString:[self urlString]];
  NSString* host = [nsURL host];

  NSString* replaceRange = @"www.";
  if (([host length] >= [replaceRange length]) &&
      ([[host substringToIndex: [replaceRange length]]
        isEqualToString:replaceRange]))
    return [host substringFromIndex: [replaceRange length]];
  else
      return host;
}

- (BOOL)isInternalPage {
  NSString* prefixStringVivaldi = @"vivaldi://";
  NSString* prefixStringChrome = @"chrome://";
  NSString* urlString = [[self urlString] lowercaseString];

  BOOL isInternal = NO;

  if (urlString.length > 0) {
    if ([urlString containsString:prefixStringVivaldi] ||
        [urlString containsString:prefixStringChrome])
      isInternal = YES;
  } else {
    return NO;
  }

  return isInternal;
}

- (NSString*)thumbnail {
  const std::string thumb = GetThumbnail(_bookmarkNode);
  return [NSString stringWithUTF8String:thumb.c_str()];
}

- (NSString*)description {
  const std::string desciptionStr = GetDescription(_bookmarkNode);
  return [NSString stringWithUTF8String:desciptionStr.c_str()];
}

- (NSDate*)createdAt {
  base::Time dateAdded = _bookmarkNode->date_added();
  return dateAdded.ToNSDate();
}

- (const bookmarks::BookmarkNode*)parent {
  return _bookmarkNode->parent();
}

@end
