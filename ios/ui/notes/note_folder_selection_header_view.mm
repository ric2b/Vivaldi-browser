// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/notes/note_folder_selection_header_view.h"

#import "UIKit/UIKit.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/custom_views/vivaldi_search_bar_view.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

@interface NoteFolderSelectionHeaderView()<VivaldiSearchBarViewDelegate>
// Label for the folder name.
@property (nonatomic,weak) VivaldiSearchBarView* searchBar;

@end

@implementation NoteFolderSelectionHeaderView
@synthesize searchBar = _searchBar;

#pragma mark - INITIALIZER
- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.backgroundColor = UIColor.clearColor;
    [self setUpUI];
  }
  return self;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {

  // Search bar
  VivaldiSearchBarView* searchBar = [VivaldiSearchBarView new];
  _searchBar = searchBar;
  NSString* placeholderString =
    GetNSString(IDS_IOS_NOTE_FOLDER_SELECTION_SEARCHBAR_PLACEHOLDER);
  [searchBar setPlaceholder:placeholderString];
  searchBar.delegate = self;

  [self addSubview:searchBar];
  [searchBar anchorTop:self.topAnchor
               leading:self.leadingAnchor
                bottom:self.bottomAnchor
              trailing:self.trailingAnchor];

}

#pragma mark - VivaldiSearchBarViewDelegate
- (void)searchBarTextDidChange:(NSString*)searchText {
  if (self.delegate)
    [self.delegate searchBarTextDidChange:searchText];
}

@end
