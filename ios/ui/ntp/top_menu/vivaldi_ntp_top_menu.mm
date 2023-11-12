// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/top_menu/vivaldi_ntp_top_menu.h"

#import "UIKit/UIKit.h"

#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/top_menu/vivaldi_ntp_top_menu_cell.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// NAMESPACE
namespace {
// Cell Identifier for the top menu CV Cell.
NSString* cellId = @"cellId";
// Extra padding added with the menu item string width.
CGFloat textPadding = 16.0;
// Collection view line spacing between items.
CGFloat lineSpacing = 12.0;
}

@interface VivaldiNTPTopMenuView() <UICollectionViewDelegate,
                                    UICollectionViewDataSource> {}
@property (weak,nonatomic) UICollectionView *collectionView;
@property (strong,nonatomic) NSArray *menuItems;
@property (assign,nonatomic) CGFloat navBarHeight;
@end

@implementation VivaldiNTPTopMenuView

@synthesize collectionView = _collectionView;
@synthesize menuItems = _menuItems;
@synthesize navBarHeight = _navBarHeight;

#pragma mark - INITIALIZER
- (instancetype)initWithHeight:(CGFloat)height {
  if (self = [super initWithFrame:CGRectZero]) {
    _navBarHeight = height;
    [self setUpUI];
  }
  return self;
}


#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {
  self.backgroundColor = UIColor.clearColor;

  // COLLECTION VIEW FLOW LAYOUT
  UICollectionViewFlowLayout *layout= [[UICollectionViewFlowLayout alloc] init];
  UICollectionView* collectionView =
  [[UICollectionView alloc] initWithFrame: self.frame
                     collectionViewLayout:layout];
  _collectionView = collectionView;
  layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
  layout.minimumLineSpacing = lineSpacing;
  layout.minimumInteritemSpacing = 0;

  [self.collectionView setDataSource: self];
  [self.collectionView setDelegate: self];
  self.collectionView.showsHorizontalScrollIndicator = false;
  self.collectionView.showsVerticalScrollIndicator = false;
  self.collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;

  [self.collectionView registerClass:[VivaldiNTPTopMenuCell class]
      forCellWithReuseIdentifier:cellId];
  [self.collectionView setBackgroundColor:[UIColor clearColor]];

  [self addSubview:self.collectionView];
  [self.collectionView fillSuperview];
}

#pragma mark - SETTERS
- (void)setMenuItemsWithSDFolders:(NSArray*)folders
                    selectedIndex:(NSInteger)selectedIndex {

  self.menuItems = folders;
  [CATransaction begin];
  [CATransaction setValue:(id)kCFBooleanTrue
                   forKey:kCATransactionDisableActions];
  [self.collectionView performBatchUpdates:^{
    [self.collectionView reloadSections:[NSIndexSet indexSetWithIndex:0]];
  } completion: ^(BOOL finished){
    [self selectItemWithIndex:selectedIndex animated:NO];
  }];
  [CATransaction commit];
}

- (void)selectItemWithIndex:(NSInteger)index
                   animated:(BOOL)animated {
  if (self.menuItems.count > 0) {
    [self scrollToItemWithIndex:index
                       animated:animated];
  }
}

- (void)invalidateLayoutWithHeight:(CGFloat)height {
  self.navBarHeight = height;
  [self.collectionView.collectionViewLayout invalidateLayout];
}

- (void)removeAllItems {
  self.menuItems = @[];
  [self.collectionView reloadData];
}

#pragma mark - PRIVATE METHODS
- (void)scrollToItemWithIndex:(NSInteger)index
                     animated:(BOOL)animated {
  NSIndexPath *indexPath = [NSIndexPath indexPathForRow:index
                                              inSection:0];
  [self.collectionView
      selectItemAtIndexPath:indexPath
                   animated:animated
             scrollPosition:UICollectionViewScrollPositionCenteredHorizontally];
}

#pragma mark - COLLECTIONVIEW DELEGATE, DATA SOURCE & FLOW LAYOUT

- (NSInteger)collectionView:(UICollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section{
    return self.menuItems.count;
}

- (UICollectionViewCell*)collectionView:(UICollectionView *)collectionView
                           cellForItemAtIndexPath:(NSIndexPath *)indexPath{
  VivaldiNTPTopMenuCell *cell =
    [collectionView dequeueReusableCellWithReuseIdentifier:cellId
                                              forIndexPath:indexPath];
  VivaldiSpeedDialItem *item =
    [self.menuItems objectAtIndex:indexPath.row];
  [cell configureCellWithItem:item];
  return cell;
}

- (void)collectionView:(UICollectionView *)collectionView
didSelectItemAtIndexPath:(NSIndexPath *)indexPath {
  if (self.delegate) {
    VivaldiSpeedDialItem *selectedItem =
      [self.menuItems objectAtIndex:indexPath.row];
    [self.delegate didSelectItem:selectedItem index:indexPath.row];
  }
  [self scrollToItemWithIndex:indexPath.row animated:YES];
}

- (CGSize)collectionView:(UICollectionView *)collectionView
                  layout:(UICollectionViewLayout *)collectionViewLayout
  sizeForItemAtIndexPath:(NSIndexPath *)indexPath {

  VivaldiSpeedDialItem *selectedItem =
      [self.menuItems objectAtIndex:indexPath.row];
  // Get the title for the item and calculate the item size on the fly
  // based on the title width.
  NSString *selectedItemTitle = selectedItem.title;
  CGSize stringSize =
    [selectedItemTitle
      sizeWithAttributes:
       @{NSFontAttributeName:
          [UIFont preferredFontForTextStyle:UIFontTextStyleBody]}];
  // Adding extra padding with the width for spacing between two items.
  CGFloat width = stringSize.width + textPadding;

  return CGSizeMake(width, self.navBarHeight);
}

@end
