// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_speed_dial_container_view_flow_layout.h"

#import "ios/ui/helpers/vivaldi_global_helpers.h"

namespace  {
// Constants for layout configurations
const CGFloat kSpacing = 16.0;
const CGFloat kTopInsetForHiddenToolbar = 0;
const CGFloat kMinimumSidePadding = 16.0;
const CGFloat kListStyleItemHeight = 76.0;
const CGFloat kMinimumItemWidthDecrease = 1.0;

// Device-specific widths for different layout styles
const CGFloat kSmallStyleWidthPad = 100.0;
const CGFloat kSmallStyleWidthPadTopSites = 60.0;
const CGFloat kSmallStyleWidthPhone = 80.0;
const CGFloat kSmallStyleWidthPhoneTopSites = 48.0;
const CGFloat kMediumStyleWidthPad = 140.0;
const CGFloat kMediumStyleWidthPhone = 100.0;
const CGFloat kLargeStyleWidthPad = 180.0;
const CGFloat kLargeStyleWidthPhone = 160.0;
const CGFloat kListStyleWidth = 300.0;

// Device-specific min column widths for different layout styles for preview.
// iPad has two states, one is small modal which is presented within the start
// page settings. And, another one is for wallpaper settings preview which is
// a larger full size modal. We need to provide different sizes for the column
// to create a similar preview as the final layout since the main window and
// and modal has different window size.

// iPad Preview Sizes for Small Modal
const CGFloat kSmallStyleWidthPadPreview = 70.0;
const CGFloat kMediumStyleWidthPadPreview = 90.0;
const CGFloat kLargeStyleWidthPadPreview = 110.0;
const CGFloat kListStyleWidthPadPreview = 260.0;

// iPad Preview Sizes for Full Modal
const CGFloat kSmallStyleWidthPadPreviewFull = 80.0;
const CGFloat kMediumStyleWidthPadPreviewFull = 110.0;
const CGFloat kLargeStyleWidthPadPreviewFull = 140.0;

// iPhone Preview Sizes
const CGFloat kSmallStyleWidthPhonePreview = 70.0;
const CGFloat kMediumStyleWidthPhonePreview = 100.0;
const CGFloat kLargeStyleWidthPhonePreview = 130.0;
const CGFloat kListStyleWidthPhonePreview = 260;

// Common
const CGFloat kMinimumSidePaddingPreview = 2.0;
}

@implementation VivaldiSpeedDialViewContainerViewFlowLayout

- (instancetype)init {
  return [super init];
}

- (CGFloat)minimumWidthForStyle:(VivaldiStartPageLayoutStyle)style {
  return [self shouldShowTabletLayout] ?
      [self iPadColumnWidth] : [self iPhoneColumnWidth];
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
        [self kTopInset], [self horizontalSpacing],
        kSpacing, [self horizontalSpacing]);
    CGFloat remainingWidth =
        availableWidth - ([self horizontalSpacing] * 2) -
            (kSpacing * (adjustedNumberOfColumns - 1));
    itemWidth = remainingWidth / adjustedNumberOfColumns;
  } else {
    CGFloat edgeInsets = MAX((availableWidth - totalWidthNeeded) / 2, kSpacing);
    self.sectionInset = UIEdgeInsetsMake(
        [self kTopInset], edgeInsets, kSpacing, edgeInsets);
  }

  CGFloat itemHeight = self.layoutStyle == VivaldiStartPageLayoutStyleList ?
      kListStyleItemHeight : itemWidth;
  self.itemSize = CGSizeMake(itemWidth, itemHeight);
}

/// Returns the minimum height needed to render the items
/// or the maxNumberOfRows, whichever is minimum.
- (CGFloat)heightNeededForContents {
  NSInteger numberOfItems = [self.collectionView numberOfItemsInSection:0];
  if (numberOfItems == 0) {
    return 0;
  }

  CGFloat itemHeight = self.itemSize.height;
  CGFloat lineSpacing = self.minimumLineSpacing;

  // Calculate the number of columns
  NSInteger numberOfColumns = [self numberOfRenderedColumns];
  // Calculate the actual number of rows needed
  NSInteger actualNumberOfRows =
      (numberOfItems + numberOfColumns - 1) / numberOfColumns;
  // Determine the number of rows to use.
  NSInteger numberOfRowsToUse =
      MIN(actualNumberOfRows, _maxNumberOfRows);
  // Calculate total height
  CGFloat totalHeight =
      (itemHeight * numberOfRowsToUse) +
          (lineSpacing * (numberOfRowsToUse - 1)) +
              self.sectionInset.top + self.sectionInset.bottom;

  return totalHeight;
}

