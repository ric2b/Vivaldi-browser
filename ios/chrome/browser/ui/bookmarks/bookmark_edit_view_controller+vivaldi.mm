// Copyright 2022 Vivaldi Technologies. All rights reserved.


#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller+vivaldi.h"

#import <objc/runtime.h>
#include <memory>

#include "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

namespace {
const void* const key = &key;

}

@implementation BookmarkEditViewController(Vivaldi)

- (void)vivaldiAddDescriptionItem:(NSInteger)type
    toSection:(NSInteger)section
    descriptionText:(NSString*)text
    delegate:(id<BookmarkTextFieldItemDelegate>)delegate {
  self.descriptionItem = [[BookmarkTextFieldItem alloc] initWithType:type];
  self.descriptionItem.accessibilityIdentifier = @"Description";
  self.descriptionItem.placeholder =
        l10n_util::GetNSString(IDS_VIVALDI_IOS_BOOKMARK_DESCRIPTION_HEADER);
  self.descriptionItem.text = text;
  self.descriptionItem.delegate = delegate;

  TableViewModel* model = self.tableViewModel;
  [model addItem:self.descriptionItem toSectionWithIdentifier:section];
}

- (NSString*)inputDescription {
  return self.descriptionItem.text;
}

- (void)setDescriptionItem:(NSString *)item {
    objc_setAssociatedObject(self, &key,
      item, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (BookmarkTextFieldItem*)descriptionItem {
    return objc_getAssociatedObject(self, &key);
}

@end
