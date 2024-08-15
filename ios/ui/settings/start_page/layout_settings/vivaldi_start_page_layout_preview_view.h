// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_PREVIEW_VIEW_H_
#define IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_PREVIEW_VIEW_H_

#import "UIKit/UIKit.h"

#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_column.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_state.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"

typedef void(^LayoutHeightUpdatedBlock)(CGFloat height);

@interface VivaldiStartPageLayoutPreviewView : UIView {
  // Instance variable for storing numberOfRows for preview
  NSInteger _numberOfRows;
}

// Custom initializer that takes numberOfRows as a parameter
- (instancetype)initWithNumberOfRows:(NSInteger)rows;

// SETTERS
- (void)reloadLayoutWithStyle:(VivaldiStartPageLayoutStyle)style
                  layoutState:(VivaldiStartPageLayoutState)state
                 layoutColumn:(VivaldiStartPageLayoutColumn)column;
/// Reloads the layout with column, state, style and returns
/// the height needed for drawing the numberOfColumns
/// in the callback.
- (void)reloadLayoutWithStyle:(VivaldiStartPageLayoutStyle)style
                  layoutState:(VivaldiStartPageLayoutState)state
                 layoutColumn:(VivaldiStartPageLayoutColumn)column
            heightUpdateBlock:(LayoutHeightUpdatedBlock)callback;
@end

#endif  // IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_PREVIEW_VIEW_H_
