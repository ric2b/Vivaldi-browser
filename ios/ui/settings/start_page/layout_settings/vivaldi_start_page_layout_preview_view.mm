// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_preview_view.h"

#import "UIKit/UIKit.h"

#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/cells/vivaldi_speed_dial_list_cell.h"
#import "ios/ui/ntp/cells/vivaldi_speed_dial_regular_cell.h"
#import "ios/ui/ntp/cells/vivaldi_speed_dial_small_cell.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_container_view_flow_layout.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"
#import "ui/base/device_form_factor.h"

using ui::GetDeviceFormFactor;
using ui::DEVICE_FORM_FACTOR_TABLET;

// NAMESPACE
namespace {
// Cell Identifier for the cell types
NSString* cellIdRegular = @"cellIdRegular";
NSString* cellIdSmall = @"cellIdSmall";
NSString* cellIdList = @"cellIdList";
}

@interface VivaldiStartPageLayoutPreviewView()<UICollectionViewDataSource>
@property(weak,nonatomic) UICollectionView *collectionView;
// Collection view layout for selected column and layout rendering
@property(nonatomic,strong) VivaldiSpeedDialViewContainerViewFlowLayout *layout;
// Currently selected layout
@property(nonatomic,assign) VivaldiStartPageLayoutStyle selectedLayout;
// Currently selected maximum columns
@property(nonatomic,assign) VivaldiStartPageLayoutColumn selectedColumn;
@end

@implementation VivaldiStartPageLayoutPreviewView

@synthesize collectionView = _collectionView;
@synthesize selectedLayout = _selectedLayout;
@synthesize selectedColumn = _selectedColumn;

#pragma mark - INITIALIZER
- (instancetype)initWithItemConfig:(PreviewItemConfig)itemConfig {
  if (self = [super initWithFrame:CGRectZero]) {
    _itemConfig = itemConfig;
    self.backgroundColor = UIColor.clearColor;
    [self setUpUI];
  }
  return self;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {

  VivaldiSpeedDialViewContainerViewFlowLayout *layout =
      [VivaldiSpeedDialViewContainerViewFlowLayout new];
  layout.isPreview = YES;
  self.layout = layout;
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
                 layoutColumn:(VivaldiStartPageLayoutColumn)column {
  self.selectedLayout = style;
  self.selectedColumn = column;
  self.layout.layoutStyle = style;
  self.layout.numberOfColumns = column;
  [self.collectionView.collectionViewLayout invalidateLayout];
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

/// Returns the number of items to render on preview based on device trait, type
/// and selected layout.
- (NSInteger)numberOfItemsInSection {
  if (self.isCurrentDeviceTablet) {
    switch (_selectedLayout) {
      case VivaldiStartPageLayoutStyleLarge:
        return _itemConfig.numberOfItemsLargeiPad;
      case VivaldiStartPageLayoutStyleMedium:
        return _itemConfig.numberOfItemsMediumiPad;
      case VivaldiStartPageLayoutStyleSmall:
        return _itemConfig.numberOfItemsSmalliPad;
      case VivaldiStartPageLayoutStyleList:
        return _itemConfig.numberOfItemsListiPad;
    }
  } else {
    switch (_selectedLayout) {
      case VivaldiStartPageLayoutStyleLarge:
        return _itemConfig.numberOfItemsLarge;
      case VivaldiStartPageLayoutStyleMedium:
        return _itemConfig.numberOfItemsMedium;
      case VivaldiStartPageLayoutStyleSmall:
        return _itemConfig.numberOfItemsSmall;
      case VivaldiStartPageLayoutStyleList:
        return _itemConfig.numberOfItemsList;
    }
  }
}

/// Returns whether current device is iPhone or iPad.
- (BOOL)isCurrentDeviceTablet {
  return GetDeviceFormFactor() == DEVICE_FORM_FACTOR_TABLET &&
  VivaldiGlobalHelpers.isHorizontalTraitRegular;
}

@end
