// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOM_SETTINGS_CONSUMER_H_
#define IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOM_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

// A protocol implemented by consumers for page zoom settings
// preference state change.
@protocol VivaldiPageZoomSettingsConsumer
- (void)setPreferenceForPageZoomLevel:(NSInteger)level;
- (void)setPreferenceForGlobalPageZoomEnabled:(BOOL)enabled;
@end

#endif  // IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOM_SETTINGS_CONSUMER_H_
