// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_MODAL_PAGE_MODAL_PAGE_VIEW_CONTROLLER_H_
#define IOS_UI_MODAL_PAGE_MODAL_PAGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ModalPageCommands;

// View controller used to display a modal webpage.
@interface ModalPageViewController : UIViewController

// Initiates a ModalPageViewController with
// `ModalPageView` UIView with a webpage page in it;
// `handler` to handle user action.
- (instancetype)initWithContentView:(UIView*)ModalPageView
                            handler:(id<ModalPageCommands>)handler
                              title:(NSString*)title;

// Closes the page.
- (void)close;

@end

#endif  // IOS_UI_MODAL_PAGE_MODAL_PAGE_VIEW_CONTROLLER_H_
