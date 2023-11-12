// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_PANEL_PANEL_BUTTON_H_
#define IOS_PANEL_PANEL_BUTTON_H_

#import <UIKit/UIKit.h>

// PROTOCOL
@protocol PanelButtonViewDelegate
- (void)didSelectIndex:(NSInteger)index;
@end

// A view to hold the top menu items of the start page.
@interface PanelButtonView : UIView

- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// DELEGATE
@property (nonatomic, weak) id<PanelButtonViewDelegate> delegate;

// SETTERS
- (void)selectItemWithIndex:(NSInteger)index;

@end

#endif  // IOS_PANEL_PANEL_BUTTON__H_
