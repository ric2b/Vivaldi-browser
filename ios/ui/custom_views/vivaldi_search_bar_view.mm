// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/custom_views/vivaldi_search_bar_view.h"

#import "UIKit/UIKit.h"

#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding for the search bar
UIEdgeInsets searchBarPadding = UIEdgeInsetsMake(8, 12, 8, 12);

}  // namespace

@interface VivaldiSearchBarView()<UISearchBarDelegate>
@property (nonatomic,weak) UISearchBar* searchBar;
@end

@implementation VivaldiSearchBarView
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
  UISearchBar* searchBar = [UISearchBar new];
  _searchBar = searchBar;
  [searchBar setDelegate:self];
  [searchBar setBarTintColor:[UIColor clearColor]];
  searchBar.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  searchBar.backgroundColor = UIColor.clearColor;
  searchBar.searchBarStyle = UISearchBarStyleMinimal;

  [self addSubview:searchBar];
  [searchBar fillSuperviewToSafeAreaInsetWithPadding:searchBarPadding];
}

#pragma mark - PRIVATE
// Only remove focus from the search bar when cancel or search button on
// the keyboard is tapped. We are showing the results as user types on the
// tableview, therefore no search tap action is required.
- (void)defocusSearchBar {
  [self.searchBar setShowsCancelButton:false animated:true];
  [self.searchBar resignFirstResponder];
}

#pragma mark - UISEARCHBAR DELEGATE
- (void)searchBar:(UISearchBar*)searchBar
    textDidChange:(NSString*)searchText {
  if (self.delegate)
    [self.delegate searchBarTextDidChange:searchText];
}

- (void)searchBarTextDidBeginEditing:(UISearchBar*)searchBar {
  [self.searchBar setShowsCancelButton:true animated:true];
}

- (void)searchBarSearchButtonClicked:(UISearchBar*)searchBar {
  [self defocusSearchBar];
}

- (void)searchBarCancelButtonClicked:(UISearchBar*)searchBar {
  [self defocusSearchBar];
}

#pragma mark - SETTER
- (void)setPlaceholder:(NSString*)placeholder {
  [_searchBar setPlaceholder:placeholder];
}

- (void)setSearchText:(NSString*)searchTerms {
  _searchBar.text = searchTerms;
}

@end
