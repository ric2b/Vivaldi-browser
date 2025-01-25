// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_PAGEZOOM_DIALOG_VIVALDI_PAGEZOOM_DIALOG_CONSUMER_H_
#define IOS_UI_SETTINGS_PAGEZOOM_DIALOG_VIVALDI_PAGEZOOM_DIALOG_CONSUMER_H_

#import <UIKit/UIKit.h>

// A protocol implemented by consumers to page zoom Dialog
@protocol VivaldiPageZoomDialogConsumer <NSObject>

// Tells the consumer that the user can currently zoom in.
- (void)setZoomInEnabled:(BOOL)enabled;
// Tells the consumer that the user can currently zoom out.
- (void)setZoomOutEnabled:(BOOL)enabled;
// Tells the consumer that the user can currently reset the zoom level.
- (void)setResetZoomEnabled:(BOOL)enabled;
// Tells the consumer that the current zoom level.
- (void)setCurrentZoomLevel:(int)zoomLevel;
// Tells the consumer the current host
- (void)setCurrentHostURL:(NSString*)host;
// Tells the consumer the current host's favicon image
- (void)setCurrentHostFavicon:(UIImage*)image;
// Global Setting
-  (void)setGlobalPageZoom:(BOOL)enabled;

@end

#endif  // IOS_UI_SETTINGS_PAGEZOOM_DIALOG_VIVALDI_PAGEZOOM_DIALOG_CONSUMER_H_
