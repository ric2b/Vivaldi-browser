// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/start_page/vivaldi_start_page_layout_preview_view.h"

#import "UIKit/UIKit.h"

#import "ios/chrome/browser/ui/ntp/cells/vivaldi_speed_dial_list_cell.h"
#import "ios/chrome/browser/ui/ntp/cells/vivaldi_speed_dial_regular_cell.h"
#import "ios/chrome/browser/ui/ntp/cells/vivaldi_speed_dial_small_cell.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_layout_style.h"
#import "ui/base/device_form_factor.h"

using ui::GetDeviceFormFactor;
using ui::DEVICE_FORM_FACTOR_TABLET;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// NAMESPACE
namespace {
// Cell Identifier for the cell types
NSString* cellIdRegular = @"cellIdRegular";
NSString* cellIdSmall = @"cellIdSmall";
NSString* cellIdList = @"cellIdList";

// The number of preview items for each layout and type of device.
const int numberOfItemsLarge = 2;
const int numberOfItemsLargeLandscape = 3;
const int numberOfItemsLargeiPad = 4;
const int numberOfItemsMedium = 3;
const int numberOfItemsMediumLandscape = 5;
const int numberOfItemsMediumiPad = 6;
const int numberOfItemsSmall = 4;
const int numberOfItemsSmallLandscape = 7;
const int numberOfItemsSmalliPad = 8;
const int numberOfItemsList = 4;
const int numberOfItemsListLandscape = 4;
const int numberOfItemsListiPad = 4;
}

@interface VivaldiStartPageLayoutPreviewView()<UICollectionViewDataSource>
@property(weak,nonatomic) UICollectionView *collectionView;
// A BOOL to keep track the device orientation
@property(assign,nonatomic) BOOL isDeviceLandscape;
// Currently selected layout
@property(nonatomic,assign) VivaldiStartPageLayoutStyle selectedLayout;
@end

@implementation VivaldiStartPageLayoutPreviewView

@synthesize collectionView = _collectionView;
@synthesize isDeviceLandscape = _isDeviceLandscape;
@synthesize selectedLayout = _selectedLayout;

#pragma mark - INITIALIZER
- (instancetype)init {
  if (self = [super initWithFrame:CGRectZero]) {
    self.backgroundColor = UIColor.clearColor;
    [self setUpUI];
  }
  return self;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {

  UICollectionViewLayout *layout= [self createLayout];
  UICollectionView* collectionView =
    [[UICollectionView alloc] initWithFrame:CGRectZero
                       collectionViewLayout:layout];
  _collectionView = collectionView;
  collectionView.dataSource = self;
  collectionView.showsHorizontalScrollIndicator = NO;
  collectionView.showsVerticalScrollIndicator = NO;
  collectionView.contentInsetAdjustmentBehavior =
    UIScrollViewContentInsetAdjustmentNever;

  [collectionView registerClass:[VivaldiSpeedDialRegularCell class]
      forCellWithReuseIdentifier:cellIdRegular];
  [collectionView registerClass:[VivaldiSpeedDialSmallCell class]
      forCellWithReuseIdentifier:cellIdSmall];
  [collectionView registerClass:[VivaldiSpeedDialListCell class]
      forCellWithReuseIdentifier:cellIdList];
  [collectionView setBackgroundColor:[UIColor clearColor]];

  [self addSubview:_collectionView];
  [_collectionView fillSuperview];
}

#pragma mark - SETTERS
- (void)reloadLayoutWithStyle:(VivaldiStartPageLayoutStyle)style
                  isLandscape:(BOOL)isLandscape {
  self.selectedLayout = style;
  self.isDeviceLandscape = isLandscape;
  [self.collectionView reloadData];
}

#pragma mark - COLLECTIONVIEW DATA SOURCE

- (NSInteger)collectionView:(UICollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return [self numberOfItemsInSection];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {

  switch (_selectedLayout) {
    case VivaldiStartPageLayoutStyleLarge:
    case VivaldiStartPageLayoutStyleMedium: {
      VivaldiSpeedDialRegularCell *largeCell =
        [collectionView dequeueReusableCellWithReuseIdentifier:cellIdRegular
                                                  forIndexPath:indexPath];
      [largeCell configurePreview];
      return largeCell;
    }
    case VivaldiStartPageLayoutStyleSmall: {
      VivaldiSpeedDialSmallCell *smallCell =
        [collectionView dequeueReusableCellWithReuseIdentifier:cellIdSmall
                                                  forIndexPath:indexPath];
      [smallCell configurePreviewForDevice: self.isCurrentDeviceTablet];
      return smallCell;
    }
    case VivaldiStartPageLayoutStyleList: {
        VivaldiSpeedDialListCell *listCell =
          [collectionView dequeueReusableCellWithReuseIdentifier:cellIdList
                                                    forIndexPath:indexPath];
        [listCell configurePreview];
        return listCell;
    }
  }
}

#pragma mark - SET UP UI COMPONENTS

/// Create and return the comositional layout for the collection view
- (UICollectionViewCompositionalLayout*)createLayout {
  UICollectionViewCompositionalLayout *layout =
    [[UICollectionViewCompositionalLayout alloc]
      initWithSectionProvider:
       ^NSCollectionLayoutSection*(NSInteger sectionIndex,
       id<NSCollectionLayoutEnvironment> layoutEnvironment) {
    return [self layoutSectionFor:sectionIndex environment:layoutEnvironment];
  }];

  return layout;
}

- (NSCollectionLayoutSection*)layoutSectionFor:(NSInteger)index
     environment:(id<NSCollectionLayoutEnvironment>)environment {

  CGFloat gridItemSize = [self itemSizeWidth];
  CGFloat sectionPadding = [self getSectionPadding];
  CGFloat itemPadding = [self getItemPadding];

  NSCollectionLayoutSize *itemSize;
  if (_selectedLayout == VivaldiStartPageLayoutStyleList) {
    itemSize =
      [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                  fractionalWidthDimension:gridItemSize]
               heightDimension:[NSCollectionLayoutDimension
                                  absoluteDimension:vSDItemHeightListLayout]];
  } else {
    itemSize =
      [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                  fractionalWidthDimension:gridItemSize]
               heightDimension:[NSCollectionLayoutDimension
                                  fractionalWidthDimension:gridItemSize]];
  }

  NSCollectionLayoutItem *item =
    [NSCollectionLayoutItem itemWithLayoutSize:itemSize];
  NSArray *items = [[NSArray alloc] initWithObjects:item, nil];

  item.contentInsets = NSDirectionalEdgeInsetsMake(itemPadding,
                                                   itemPadding,
                                                   itemPadding,
                                                   itemPadding);

  NSCollectionLayoutSize *groupSize;
  if (_selectedLayout == VivaldiStartPageLayoutStyleList) {
    groupSize =
      [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                  fractionalWidthDimension:1.0]
               heightDimension:[NSCollectionLayoutDimension
                                  absoluteDimension:vSDItemHeightListLayout]];
  } else {
    groupSize =
      [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                  fractionalWidthDimension:1.0]
               heightDimension:[NSCollectionLayoutDimension
                                  fractionalWidthDimension:gridItemSize]];
  }

  NSCollectionLayoutGroup *group =
    [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                                  subitems:items];

  NSCollectionLayoutSection *section =
    [NSCollectionLayoutSection sectionWithGroup:group];
  section.contentInsets = NSDirectionalEdgeInsetsMake(0,
                                                      sectionPadding,
                                                      0,
                                                      sectionPadding);
  return section;
}

