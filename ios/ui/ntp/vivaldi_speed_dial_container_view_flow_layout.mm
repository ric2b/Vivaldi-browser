// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_container_view_flow_layout.h"

namespace  {
// Constants for layout configurations
const CGFloat kSpacing = 16.0;
const CGFloat kMinimumSidePadding = 16.0;
const CGFloat kListStyleItemHeight = 76.0;
const CGFloat kMinimumItemWidthDecrease = 1.0;

// Device-specific widths for different layout styles
const CGFloat kSmallStyleWidthPad = 100.0;
const CGFloat kSmallStyleWidthPhone = 80.0;
const CGFloat kMediumStyleWidthPad = 140.0;
const CGFloat kMediumStyleWidthPhone = 100.0;
const CGFloat kLargeStyleWidthPad = 180.0;
const CGFloat kLargeStyleWidthPhone = 160.0;
const CGFloat kListStyleWidth = 300.0;

// Device-specific widths for different layout styles for preview
// Only for iPads since iPad preview screen doesn't reflect the main layout
// size due to being presented as modal.

const CGFloat kSmallStyleWidthPadPreview = 80.0;
const CGFloat kMediumStyleWidthPadPreview = 100.0;
const CGFloat kLargeStyleWidthPadPreview = 120.0;
const CGFloat kListStyleWidthPreview = 140.0;
}

@implementation VivaldiSpeedDialViewContainerViewFlowLayout

- (instancetype)init {
  return [super init];
}

- (CGFloat)minimumWidthForStyle:(VivaldiStartPageLayoutStyle)style {
  switch (style) {
    case VivaldiStartPageLayoutStyleSmall:
      return [self isTablet] ? [self iPadColumnWidth] : kSmallStyleWidthPhone;
    case VivaldiStartPageLayoutStyleMedium:
      return [self isTablet] ? [self iPadColumnWidth] : kMediumStyleWidthPhone;
    case VivaldiStartPageLayoutStyleLarge:
      return [self isTablet] ? [self iPadColumnWidth] : kLargeStyleWidthPhone;
    case VivaldiStartPageLayoutStyleList:
      return [self isTablet] ? [self iPadColumnWidth] : kListStyleWidth;
  }
}

- (void)prepareLayout {
  [super prepareLayout];

  if (!self.collectionView) return;

  self.minimumInteritemSpacing = kSpacing;
  self.minimumLineSpacing = kSpacing;

  CGFloat availableWidth =  CGRectGetWidth(self.collectionView.bounds) -
      (self.collectionView.contentInset.left +
        self.collectionView.contentInset.right);
  CGFloat styleMinWidth = [self minimumWidthForStyle:self.layoutStyle];
  CGFloat itemWidth = styleMinWidth;
  CGFloat totalWidthNeeded = 0;
  NSInteger adjustedNumberOfColumns = 0;
  double numberOfColumns = (double)self.numberOfColumns;

  do {
    adjustedNumberOfColumns =
        MIN(numberOfColumns,
            floor((availableWidth + kSpacing) / (itemWidth + kSpacing)));
    totalWidthNeeded = (itemWidth * adjustedNumberOfColumns) +
        (kSpacing * (adjustedNumberOfColumns - 1));
    if (totalWidthNeeded > availableWidth) {
      itemWidth -= kMinimumItemWidthDecrease;
    }
  } while (totalWidthNeeded > availableWidth && itemWidth > styleMinWidth);

  CGFloat extraSpace = availableWidth - totalWidthNeeded;
  CGFloat oneColumnWidth = itemWidth + kSpacing;

  if (extraSpace < oneColumnWidth && adjustedNumberOfColumns >= 1) {
    self.sectionInset = UIEdgeInsetsMake(
        kSpacing, kMinimumSidePadding, kSpacing, kMinimumSidePadding);
    CGFloat remainingWidth =
        availableWidth - (kMinimumSidePadding * 2) -
            (kSpacing * (adjustedNumberOfColumns - 1));
    itemWidth = remainingWidth / adjustedNumberOfColumns;
  } else {
    CGFloat edgeInsets = MAX((availableWidth - totalWidthNeeded) / 2, kSpacing);
    self.sectionInset = UIEdgeInsetsMake(
        kSpacing, edgeInsets, kSpacing, edgeInsets);
  }

  CGFloat itemHeight = self.layoutStyle == VivaldiStartPageLayoutStyleList ?
      kListStyleItemHeight : itemWidth;
  self.itemSize = CGSizeMake(itemWidth, itemHeight);
}

#pragma mark - Private
- (BOOL)isTablet {
  return UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad;
}

- (CGFloat)iPadColumnWidth {
  switch (self.layoutStyle) {
    case VivaldiStartPageLayoutStyleSmall:
      return [self isPreview] ?
          kSmallStyleWidthPadPreview : kSmallStyleWidthPad;
    case VivaldiStartPageLayoutStyleMedium:
      return [self isPreview] ?
          kMediumStyleWidthPadPreview : kMediumStyleWidthPad;
    case VivaldiStartPageLayoutStyleLarge:
      return [self isPreview] ?
          kLargeStyleWidthPadPreview : kLargeStyleWidthPad;
    case VivaldiStartPageLayoutStyleList:
      return [self isPreview] ?
          kListStyleWidthPreview : kListStyleWidth;
  }
}

@end