- (void)adjustCollectionViewHeight {
  CGRect frame = self.collectionView.frame;
  CGFloat height = [self heightNeededForContents];
  self.collectionView.frame = frame;
  self.collectionView.scrollEnabled = NO;
  self.collectionView.contentSize = CGSizeMake(frame.size.width, height);
  [self.collectionView setNeedsLayout];
  [self.collectionView layoutIfNeeded];
}

#pragma mark - Private

- (CGFloat)horizontalSpacing {
  return self.layoutState == VivaldiStartPageLayoutStateNormal ?
      kMinimumSidePadding : kMinimumSidePaddingPreview;
}

- (BOOL)isSmallModalPreview {
  return self.layoutState == VivaldiStartPageLayoutStatePreviewModalSmall;
}

- (BOOL)isPreview {
  return self.layoutState == VivaldiStartPageLayoutStatePreviewModalFull ||
      [self isSmallModalPreview];
}

- (BOOL)isTopSites {
  return self.layoutState == VivaldiStartPageLayoutStateNormalTopSites;
}

/// Returns the number of calculate actual columns
- (NSInteger)numberOfRenderedColumns {
  CGFloat availableWidth =
  self.collectionView.bounds.size.width -
  self.sectionInset.left - self.sectionInset.right;
  CGFloat itemWidth = self.itemSize.width + self.minimumInteritemSpacing;
  return MAX((availableWidth + self.minimumInteritemSpacing) / itemWidth, 1);
}

/// Returns the minimum column width for iPads.
- (CGFloat)iPadColumnWidth {
  if ([self isPreview]) {
    return [self iPadColumnWidthForPreview];
  }

  switch (self.layoutStyle) {
    case VivaldiStartPageLayoutStyleSmall:
      return [self isTopSites] ?
          kSmallStyleWidthPadTopSites : kSmallStyleWidthPad;
    case VivaldiStartPageLayoutStyleMedium:
      return kMediumStyleWidthPad;
    case VivaldiStartPageLayoutStyleLarge:
      return kLargeStyleWidthPad;
    case VivaldiStartPageLayoutStyleList:
      return kListStyleWidth;
  }
}

/// Returns the minimum column width for preview only for iPads.
- (CGFloat)iPadColumnWidthForPreview {
  switch (self.layoutStyle) {
    case VivaldiStartPageLayoutStyleSmall:
      return [self isSmallModalPreview] ?
          kSmallStyleWidthPadPreview : kSmallStyleWidthPadPreviewFull;
    case VivaldiStartPageLayoutStyleMedium:
      return [self isSmallModalPreview] ?
          kMediumStyleWidthPadPreview : kMediumStyleWidthPadPreviewFull;
    case VivaldiStartPageLayoutStyleLarge:
      return [self isSmallModalPreview] ?
          kLargeStyleWidthPadPreview : kLargeStyleWidthPadPreviewFull;
    case VivaldiStartPageLayoutStyleList:
      return kListStyleWidthPadPreview;
  }
}

/// Returns the minimum column width for iPhone.
- (CGFloat)iPhoneColumnWidth {
  switch (self.layoutStyle) {
    case VivaldiStartPageLayoutStyleSmall:
      return [self isPreview] ?
          kSmallStyleWidthPhonePreview :
            ([self isTopSites] ?
                kSmallStyleWidthPhoneTopSites : kSmallStyleWidthPhone);
    case VivaldiStartPageLayoutStyleMedium:
      return [self isPreview] ?
          kMediumStyleWidthPhonePreview : kMediumStyleWidthPhone;
    case VivaldiStartPageLayoutStyleLarge:
      return [self isPreview] ?
          kLargeStyleWidthPhonePreview: kLargeStyleWidthPhone;
    case VivaldiStartPageLayoutStyleList:
      return [self isPreview] ?
          kListStyleWidthPhonePreview : kListStyleWidth;
  }
}

/// Returns top inset for the Collection View.
- (CGFloat)kTopInset {
  return self.topToolbarHidden ? kTopInsetForHiddenToolbar : kSpacing;
}

@end
