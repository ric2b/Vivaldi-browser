// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_home_node_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/table_view_bookmarks_folder_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_constants.h"

using vivaldi::IsVivaldiRunning;
using vivaldi_bookmark_kit::GetSpeeddial;
using vivaldi_bookmark_kit::GetNickname;
using vivaldi_bookmark_kit::GetDescription;
// End Vivaldi

@implementation BookmarksHomeNodeItem
@synthesize bookmarkNode = _bookmarkNode;

- (instancetype)initWithType:(NSInteger)type
                bookmarkNode:(const bookmarks::BookmarkNode*)node {
  if ((self = [super initWithType:type])) {
    if (node->is_folder()) {
      self.cellClass = [TableViewBookmarksFolderCell class];
    } else {
      self.cellClass = [TableViewURLCell class];
    }
    _bookmarkNode = node;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  if (_bookmarkNode->is_folder()) {
    if (IsVivaldiRunning()) {
      TableViewBookmarksFolderCell* bookmarkCell =
        base::apple::ObjCCastStrict<TableViewBookmarksFolderCell>(cell);
      bookmarkCell.folderTitleTextField.text =
        bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
      if (self.shouldShowTrashIcon) {
        bookmarkCell.folderImageView.image =
            [UIImage imageNamed:vBookmarkTrashFolder];
      } else {
        bookmarkCell.folderImageView.image = GetSpeeddial(_bookmarkNode) ?
          [UIImage imageNamed: vSpeedDialFolderIcon] :
          [UIImage imageNamed: vBookmarksFolderIcon];
      }
      bookmarkCell.folderImageView.contentMode =
          UIViewContentModeScaleAspectFit;
      bookmarkCell.bookmarksAccessoryType =
          BookmarksFolderAccessoryTypeDisclosureIndicator;
      bookmarkCell.accessibilityIdentifier =
          bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
      bookmarkCell.accessibilityTraits |= UIAccessibilityTraitButton;
    } else {
    TableViewBookmarksFolderCell* bookmarkCell =
        base::apple::ObjCCastStrict<TableViewBookmarksFolderCell>(cell);
    bookmarkCell.folderTitleTextField.text =
        bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
    bookmarkCell.folderImageView.image =
        [UIImage imageNamed:@"bookmark_blue_folder"];
    bookmarkCell.bookmarksAccessoryType =
        BookmarksFolderAccessoryTypeDisclosureIndicator;
    bookmarkCell.accessibilityIdentifier =
        bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
    bookmarkCell.accessibilityTraits |= UIAccessibilityTraitButton;
    bookmarkCell.cloudSlashedView.hidden = !self.shouldDisplayCloudSlashIcon;
    } // Vivaldi
  } else {
    TableViewURLCell* urlCell =
        base::apple::ObjCCastStrict<TableViewURLCell>(cell);
    urlCell.titleLabel.text =
        bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
    urlCell.URLLabel.text = base::SysUTF16ToNSString(
        url_formatter::
            FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
                _bookmarkNode->url()));
    urlCell.accessibilityTraits |= UIAccessibilityTraitButton;
    urlCell.metadataImage.image =
        self.shouldDisplayCloudSlashIcon
            ? CustomSymbolWithPointSize(kCloudSlashSymbol,
                                        kCloudSlashSymbolPointSize)
            : nil;
    urlCell.metadataImage.tintColor = CloudSlashTintColor();
    [urlCell configureUILayout];
  }
}

#pragma mark - Vivaldi

- (BOOL)isFolder {
  return _bookmarkNode->is_folder();
}

- (NSString*)title {
  return bookmark_utils_ios::TitleForBookmarkNode(_bookmarkNode);
}

- (NSString*)nickname {
  const std::string nick = GetNickname(_bookmarkNode);
  return [NSString stringWithUTF8String:nick.c_str()];
}

- (NSString*)urlString {
  return base::SysUTF8ToNSString(_bookmarkNode->url().spec());
}

- (NSString*)description {
  const std::string desciptionStr = GetDescription(_bookmarkNode);
  return [NSString stringWithUTF8String:desciptionStr.c_str()];
}

- (NSDate*)createdAt {
  base::Time dateAdded = _bookmarkNode->date_added();
  return dateAdded.ToNSDate();
}

@end
