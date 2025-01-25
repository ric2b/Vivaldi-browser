// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/panel/panel_button_view.h"

#import "UIKit/UIKit.h"

#import "ios/panel/panel_button_cell.h"
#import "ios/panel/panel_interaction_controller.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

// NAMESPACE
namespace {
// Cell Identifier for the top menu CV Cell.
NSString* cellId = @"cellId";

NSInteger numberOfPages = 5;

CGFloat iconSize = 56.0;
}

@interface PanelButtonView() <UICollectionViewDelegate,
                              UICollectionViewDataSource> {
    NSInteger activeIndex;
}
@property (weak,nonatomic) UICollectionView *collectionView;
@end

@implementation PanelButtonView

@synthesize collectionView = _collectionView;

#pragma mark - INITIALIZER
- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setUpUI];
  }
  return self;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {
  self.backgroundColor = UIColor.clearColor;
  activeIndex = 0;

  // COLLECTION VIEW FLOW LAYOUT
  UICollectionViewFlowLayout *layout= [[UICollectionViewFlowLayout alloc] init];
  UICollectionView* collectionView =
  [[UICollectionView alloc] initWithFrame: self.frame
                     collectionViewLayout:layout];
  _collectionView = collectionView;
  layout.scrollDirection = UICollectionViewScrollDirectionVertical;
  layout.minimumLineSpacing = 0;
  layout.minimumInteritemSpacing = 0;

  [self.collectionView setDataSource: self];
  [self.collectionView setDelegate: self];
  self.collectionView.showsHorizontalScrollIndicator = false;
  self.collectionView.showsVerticalScrollIndicator = false;
  self.collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;

  [self.collectionView registerClass:[PanelButtonCell class]
      forCellWithReuseIdentifier:cellId];
  [self.collectionView setBackgroundColor:[UIColor clearColor]];

  [self addSubview:self.collectionView];
  [self.collectionView fillSuperview];
}

#pragma mark - SETTERS

- (void)selectItemWithIndex:(NSInteger)index {
    activeIndex = index;
    [self scrollToItemWithIndex:index];
}

#pragma mark - PRIVATE METHODS
- (void)scrollToItemWithIndex:(NSInteger)index {
  NSIndexPath *indexPath = [NSIndexPath indexPathForRow:index
                                              inSection:0];
  [self.collectionView
      selectItemAtIndexPath:indexPath
                   animated:true
             scrollPosition:UICollectionViewScrollPositionCenteredHorizontally];
}

#pragma mark - COLLECTIONVIEW DELEGATE, DATA SOURCE & FLOW LAYOUT

- (NSInteger)collectionView:(UICollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section {
    return numberOfPages;
}

- (UICollectionViewCell*)collectionView:(UICollectionView *)collectionView
                           cellForItemAtIndexPath:(NSIndexPath *)indexPath {
  PanelButtonCell *cell =
    [collectionView dequeueReusableCellWithReuseIdentifier:cellId
                                              forIndexPath:indexPath];
  [cell configureCellWithIndex:indexPath.row
                   highlighted:indexPath.row == activeIndex];
  return cell;
}

- (void)collectionView:(UICollectionView *)collectionView
didSelectItemAtIndexPath:(NSIndexPath *)indexPath {
  activeIndex = indexPath.row;
  if (self.delegate) {
    [self.delegate didSelectIndex:indexPath.row];
  }
  [self.collectionView reloadData];
  [self scrollToItemWithIndex:indexPath.row];
}

- (CGSize)collectionView:(UICollectionView *)collectionView
                  layout:(UICollectionViewLayout *)collectionViewLayout
  sizeForItemAtIndexPath:(NSIndexPath *)indexPath {
  return CGSizeMake(iconSize, iconSize);
}

@end
