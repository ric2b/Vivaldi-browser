#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"

using vivaldi_bookmark_kit::GetNickname;
using vivaldi_bookmark_kit::GetThumbnail;
using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi_bookmark_kit::GetDescription;

@interface VivaldiSpeedDialItem() <NSItemProviderWriting>{}
@end

@implementation VivaldiSpeedDialItem

@synthesize bookmarkNode = _bookmarkNode;

#pragma mark - INITIALIZER
- (instancetype)initWithBookmark:(const bookmarks::BookmarkNode*)node {
  self = [super init];
  if (self) {
    if (node != nullptr) {
      _bookmarkNode = node;
      _title = bookmark_utils_ios::TitleForBookmarkNode(node);
      _url = node->url();
      _isFolder = node->is_folder();
      _isSpeedDial = GetSpeeddial(_bookmarkNode);
    }
    _isFrequentlyVisited = NO;
  }
  return self;
}

- (instancetype)initWithTitle:(NSString*)title
                          url:(GURL)url {
  self = [super init];
  if (self) {
    _title = title;
    _url = url;
    _isFolder = NO;
    _isSpeedDial = NO;
    _isFrequentlyVisited = YES;
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

- (NSString*)nickname {
  const std::string nick = GetNickname(_bookmarkNode);
  return [NSString stringWithUTF8String:nick.c_str()];
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
  if (![self urlString])
    return NO;
  return [VivaldiGlobalHelpers isURLInternalPage:[self urlString]];
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
