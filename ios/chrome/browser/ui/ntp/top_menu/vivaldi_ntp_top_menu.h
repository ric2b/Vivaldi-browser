// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_VIVALDI_NTP_TOP_MENU_H_
#define IOS_CHROME_BROWSER_UI_VIVALDI_NTP_TOP_MENU_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_item.h"

// PROTOCOL
@protocol VivaldiNTPTopMenuViewDelegate
- (void)didSelectItem:(VivaldiSpeedDialItem*)item
                index:(NSInteger)index;
@end

// A view to hold the top menu items of the start page.
@interface VivaldiNTPTopMenuView : UIView

- (instancetype)initWithHeight:(CGFloat)height NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// DELEGATE
@property (nonatomic, weak) id<VivaldiNTPTopMenuViewDelegate> delegate;

// SETTERS
- (void)setMenuItemsWithSDFolders:(NSArray*)folders
                  selectedIndex:(NSInteger)selectedIndex;
- (void)selectItemWithIndex:(NSInteger)index
                   animated:(BOOL)animated;
- (void)invalidateLayoutWithHeight:(CGFloat)height;
- (void)removeAllItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_VIVALDI_NTP_TOP_MENU_H_
