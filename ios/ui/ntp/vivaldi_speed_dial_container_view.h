// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

#import "components/direct_match/direct_match_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/ui/ntp/vivaldi_speed_dial_container_delegate.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_column.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"

// A view to hold the top menu items of the start page.
@interface VivaldiSpeedDialContainerView : UIView

// INITIALIZERS
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// DELEGATE
@property (nonatomic, weak) id<VivaldiSpeedDialContainerDelegate> delegate;

// SETTERS
- (void)configureActionFactory:(BrowserActionFactory*)actionFactory;

- (void)configureWith:(NSArray*)speedDials
               parent:(VivaldiSpeedDialItem*)parent
        faviconLoader:(FaviconLoader*)faviconLoader
   directMatchService:(direct_match::DirectMatchService*)directMatchService
          layoutStyle:(VivaldiStartPageLayoutStyle)style
         layoutColumn:(VivaldiStartPageLayoutColumn)column
         showAddGroup:(BOOL)showAddGroup
    frequentlyVisited:(BOOL)frequentlyVisited
    topSitesAvailable:(BOOL)topSitesAvailable
     topToolbarHidden:(BOOL)topToolbarHidden
    verticalSizeClass:(UIUserInterfaceSizeClass)verticalSizeClass;

- (void)reloadLayoutWithStyle:(VivaldiStartPageLayoutStyle)style
                 layoutColumn:(VivaldiStartPageLayoutColumn)column;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_VIEW_H_
