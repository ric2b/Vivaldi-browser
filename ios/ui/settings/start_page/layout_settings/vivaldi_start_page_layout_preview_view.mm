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

// A constant high enough to render 2 or more rows of preview.
// However, preview will always show rows depending on numberOfRows value passed
// on init regardless of numberOfItems rendered.
const CGFloat numberOfItems = 30;
}

@interface VivaldiStartPageLayoutPreviewView()<UICollectionViewDataSource>
@property(weak,nonatomic) UICollectionView *collectionView;
// Collection view layout for selected column and layout rendering
@property(nonatomic,strong) VivaldiSpeedDialViewContainerViewFlowLayout *layout;
// Currently selected layout
@property(nonatomic,assign) VivaldiStartPageLayoutStyle selectedLayout;
// Currently selected layout state
@property(nonatomic,assign) VivaldiStartPageLayoutState selectedLayoutState;
// Currently selected maximum columns
@property(nonatomic,assign) VivaldiStartPageLayoutColumn selectedColumn;
// Collection view height constraint
@property(nonatomic, strong) NSLayoutConstraint* cvHeightConstraint;
@end

@implementation VivaldiStartPageLayoutPreviewView

@synthesize collectionView = _collectionView;
@synthesize selectedLayout = _selectedLayout;
@synthesize selectedLayoutState = _selectedLayoutState;
@synthesize selectedColumn = _selectedColumn;

#pragma mark - INITIALIZER
- (instancetype)initWithNumberOfRows:(NSInteger)rows {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _numberOfRows = rows;
    self.backgroundColor = UIColor.clearColor;
    [self setUpUI];
  }
  return self;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {

  VivaldiSpeedDialViewContainerViewFlowLayout *layout =
      [VivaldiSpeedDialViewContainerViewFlowLayout new];
  layout.shouldShowTabletLayout = [self showTabletLayout];
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
  [_collectionView anchorTop:self.topAnchor
                     leading:self.leadingAnchor
                      bottom:nil
                    trailing:self.trailingAnchor];

  self.cvHeightConstraint =
      [self.collectionView.heightAnchor constraintEqualToConstant:0];
  self.cvHeightConstraint.active = YES;
}

#pragma mark - SETTERS
- (void)reloadLayoutWithStyle:(VivaldiStartPageLayoutStyle)style
                  layoutState:(VivaldiStartPageLayoutState)state
                 layoutColumn:(VivaldiStartPageLayoutColumn)column {
  [self reloadLayoutWithStyle:style
                  layoutState:state
                 layoutColumn:column
            heightUpdateBlock:nil];
}

- (void)reloadLayoutWithStyle:(VivaldiStartPageLayoutStyle)style
                  layoutState:(VivaldiStartPageLayoutState)state
                 layoutColumn:(VivaldiStartPageLayoutColumn)column
            heightUpdateBlock:(LayoutHeightUpdatedBlock)callback {
  self.selectedLayout = style;
  self.selectedLayoutState = state;
  self.selectedColumn = column;
  self.layout.layoutStyle = style;
  self.layout.layoutState = state;
  self.layout.shouldShowTabletLayout = [self showTabletLayout];
  self.layout.numberOfColumns = column;
  self.layout.maxNumberOfRows = _numberOfRows;

  // Force layout reload so that we get final height in the callback.
  [self.layout prepareLayout];
  [self.collectionView.collectionViewLayout invalidateLayout];
  [self.collectionView reloadData];

  self.cvHeightConstraint.constant = [self.layout heightNeededForContents];
  [self.layout adjustCollectionViewHeight];

  if (callback) {
    callback([self.layout heightNeededForContents]);
  }
}

#pragma mark - COLLECTIONVIEW DATA SOURCE

- (NSInteger)collectionView:(UICollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return numberOfItems;
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
      [smallCell configurePreviewForDevice: self.showTabletLayout];
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

/// Returns whether items should have tablet or phone layout.
- (BOOL)showTabletLayout {
  return [VivaldiGlobalHelpers canShowSidePanelForTrait:self.traitCollection];
}

@end