/// Returns whether current device is iPhone or iPad.
- (BOOL)isCurrentDeviceTablet {
  return GetDeviceFormFactor() == DEVICE_FORM_FACTOR_TABLET;
}

// Returns the multiplier to generate the grid item from view width.
- (CGFloat)itemSizeWidth {
  switch (_selectedLayout) {
    case VivaldiStartPageLayoutStyleLarge:
      if (self.isCurrentDeviceTablet) {
        return vSDWidthiPadLarge;
      } else {
        if (self.isDeviceLandscape) {
          return vSDWidthiPhoneLargeLand;
        } else {
          return vSDWidthiPhoneLarge;
        }
      }
    case VivaldiStartPageLayoutStyleMedium:
      if (self.isCurrentDeviceTablet) {
        return vSDWidthiPadMedium;
      } else {
        if (self.isDeviceLandscape) {
          return vSDWidthiPhoneMediumLand;
        } else {
          return vSDWidthiPhoneMedium;
        }
      }
    case VivaldiStartPageLayoutStyleSmall:
      if (self.isCurrentDeviceTablet) {
        return vSDWidthiPadSmall;
      } else {
        if (self.isDeviceLandscape) {
          return vSDWidthiPhoneSmallLand;
        } else {
          return vSDWidthiPhoneSmall;
        }
      }
    case VivaldiStartPageLayoutStyleList:
      if (self.isCurrentDeviceTablet) {
        return vSDWidthiPadList;
      } else {
        if (self.isDeviceLandscape) {
          return vSDWidthiPhoneListLand;
        } else {
          return vSDWidthiPhoneList;
        }
      }
  }
}

/// Return the item padding. For preview we will use the padding for iPhone on
/// the iPad too since for iPad the settings page is a popup and doesn't have
/// the actual height and width of the start page.
- (CGFloat)getItemPadding {
  return vSDPaddingiPhone;
}

/// Returns the section padding.
- (CGFloat)getSectionPadding {
  return self.isDeviceLandscape ?
    vSDSectionPaddingiPhoneLandscape : vSDSectionPaddingiPhonePortrait;
}

/// Returns the number of items to render on preview based on device trait, type
/// and selected layout.
- (NSInteger)numberOfItemsInSection {
  if (self.isCurrentDeviceTablet) {
    switch (_selectedLayout) {
      case VivaldiStartPageLayoutStyleLarge:
        return numberOfItemsLargeiPad;
      case VivaldiStartPageLayoutStyleMedium:
        return numberOfItemsMediumiPad;
      case VivaldiStartPageLayoutStyleSmall:
        return numberOfItemsSmalliPad;
      case VivaldiStartPageLayoutStyleList:
        return numberOfItemsListiPad;
    }
  } else {
    switch (_selectedLayout) {
      case VivaldiStartPageLayoutStyleLarge:
        return _isDeviceLandscape ?
          numberOfItemsLargeLandscape :
          numberOfItemsLarge;
      case VivaldiStartPageLayoutStyleMedium:
        return _isDeviceLandscape ?
          numberOfItemsMediumLandscape :
          numberOfItemsMedium;
      case VivaldiStartPageLayoutStyleSmall:
        return _isDeviceLandscape ?
          numberOfItemsSmallLandscape :
          numberOfItemsSmall;
      case VivaldiStartPageLayoutStyleList:
        return _isDeviceLandscape ?
          numberOfItemsListLandscape :
          numberOfItemsList;
    }
  }
}

@end
