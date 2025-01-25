// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/bookmarks_editor/vivaldi_bookmark_folder_selection_header_view.h"

#import "UIKit/UIKit.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_constants.h"
#import "ios/ui/custom_views/vivaldi_search_bar_view.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"


using l10n_util::GetNSString;

namespace {
// Padding for the container holds the label and switch.
// In order: Top, Left, Bottom, Right
UIEdgeInsets sdSelectionViewPadding = UIEdgeInsetsMake(0, 16, 0, 16);
// Padding for speed dial toggle
UIEdgeInsets speedDialTogglePadding = UIEdgeInsetsMake(0, 0, 8, 4);
// Padding for the speed dial selection label
UIEdgeInsets speedDialSelectionLabelPadding = UIEdgeInsetsMake(0, 4, 0, 8);

}  // namespace

@interface VivaldiBookmarkFolderSelectionHeaderView()
                                                  <VivaldiSearchBarViewDelegate>
// Label for the folder name.
@property (nonatomic,weak) VivaldiSearchBarView* searchBar;
// The label that describes the text for speed dial selection.
@property (nonatomic,weak) UILabel* speedDialSelectionLabel;
// Speed dial toggle.
@property (nonatomic,weak) UISwitch* setSpeedDialToggle;

@end

@implementation VivaldiBookmarkFolderSelectionHeaderView
@synthesize searchBar = _searchBar;
@synthesize speedDialSelectionLabel = _speedDialSelectionLabel;
@synthesize setSpeedDialToggle = _setSpeedDialToggle;

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
    GetNSString(IDS_IOS_BOOKMARK_FOLDER_SELECTION_SEARCHBAR_PLACEHOLDER);
  [searchBar setPlaceholder:placeholderString];
  searchBar.delegate = self;

  [self addSubview:searchBar];
  [searchBar anchorTop:self.topAnchor
               leading:self.leadingAnchor
                bottom:nil
              trailing:self.trailingAnchor];


  // Speed dial selection view that holds the description label and the toggle.
  UIView* speedDialSelectionView = [UIView new];
  speedDialSelectionView.backgroundColor = UIColor.clearColor;

  [self addSubview:speedDialSelectionView];
  [speedDialSelectionView anchorTop:searchBar.bottomAnchor
                            leading:searchBar.safeLeftAnchor
                             bottom:self.bottomAnchor
                           trailing:searchBar.safeRightAnchor
                            padding:sdSelectionViewPadding];

  // Toggle button
  UISwitch* toggle = [UISwitch new];
  _setSpeedDialToggle = toggle;
  [speedDialSelectionView addSubview:toggle];
  [toggle addTarget:self
             action:@selector(showSpeedDialFoldersToggleDidChange:)
   forControlEvents:UIControlEventValueChanged];

  [toggle anchorTop:speedDialSelectionView.topAnchor
            leading:nil
             bottom:speedDialSelectionView.bottomAnchor
           trailing:speedDialSelectionView.trailingAnchor
            padding:speedDialTogglePadding];
  [toggle centerYInSuperview];

  // Speed dial selection view description label
  UILabel* speedDialSelectionLabel = [[UILabel alloc] init];
  _speedDialSelectionLabel = speedDialSelectionLabel;
  NSString* titleString = GetNSString(IDS_IOS_BOOKMARK_SHOW_ONLY_GROUPS);
  speedDialSelectionLabel.text = titleString;
  speedDialSelectionLabel.accessibilityLabel = titleString;
  speedDialSelectionLabel.textColor = UIColor.labelColor;
  speedDialSelectionLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  speedDialSelectionLabel.numberOfLines = 1;
  speedDialSelectionLabel.textAlignment = NSTextAlignmentLeft;

  [speedDialSelectionView addSubview:speedDialSelectionLabel];
  [_speedDialSelectionLabel anchorTop:toggle.topAnchor
                              leading:speedDialSelectionView.leadingAnchor
                               bottom:toggle.bottomAnchor
                             trailing:toggle.leadingAnchor
                              padding:speedDialSelectionLabelPadding];
}

#pragma mark - PRIVATE
- (void)showSpeedDialFoldersToggleDidChange:(UISwitch*)sender {
  if (self.delegate)
    [self.delegate didChangeShowOnlySpeedDialFoldersState:sender.isOn];
}

#pragma mark - VivaldiSearchBarViewDelegate
- (void)searchBarTextDidChange:(NSString*)searchText {
  if (self.delegate)
    [self.delegate searchBarTextDidChange:searchText];
}

#pragma mark - GETTERS
- (BOOL)showOnlySpeedDialFolder {
  return self.setSpeedDialToggle.on;
}

#pragma mark - SETTERS
- (void)setShowOnlySpeedDialFolder:(BOOL)show {
  [self.setSpeedDialToggle setOn:show animated:NO];
}

@end
