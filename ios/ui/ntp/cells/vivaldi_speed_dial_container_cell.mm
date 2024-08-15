// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/cells/vivaldi_speed_dial_container_cell.h"

#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_style_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"

@interface VivaldiSpeedDialContainerCell()<VivaldiSpeedDialContainerDelegate>
// The view whole the children of the speed dial folder
@property(nonatomic,weak) VivaldiSpeedDialContainerView* speedDialView;
// Currently visible parent index
@property(nonatomic,assign) NSInteger currentPage;
@end

@implementation VivaldiSpeedDialContainerCell

@synthesize speedDialView = _speedDialView;
@synthesize currentPage = _currentPage;

#pragma mark - INITIALIZER
- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setUpUI];
  }
  return self;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {
  VivaldiSpeedDialContainerView *speedDialView =
    [VivaldiSpeedDialContainerView new];
  _speedDialView = speedDialView;
  [self addSubview:speedDialView];
  [speedDialView fillSuperview];
  speedDialView.delegate = self;
}

#pragma mark - SETTERS
- (void)configureWith:(NSArray*)speedDials
               parent:(VivaldiSpeedDialItem*)parent
        faviconLoader:(FaviconLoader*)faviconLoader
          layoutStyle:(VivaldiStartPageLayoutStyle)style
         layoutColumn:(VivaldiStartPageLayoutColumn)column
         showAddGroup:(BOOL)showAddGroup
    verticalSizeClass:(UIUserInterfaceSizeClass)verticalSizeClass {
  [self.speedDialView configureWith:speedDials
                             parent:parent
                      faviconLoader:faviconLoader
                        layoutStyle:style
                       layoutColumn:column
                       showAddGroup:showAddGroup
                  verticalSizeClass:verticalSizeClass];
}

- (void)reloadLayoutWithStyle:(VivaldiStartPageLayoutStyle)style
                 layoutColumn:(VivaldiStartPageLayoutColumn)column {
  [self.speedDialView reloadLayoutWithStyle:style
                               layoutColumn:column];
}

- (void)setCurrentPage:(NSInteger)page {
  _currentPage = page;
}

#pragma mark - VIVALDI_SPEED_DIAL_CONTAINER_VIEW_DELEGATE
- (void)didSelectItem:(VivaldiSpeedDialItem*)item
               parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectItem:item parent:parent];
}

- (void)didSelectEditItem:(VivaldiSpeedDialItem*)item
                   parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectEditItem:item parent:parent];
}

- (void)didSelectMoveItem:(VivaldiSpeedDialItem*)item
                   parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectMoveItem:item parent:parent];
}

- (void)didMoveItemByDragging:(VivaldiSpeedDialItem*)item
                       parent:(VivaldiSpeedDialItem*)parent
                   toPosition:(NSInteger)position {
  if (self.delegate)
    [self.delegate didMoveItemByDragging:item
                                  parent:parent
                              toPosition:position];
}

- (void)didSelectDeleteItem:(VivaldiSpeedDialItem*)item
                     parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectDeleteItem:item parent:parent];
}

- (void)didRefreshThumbnailForItem:(VivaldiSpeedDialItem*)item
                            parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didRefreshThumbnailForItem:item parent:parent];
}

- (void)didSelectAddNewSpeedDial:(bool)isFolder
                          parent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectAddNewSpeedDial:isFolder parent:parent];
}

- (void)didSelectAddNewGroupForParent:(VivaldiSpeedDialItem*)parent {
  if (self.delegate)
    [self.delegate didSelectAddNewGroupForParent:parent];
}

@end
